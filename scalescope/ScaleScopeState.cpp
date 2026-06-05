#include "ScaleScopeState.h"

#include <cmath>
#include <limits>

namespace
{
using ScaleTemplate = AggregaScaleState::ScaleTemplate;

constexpr std::array<ScaleTemplate, 11> scaleTemplates
{{
    { "Major",            { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 } },
    { "Natural Minor",    { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 } },
    { "Harmonic Minor",   { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1 } },
    { "Melodic Minor",    { 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1 } },
    { "Dorian",           { 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0 } },
    { "Phrygian",         { 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0 } },
    { "Lydian",           { 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1 } },
    { "Mixolydian",       { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0 } },
    { "Locrian",          { 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0 } },
    { "Major Pentatonic", { 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0 } },
    { "Minor Pentatonic", { 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0 } }
}};

double estimateRms(const float* samples, int startSample, int length)
{
    if (length <= 0)
        return 0.0;

    double sumSquares = 0.0;
    for (int i = 0; i < length; ++i)
    {
        const auto sample = static_cast<double>(samples[startSample + i]);
        sumSquares += sample * sample;
    }

    return std::sqrt(sumSquares / static_cast<double>(length));
}

float estimatePitchHz(const float* samples, int startSample, int length, double sampleRate)
{
    constexpr double minFrequency = 50.0;
    constexpr double maxFrequency = 1400.0;

    const auto minLag = juce::jmax(2, juce::roundToInt(sampleRate / maxFrequency));
    const auto maxLag = juce::jmin(length - 2, juce::roundToInt(sampleRate / minFrequency));
    if (maxLag <= minLag)
        return 0.0f;

    auto scoreForLag = [&](int lag)
    {
        double correlation = 0.0;
        double energyA = 0.0;
        double energyB = 0.0;

        for (int i = 0; i < length - lag; ++i)
        {
            const auto a = static_cast<double>(samples[startSample + i]);
            const auto b = static_cast<double>(samples[startSample + i + lag]);
            correlation += a * b;
            energyA += a * a;
            energyB += b * b;
        }

        const auto normaliser = std::sqrt(energyA * energyB);
        return normaliser > 0.0 ? correlation / normaliser : 0.0;
    };

    double bestScore = 0.0;
    int bestLag = 0;
    for (int lag = minLag; lag <= maxLag; lag += (lag < 128 ? 1 : 2))
    {
        const auto score = scoreForLag(lag);
        if (score > bestScore)
        {
            bestScore = score;
            bestLag = lag;
        }
    }

    if (bestLag == 0 || bestScore < 0.68)
        return 0.0f;

    return static_cast<float>(sampleRate / static_cast<double>(bestLag));
}

struct DetectionResult
{
    int rootNote = 0;
    juce::String scaleName { "Major" };
    std::array<bool, 12> enabledPitchClasses {};
    juce::String detailText;
    bool valid = false;
};

DetectionResult detectScaleFromAudio(const juce::AudioBuffer<float>& source, double sampleRate, std::atomic<float>& progress)
{
    DetectionResult result;

    if (source.getNumSamples() < 4096 || sampleRate <= 0.0)
    {
        result.detailText = "The file was too short for reliable scale detection.";
        return result;
    }

    juce::AudioBuffer<float> mono(1, source.getNumSamples());
    mono.copyFrom(0, 0, source, 0, 0, source.getNumSamples());
    if (source.getNumChannels() > 1)
        mono.addFrom(0, 0, source, 1, 0, source.getNumSamples(), 1.0f);
    mono.applyGain(0.5f);

    auto* monoData = mono.getWritePointer(0);
    constexpr float cutoffHz = 1800.0f;
    const auto rc = 1.0f / (juce::MathConstants<float>::twoPi * cutoffHz);
    const auto dt = 1.0f / static_cast<float>(sampleRate);
    const auto alpha = dt / (rc + dt);
    float filtered = monoData[0];
    for (int i = 1; i < mono.getNumSamples(); ++i)
    {
        filtered += alpha * (monoData[i] - filtered);
        monoData[i] = filtered;
    }

    constexpr int frameSize = 4096;
    constexpr int hopSize = 2048;
    std::array<double, 12> pitchClassEnergy {};
    int stableFrames = 0;
    const auto* samples = mono.getReadPointer(0);
    const auto totalFrames = juce::jmax(1, (mono.getNumSamples() - frameSize) / hopSize);
    int frameIndex = 0;

    for (int startSample = 0; startSample + frameSize < mono.getNumSamples(); startSample += hopSize)
    {
        ++frameIndex;
        if ((frameIndex % 32) == 0)
            progress.store(0.30f + 0.55f * juce::jlimit(0.0f, 1.0f, static_cast<float>(frameIndex) / static_cast<float>(totalFrames)));

        const auto rms = estimateRms(samples, startSample, frameSize);
        if (rms < 0.015)
            continue;

        const auto pitchHz = estimatePitchHz(samples, startSample, frameSize, sampleRate);
        if (pitchHz <= 0.0f)
            continue;

        const auto midiNote = juce::roundToInt(69.0 + 12.0 * std::log2(static_cast<double>(pitchHz) / 440.0));
        const auto pitchClass = juce::negativeAwareModulo(midiNote, 12);
        pitchClassEnergy[static_cast<size_t>(pitchClass)] += rms;
        ++stableFrames;
    }

    if (stableFrames < 8)
    {
        result.detailText = "The file did not contain enough stable pitched material to infer a scale.";
        return result;
    }

    double totalEnergy = 0.0;
    for (const auto energy : pitchClassEnergy)
        totalEnergy += energy;

    double bestScore = -std::numeric_limits<double>::max();
    int bestRoot = 0;
    const ScaleTemplate* bestTemplate = nullptr;

    for (int root = 0; root < 12; ++root)
    {
        for (const auto& scaleTemplate : scaleTemplates)
        {
            auto mask = AggregaScaleState::buildMaskFromTemplate(root, scaleTemplate);
            double includedEnergy = 0.0;
            double excludedEnergy = 0.0;

            for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
            {
                if (mask[static_cast<size_t>(pitchClass)])
                    includedEnergy += pitchClassEnergy[static_cast<size_t>(pitchClass)];
                else
                    excludedEnergy += pitchClassEnergy[static_cast<size_t>(pitchClass)];
            }

            const auto tonicEnergy = pitchClassEnergy[static_cast<size_t>(root)];
            const auto score = includedEnergy - (excludedEnergy * 1.6) + tonicEnergy * 0.35;

            if (score > bestScore)
            {
                bestScore = score;
                bestRoot = root;
                bestTemplate = &scaleTemplate;
            }
        }
    }

    if (bestTemplate == nullptr || totalEnergy <= 0.0)
    {
        result.detailText = "Scale detection could not find a confident result.";
        return result;
    }

    const auto matchedMask = AggregaScaleState::buildMaskFromTemplate(bestRoot, *bestTemplate);
    double matchedEnergy = 0.0;
    juce::StringArray prominentNotes;

    for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
    {
        const auto energy = pitchClassEnergy[static_cast<size_t>(pitchClass)];
        if (matchedMask[static_cast<size_t>(pitchClass)])
            matchedEnergy += energy;

        if (energy >= totalEnergy * 0.09)
            prominentNotes.add(AggregaScaleState::pitchClassName(pitchClass));
    }

    result.rootNote = bestRoot;
    result.scaleName = bestTemplate->name;
    result.enabledPitchClasses = matchedMask;
    result.valid = true;
    result.detailText = "Detected " + AggregaScaleState::pitchClassName(bestRoot) + " " + bestTemplate->name
        + " from " + juce::String(stableFrames) + " stable frames. "
        + juce::String(juce::roundToInt((matchedEnergy / totalEnergy) * 100.0)) + "% of the pitched energy fit the scale. "
        + "Prominent notes: " + prominentNotes.joinIntoString(", ") + ".";
    return result;
}
} // namespace

AggregaScaleState::AggregaScaleState()
{
    formatManager.registerBasicFormats();
    applyPresetScale(0, "Major");
}

AggregaScaleState::~AggregaScaleState()
{
    joinLoaderThread();
}

AggregaScaleState::ScaleInfo AggregaScaleState::getScaleInfo() const
{
    const juce::ScopedLock lock(stateLock);
    return scaleInfo;
}

void AggregaScaleState::setScaleInfo(const ScaleInfo& newState)
{
    const juce::ScopedLock lock(stateLock);
    scaleInfo = newState;
}

std::array<bool, 12> AggregaScaleState::buildMaskFromTemplate(int rootNote, const ScaleTemplate& scaleTemplate)
{
    std::array<bool, 12> mask {};
    for (int i = 0; i < 12; ++i)
        if (scaleTemplate.intervals[static_cast<size_t>(i)] != 0)
            mask[static_cast<size_t>((rootNote + i) % 12)] = true;
    return mask;
}

const AggregaScaleState::ScaleTemplate* AggregaScaleState::findScaleTemplate(const juce::String& scaleName)
{
    for (const auto& scaleTemplate : scaleTemplates)
        if (scaleName.equalsIgnoreCase(scaleTemplate.name))
            return &scaleTemplate;

    return nullptr;
}

juce::String AggregaScaleState::pitchClassName(int pitchClass)
{
    static const std::array<const char*, 12> names { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return names[static_cast<size_t>(juce::negativeAwareModulo(pitchClass, 12))];
}

void AggregaScaleState::applyPresetScale(int rootNote, const juce::String& scaleName)
{
    if (const auto* scaleTemplate = findScaleTemplate(scaleName))
    {
        ScaleInfo newState;
        newState.rootNote = juce::negativeAwareModulo(rootNote, 12);
        newState.scaleName = scaleTemplate->name;
        newState.sourceLabel = "Manual preset";
        newState.detailText = "Showing " + pitchClassName(newState.rootNote) + " " + newState.scaleName + " across the keyboard.";
        newState.enabledPitchClasses = buildMaskFromTemplate(newState.rootNote, *scaleTemplate);
        newState.derivedFromAnalysis = false;
        setScaleInfo(newState);
    }
}

void AggregaScaleState::applyCustomScale(int rootNote, const std::array<bool, 12>& enabledPitchClasses, const juce::String& label)
{
    ScaleInfo newState;
    newState.rootNote = juce::negativeAwareModulo(rootNote, 12);
    newState.scaleName = label.isNotEmpty() ? label : "Custom";
    newState.sourceLabel = "Manual custom";
    newState.detailText = "Showing a custom set of notes rooted at " + pitchClassName(newState.rootNote) + ".";
    newState.enabledPitchClasses = enabledPitchClasses;
    newState.derivedFromAnalysis = false;
    setScaleInfo(newState);
}

void AggregaScaleState::joinLoaderThread()
{
    if (loaderThread.joinable())
        loaderThread.join();
}

bool AggregaScaleState::startLoadingSourceFile(const juce::File& file)
{
    if (loadingSource.exchange(true))
        return false;

    joinLoaderThread();
    loadingProgress.store(0.05f);

    loaderThread = std::thread([this, file]
    {
        performLoad(file);
    });

    return true;
}

void AggregaScaleState::performLoad(const juce::File& file)
{
    loadingProgress.store(0.10f);

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        auto failedState = getScaleInfo();
        failedState.sourceLabel = "Analysis failed";
        failedState.detailText = "Could not open " + file.getFileName() + ". Try a WAV, AIFF, or MP3.";
        setScaleInfo(failedState);
        loadingProgress.store(0.0f);
        loadingSource.store(false);
        return;
    }

    loadingProgress.store(0.20f);
    juce::AudioBuffer<float> source(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    reader->read(&source, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    loadingProgress.store(0.30f);
    auto detection = detectScaleFromAudio(source, reader->sampleRate, loadingProgress);

    auto newState = getScaleInfo();
    if (detection.valid)
    {
        newState.rootNote = detection.rootNote;
        newState.scaleName = detection.scaleName;
        newState.sourceLabel = "Detected from " + file.getFileName();
        newState.detailText = detection.detailText;
        newState.enabledPitchClasses = detection.enabledPitchClasses;
        newState.derivedFromAnalysis = true;
    }
    else
    {
        newState.sourceLabel = "Analysis failed";
        newState.detailText = detection.detailText + " File: " + file.getFileName();
        newState.derivedFromAnalysis = true;
    }

    setScaleInfo(newState);
    loadingProgress.store(1.0f);
    loadingSource.store(false);
}
