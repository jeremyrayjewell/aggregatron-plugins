#include "VocoderProcessor.h"
#include "VocoderEditor.h"

#include <cmath>
#include <limits>

namespace
{
using ScaleTemplate = AggregaVocoderAudioProcessor::ScaleTemplate;

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

constexpr std::array<int, 7> harmonyIntervals { -12, -7, -5, 5, 7, 12, 19 };

double estimateRms(const float* samples, int length)
{
    double sumSquares = 0.0;
    for (int i = 0; i < length; ++i)
    {
        const auto sample = static_cast<double>(samples[i]);
        sumSquares += sample * sample;
    }

    return std::sqrt(sumSquares / static_cast<double>(juce::jmax(1, length)));
}
} // namespace

AggregaVocoderAudioProcessor::AggregaVocoderAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout()),
      analysisBuffer(1, analysisWindowSize)
{
    updateScaleMask();
    updateBandFrequencies();
}

void AggregaVocoderAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
    analysisBuffer.clear();
    analysisWritePosition = 0;
    smoothedCarrierFrequency = 110.0f;
    detectedPitchHz.store(0.0f);
    targetMidiNote.store(60);
    std::fill(envelopes.begin(), envelopes.end(), 0.0f);
    leftCarrier = {};
    rightCarrier = {};
    updateBandFrequencies();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;

    for (auto& filter : modulatorFilters)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    }

    for (auto& filter : carrierFiltersLeft)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    }

    for (auto& filter : carrierFiltersRight)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    }
}

void AggregaVocoderAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AggregaVocoderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}
#endif

void AggregaVocoderAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    updateScaleMask();
    updateBandFrequencies();

    const auto modeIndex = juce::roundToInt(parameters.getRawParameterValue("mode")->load());
    const auto mix = parameters.getRawParameterValue("mix")->load();
    const auto outputGain = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("outputGain")->load());
    const auto tone = parameters.getRawParameterValue("tone")->load();
    const auto trackingMs = parameters.getRawParameterValue("tracking")->load();
    const auto correctionAmount = parameters.getRawParameterValue("correctionAmount")->load();
    const auto octaveShift = juce::roundToInt(parameters.getRawParameterValue("octaveShift")->load()) - 4;
    const auto harmonyMix = parameters.getRawParameterValue("harmonyMix")->load();
    const auto harmonyIndex = juce::roundToInt(parameters.getRawParameterValue("harmonyInterval")->load());
    const auto harmonySemitones = harmonyIntervals[static_cast<size_t>(juce::jlimit(0, static_cast<int>(harmonyIntervals.size()) - 1, harmonyIndex))];
    const auto robotMode = modeIndex == 1;

    const auto noiseMix = robotMode ? 0.06f : 0.015f;
    const auto filterQ = robotMode ? 2.0f : 1.15f;
    const auto envAttack = juce::jmap(trackingMs, 0.001f, 0.012f);
    const auto envRelease = juce::jmap(trackingMs, 0.03f, 0.12f);
    const auto attackCoeff = std::exp(-1.0f / static_cast<float>(juce::jmax(1.0, currentSampleRate * envAttack)));
    const auto releaseCoeff = std::exp(-1.0f / static_cast<float>(juce::jmax(1.0, currentSampleRate * envRelease)));

    auto* analysisData = analysisBuffer.getWritePointer(0);
    float detectedHzThisBlock = detectedPitchHz.load();
    int targetNoteThisBlock = targetMidiNote.load();
    juce::HeapBlock<float> contiguous(static_cast<size_t>(analysisWindowSize));

    for (size_t band = 0; band < numBands; ++band)
    {
        modulatorFilters[band].setCutoffFrequency(bandFrequencies[band]);
        modulatorFilters[band].setResonance(filterQ);
        carrierFiltersLeft[band].setCutoffFrequency(bandFrequencies[band]);
        carrierFiltersLeft[band].setResonance(filterQ);
        carrierFiltersRight[band].setCutoffFrequency(bandFrequencies[band]);
        carrierFiltersRight[band].setResonance(filterQ);
    }

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto inLeft = buffer.getSample(0, sample);
        const auto inRight = totalNumInputChannels > 1 ? buffer.getSample(1, sample) : inLeft;
        const auto monoIn = 0.5f * (inLeft + inRight);
        const auto modulator = std::tanh(monoIn * 4.0f);
        analysisData[analysisWritePosition] = modulator;
        analysisWritePosition = (analysisWritePosition + 1) % analysisWindowSize;

        if ((sample % 64) == 0)
        {
            for (int i = 0; i < analysisWindowSize; ++i)
                contiguous[i] = analysisData[(analysisWritePosition + i) % analysisWindowSize];

            const auto rms = estimateRms(contiguous.getData(), analysisWindowSize);
            if (rms > 0.008)
            {
                const auto estimatedHz = estimatePitchHz(contiguous.getData(), analysisWindowSize, currentSampleRate);
                if (estimatedHz > 0.0f)
                {
                    detectedHzThisBlock = estimatedHz;
                    detectedPitchHz.store(estimatedHz);
                    const auto detectedMidi = 69.0f + 12.0f * std::log2(estimatedHz / 440.0f);
                    auto snappedMidi = static_cast<float>(findNearestAllowedMidiNote(detectedMidi) + (octaveShift * 12));
                    snappedMidi = juce::jlimit(24.0f, 108.0f, snappedMidi);
                    const auto snappedHz = 440.0f * std::pow(2.0f, (snappedMidi - 69.0f) / 12.0f);
                    const auto correctedHz = juce::jmap(correctionAmount, estimatedHz, snappedHz);
                    smoothedCarrierFrequency += (correctedHz - smoothedCarrierFrequency) * (robotMode ? 0.45f : 0.12f);
                    targetNoteThisBlock = juce::roundToInt(snappedMidi);
                    targetMidiNote.store(targetNoteThisBlock);
                }
            }
        }

        const auto leftCarrierSample = renderCarrierSample(leftCarrier, smoothedCarrierFrequency, tone, noiseMix, harmonyMix, harmonySemitones);
        const auto rightCarrierSample = renderCarrierSample(rightCarrier, smoothedCarrierFrequency, tone, noiseMix, harmonyMix, harmonySemitones);

        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        float totalEnv = 0.0f;

        for (size_t band = 0; band < numBands; ++band)
        {
            const auto modBand = modulatorFilters[band].processSample(0, modulator);
            const auto envTarget = juce::jlimit(0.0f, 1.5f, std::abs(modBand) * 10.0f);
            const auto coeff = envTarget > envelopes[band] ? attackCoeff : releaseCoeff;
            envelopes[band] = (coeff * envelopes[band]) + ((1.0f - coeff) * envTarget);
            const auto shapedEnv = std::sqrt(juce::jlimit(0.0f, 1.0f, envelopes[band]));
            totalEnv += shapedEnv;

            wetLeft += carrierFiltersLeft[band].processSample(0, leftCarrierSample) * shapedEnv;
            wetRight += carrierFiltersRight[band].processSample(0, rightCarrierSample) * shapedEnv;
        }

        const auto envNorm = totalEnv > 0.001f ? (2.4f / totalEnv) : 0.0f;
        wetLeft *= envNorm;
        wetRight *= envNorm;

        const auto unvoiced = juce::jlimit(0.0f, 1.0f, (std::abs(modulator) - 0.04f) * 5.0f);
        wetLeft += modulator * unvoiced * 0.12f;
        wetRight += modulator * unvoiced * 0.12f;

        const auto wetGain = robotMode ? 4.8f : 3.8f;
        wetLeft = std::tanh(wetLeft * wetGain);
        wetRight = std::tanh(wetRight * wetGain);

        buffer.setSample(0, sample, juce::jlimit(-1.0f, 1.0f, ((inLeft * (1.0f - mix)) + (wetLeft * mix)) * outputGain));
        if (totalNumOutputChannels > 1)
            buffer.setSample(1, sample, juce::jlimit(-1.0f, 1.0f, ((inRight * (1.0f - mix)) + (wetRight * mix)) * outputGain));
    }
}

juce::AudioProcessorEditor* AggregaVocoderAudioProcessor::createEditor()
{
    return new AggregaVocoderAudioProcessorEditor(*this);
}

bool AggregaVocoderAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String AggregaVocoderAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AggregaVocoderAudioProcessor::acceptsMidi() const
{
    return false;
}

bool AggregaVocoderAudioProcessor::producesMidi() const
{
    return false;
}

bool AggregaVocoderAudioProcessor::isMidiEffect() const
{
    return false;
}

double AggregaVocoderAudioProcessor::getTailLengthSeconds() const
{
    return 0.2;
}

int AggregaVocoderAudioProcessor::getNumPrograms()
{
    return 1;
}

int AggregaVocoderAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AggregaVocoderAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String AggregaVocoderAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void AggregaVocoderAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void AggregaVocoderAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void AggregaVocoderAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));

    updateScaleMask();
    updateBandFrequencies();
}

juce::String AggregaVocoderAudioProcessor::pitchClassName(int pitchClass)
{
    static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[juce::jlimit(0, 11, pitchClass)];
}

const std::array<ScaleTemplate, 11>& AggregaVocoderAudioProcessor::getScaleTemplates()
{
    return scaleTemplates;
}

juce::AudioProcessorValueTreeState::ParameterLayout AggregaVocoderAudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using ChoiceParam = juce::AudioParameterChoice;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;
    layout.push_back(std::make_unique<ChoiceParam>("rootNote", "Root", juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

    juce::StringArray scaleNames;
    for (const auto& scale : scaleTemplates)
        scaleNames.add(scale.name);
    layout.push_back(std::make_unique<ChoiceParam>("scale", "Scale", scaleNames, 0));
    layout.push_back(std::make_unique<ChoiceParam>("octaveShift", "Octave", juce::StringArray { "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4" }, 4));
    layout.push_back(std::make_unique<ChoiceParam>("mode", "Mode", juce::StringArray { "Natural", "Robot" }, 0));
    layout.push_back(std::make_unique<ChoiceParam>("harmonyInterval", "Harmony Interval", juce::StringArray { "-12", "-7", "-5", "+5", "+7", "+12", "+19" }, 4));

    layout.push_back(std::make_unique<FloatParam>("correctionAmount", "Correction", juce::NormalisableRange<float>(0.0f, 1.0f), 0.75f));
    layout.push_back(std::make_unique<FloatParam>("snapSpeed", "Snap Speed", juce::NormalisableRange<float>(0.01f, 0.35f, 0.001f, 0.45f), 0.12f));
    layout.push_back(std::make_unique<FloatParam>("tracking", "Tracking", juce::NormalisableRange<float>(8.0f, 40.0f, 0.1f, 0.45f), 20.0f));
    layout.push_back(std::make_unique<FloatParam>("tone", "Tone", juce::NormalisableRange<float>(0.0f, 1.0f), 0.65f));
    layout.push_back(std::make_unique<FloatParam>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    layout.push_back(std::make_unique<FloatParam>("harmonyMix", "Harmony", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.push_back(std::make_unique<FloatParam>("outputGain", "Output", juce::NormalisableRange<float>(-18.0f, 18.0f, 0.01f), 0.0f));

    return { layout.begin(), layout.end() };
}

float AggregaVocoderAudioProcessor::estimatePitchHz(const float* samples, int length, double sampleRate)
{
    constexpr double minFrequency = 70.0;
    constexpr double maxFrequency = 900.0;

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
            const auto a = static_cast<double>(samples[i]);
            const auto b = static_cast<double>(samples[i + lag]);
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

    if (bestLag == 0 || bestScore < 0.58)
        return 0.0f;

    return static_cast<float>(sampleRate / static_cast<double>(bestLag));
}

std::array<bool, 12> AggregaVocoderAudioProcessor::buildMaskFromTemplate(int rootNote, const ScaleTemplate& scaleTemplate)
{
    std::array<bool, 12> mask {};
    for (int i = 0; i < 12; ++i)
        if (scaleTemplate.intervals[static_cast<size_t>(i)] != 0)
            mask[static_cast<size_t>((rootNote + i) % 12)] = true;
    return mask;
}

float AggregaVocoderAudioProcessor::wrapPhase(float phase) noexcept
{
    while (phase >= 1.0f)
        phase -= 1.0f;
    while (phase < 0.0f)
        phase += 1.0f;
    return phase;
}

int AggregaVocoderAudioProcessor::findNearestAllowedMidiNote(float midiNote) const
{
    const auto rounded = juce::roundToInt(midiNote);
    int bestNote = rounded;
    float bestDistance = std::numeric_limits<float>::max();

    for (int note = 24; note <= 108; ++note)
    {
        if (! allowedPitchClasses[static_cast<size_t>(note % 12)])
            continue;

        const auto distance = std::abs(static_cast<float>(note) - midiNote);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestNote = note;
        }
    }

    return bestNote;
}

void AggregaVocoderAudioProcessor::updateScaleMask()
{
    const auto root = juce::roundToInt(parameters.getRawParameterValue("rootNote")->load());
    const auto scaleIndex = juce::roundToInt(parameters.getRawParameterValue("scale")->load());
    allowedPitchClasses = buildMaskFromTemplate(root, scaleTemplates[static_cast<size_t>(juce::jlimit(0, static_cast<int>(scaleTemplates.size()) - 1, scaleIndex))]);
}

void AggregaVocoderAudioProcessor::updateBandFrequencies()
{
    constexpr auto lowHz = 90.0f;
    constexpr auto highHz = 9000.0f;
    for (size_t i = 0; i < numBands; ++i)
    {
        const auto t = static_cast<float>(i) / static_cast<float>(numBands - 1);
        bandFrequencies[i] = lowHz * std::pow(highHz / lowHz, t);
    }
}

float AggregaVocoderAudioProcessor::renderCarrierSample(CarrierState& state, float frequency, float tone, float noiseMix, float harmonyMix, int harmonySemitones) noexcept
{
    const auto sampleRate = static_cast<float>(currentSampleRate);
    const auto harmFreq = frequency * std::pow(2.0f, static_cast<float>(harmonySemitones) / 12.0f);

    state.phaseA = wrapPhase(state.phaseA + (frequency / sampleRate));
    state.phaseB = wrapPhase(state.phaseB + ((frequency * 2.0f) / sampleRate));
    state.phaseSub = wrapPhase(state.phaseSub + (harmFreq / sampleRate));

    const auto saw = (state.phaseA * 2.0f) - 1.0f;
    const auto pulse = state.phaseB < 0.45f ? 1.0f : -1.0f;
    const auto sub = std::sin(state.phaseSub * juce::MathConstants<float>::twoPi);
    const auto noise = random.nextFloat() * 2.0f - 1.0f;

    auto carrier = juce::jmap(tone, saw, pulse);
    carrier = (carrier * (1.0f - harmonyMix * 0.35f)) + (sub * harmonyMix * 0.35f);
    carrier = (carrier * (1.0f - noiseMix)) + (noise * noiseMix);
    return carrier * 0.45f;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AggregaVocoderAudioProcessor();
}
