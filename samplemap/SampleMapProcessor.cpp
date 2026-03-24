#include "SampleMapProcessor.h"
#include "SampleMapEditor.h"

namespace
{
double midiToFrequency(double midiNote)
{
    return 440.0 * std::pow(2.0, (midiNote - 69.0) / 12.0);
}

float estimateRms(const float* data, int start, int length)
{
    if (length <= 0)
        return 0.0f;

    double sum = 0.0;
    for (int i = 0; i < length; ++i)
    {
        const auto sample = data[start + i];
        sum += static_cast<double>(sample) * static_cast<double>(sample);
    }

    return static_cast<float>(std::sqrt(sum / static_cast<double>(length)));
}

float estimatePitchHz(const float* data, int start, int length, double sampleRate)
{
    constexpr float minFrequency = 55.0f;
    constexpr float maxFrequency = 1760.0f;

    const auto minLag = std::max(2, static_cast<int>(sampleRate / maxFrequency));
    const auto maxLag = std::min(length - 2, static_cast<int>(sampleRate / minFrequency));

    if (maxLag <= minLag)
        return 0.0f;

    auto correlationScoreForLag = [data, start, length](int lag)
    {
        double correlation = 0.0;
        double energyA = 0.0;
        double energyB = 0.0;

        for (int i = 0; i < length - lag; ++i)
        {
            const auto a = static_cast<double>(data[start + i]);
            const auto b = static_cast<double>(data[start + i + lag]);
            correlation += a * b;
            energyA += a * a;
            energyB += b * b;
        }

        const auto normaliser = std::sqrt(energyA * energyB);
        if (normaliser <= 0.0)
            return 0.0;

        return correlation / normaliser;
    };

    double bestCorrelation = 0.0;
    int bestLag = 0;

    for (int lag = minLag; lag <= maxLag;)
    {
        const auto score = correlationScoreForLag(lag);
        if (score > bestCorrelation)
        {
            bestCorrelation = score;
            bestLag = lag;
        }

        lag += (lag < 128 ? 1 : 2);
    }

    if (bestLag == 0 || bestCorrelation < 0.72)
        return 0.0f;

    for (int divisor = 2; divisor <= 4; ++divisor)
    {
        const auto candidateLag = bestLag / divisor;
        if (candidateLag < minLag)
            continue;

        const auto candidateScore = correlationScoreForLag(candidateLag);
        if (candidateScore >= bestCorrelation * 0.94)
        {
            bestLag = candidateLag;
            bestCorrelation = candidateScore;
        }
    }

    return static_cast<float>(sampleRate / static_cast<double>(bestLag));
}

class MappedSampleSound : public juce::SynthesiserSound
{
public:
    MappedSampleSound(std::array<AggregaMapAudioProcessor::NoteAssignment, 128> assignmentsToCopy,
                      std::shared_ptr<juce::AudioBuffer<float>> sourceToShare)
        : assignments(std::move(assignmentsToCopy)), source(std::move(sourceToShare))
    {
    }

    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }

    const auto& getAssignments() const noexcept { return assignments; }
    std::shared_ptr<juce::AudioBuffer<float>> getSource() const noexcept { return source; }

private:
    std::array<AggregaMapAudioProcessor::NoteAssignment, 128> assignments;
    std::shared_ptr<juce::AudioBuffer<float>> source;
};

class MappedSampleVoice : public juce::SynthesiserVoice
{
public:
    explicit MappedSampleVoice(juce::AudioProcessorValueTreeState& state)
        : parameters(state)
    {
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<MappedSampleSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int) override
    {
        auto* mappedSound = dynamic_cast<MappedSampleSound*>(sound);
        if (mappedSound == nullptr)
            return;

        const auto& assignment = mappedSound->getAssignments()[juce::jlimit(0, 127, midiNoteNumber)];
        if (! assignment.valid)
        {
            clearCurrentNote();
            return;
        }

        source = mappedSound->getSource();
        if (source == nullptr || source->getNumSamples() <= 1 || source->getNumChannels() == 0)
        {
            clearCurrentNote();
            return;
        }

        startSample = juce::jlimit(0, source->getNumSamples() - 1, assignment.startSample);
        endSample = juce::jlimit(startSample + 1, source->getNumSamples(), assignment.endSample);
        currentPosition = static_cast<double>(startSample);
        level = velocity;

        adsr.setSampleRate(getSampleRate() > 0.0 ? getSampleRate() : 44100.0);

        const auto sourceFrequency = midiToFrequency(static_cast<double>(assignment.rootMidiNote));
        const auto targetFrequency = midiToFrequency(static_cast<double>(midiNoteNumber));
        step = targetFrequency / sourceFrequency;
        noteLevelCompensation = assignment.levelCompensation;

        juce::ADSR::Parameters env;
        env.attack = 0.002f;
        env.decay = 0.03f;
        env.sustain = 1.0f;
        env.release = parameters.getRawParameterValue("release")->load();
        adsr.setParameters(env);
        adsr.noteOn();
    }

    void stopNote(float, bool allowTailOff) override
    {
        adsr.noteOff();

        if (! allowTailOff || ! adsr.isActive())
        {
            source.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int start, int numSamples) override
    {
        if (source == nullptr || ! isVoiceActive() || endSample <= startSample)
            return;

        const auto segmentLength = endSample - startSample;
        if (segmentLength <= 1)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            if (! adsr.isActive())
            {
                source.reset();
                clearCurrentNote();
                break;
            }

            while (currentPosition >= static_cast<double>(endSample))
                currentPosition -= static_cast<double>(segmentLength);

            const auto sampleIndex = juce::jlimit(startSample, endSample - 1, static_cast<int>(currentPosition));
            const auto nextIndex = juce::jlimit(startSample, endSample - 1, sampleIndex + 1);
            const auto frac = static_cast<float>(currentPosition - static_cast<double>(sampleIndex));
            const auto left0 = source->getSample(0, sampleIndex);
            const auto left1 = source->getSample(0, nextIndex);
            auto sample = juce::jmap(frac, left0, left1);

            if (source->getNumChannels() > 1)
            {
                const auto right0 = source->getSample(1, sampleIndex);
                const auto right1 = source->getSample(1, nextIndex);
                sample = 0.5f * (sample + juce::jmap(frac, right0, right1));
            }

            const auto levelMatchEnabled = parameters.getRawParameterValue("levelMatch")->load() >= 0.5f;
        const auto assignmentGain = levelMatchEnabled ? noteLevelCompensation : 1.0f;
        sample *= adsr.getNextSample() * level * assignmentGain * parameters.getRawParameterValue("gain")->load();

            for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
                outputBuffer.addSample(channel, start + i, sample);

            currentPosition += step;
        }
    }

private:
    juce::AudioProcessorValueTreeState& parameters;
    juce::ADSR adsr;
    std::shared_ptr<juce::AudioBuffer<float>> source;
    int startSample = 0;
    int endSample = 0;
    double currentPosition = 0.0;
    double step = 1.0;
    float level = 0.0f;
    float noteLevelCompensation = 1.0f;
};
} // namespace

AggregaMapAudioProcessor::AggregaMapAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();

    const juce::ScopedLock lock(synthLock);
    for (int i = 0; i < 16; ++i)
        synth.addVoice(new MappedSampleVoice(parameters));

    loadedBuffer = std::make_shared<juce::AudioBuffer<float>>();
    synth.addSound(new MappedSampleSound(noteAssignments, loadedBuffer));
}

AggregaMapAudioProcessor::~AggregaMapAudioProcessor()
{
    joinLoaderThread();
}

void AggregaMapAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate(sampleRate);
    juce::ignoreUnused(samplesPerBlock);
}

void AggregaMapAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AggregaMapAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
#endif

void AggregaMapAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    const juce::ScopedLock lock(synthLock);
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* AggregaMapAudioProcessor::createEditor()
{
    return new AggregaMapAudioProcessorEditor(*this);
}

bool AggregaMapAudioProcessor::hasEditor() const { return true; }
const juce::String AggregaMapAudioProcessor::getName() const { return JucePlugin_Name; }
bool AggregaMapAudioProcessor::acceptsMidi() const { return true; }
bool AggregaMapAudioProcessor::producesMidi() const { return false; }
bool AggregaMapAudioProcessor::isMidiEffect() const { return false; }
double AggregaMapAudioProcessor::getTailLengthSeconds() const { return 1.0; }
int AggregaMapAudioProcessor::getNumPrograms() { return 1; }
int AggregaMapAudioProcessor::getCurrentProgram() { return 0; }
void AggregaMapAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String AggregaMapAudioProcessor::getProgramName(int index) { juce::ignoreUnused(index); return {}; }
void AggregaMapAudioProcessor::changeProgramName(int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

void AggregaMapAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void AggregaMapAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::String AggregaMapAudioProcessor::getAnalysisSummary() const
{
    const juce::ScopedLock lock(stateLock);
    return analysisSummary;
}

void AggregaMapAudioProcessor::setAnalysisSummary(const juce::String& summary)
{
    const juce::ScopedLock lock(stateLock);
    analysisSummary = summary;
}

bool AggregaMapAudioProcessor::hasLoadedSource() const noexcept
{
    const juce::ScopedLock lock(stateLock);
    return loadedBuffer != nullptr && loadedBuffer->getNumSamples() > 0 && ! detectedRegions.empty();
}

void AggregaMapAudioProcessor::joinLoaderThread()
{
    if (loaderThread.joinable())
        loaderThread.join();
}

bool AggregaMapAudioProcessor::startLoadingSourceFile(const juce::File& file)
{
    if (loadingSource.exchange(true))
        return false;

    joinLoaderThread();
    loadingProgress.store(0.0f);
    setAnalysisSummary("Queued " + file.getFileName() + "...");

    loaderThread = std::thread([this, file]
    {
        performLoad(file);
    });

    return true;
}

void AggregaMapAudioProcessor::performLoad(const juce::File& file)
{
    loadingProgress.store(0.05f);
    setAnalysisSummary("Opening " + file.getFileName() + "...");

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        setAnalysisSummary("Could not read file. Try a WAV or MP3.");
        loadingProgress.store(0.0f);
        loadingSource.store(false);
        return;
    }

    loadingProgress.store(0.20f);
    setAnalysisSummary("Reading " + file.getFileName() + "...");

    juce::AudioBuffer<float> tempBuffer(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    reader->read(&tempBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    auto newBuffer = std::make_shared<juce::AudioBuffer<float>>();
    if (tempBuffer.getNumChannels() == 1)
    {
        newBuffer->setSize(2, tempBuffer.getNumSamples());
        newBuffer->copyFrom(0, 0, tempBuffer, 0, 0, tempBuffer.getNumSamples());
        newBuffer->copyFrom(1, 0, tempBuffer, 0, 0, tempBuffer.getNumSamples());
    }
    else
    {
        newBuffer->makeCopyOf(tempBuffer, true);
    }

    loadingProgress.store(0.60f);
    setAnalysisSummary("Analysing pitches in " + file.getFileName() + "...");

    auto newRegions = analyseSource(*newBuffer, reader->sampleRate);
    std::array<NoteAssignment, 128> newAssignments {};
    rebuildAssignments(newRegions, newAssignments);

    loadingProgress.store(0.90f);
    setAnalysisSummary("Building playable map...");

    {
        const juce::ScopedLock state(stateLock);
        loadedFileName = file.getFileName();
        loadedBuffer = newBuffer;
        detectedRegions = newRegions;
        noteAssignments = newAssignments;

        if (detectedRegions.empty())
        {
            analysisSummary = "Loaded " + loadedFileName + " but did not find stable pitched regions.";
        }
        else
        {
            analysisSummary = loadedFileName + " mapped across 128 notes from "
                + juce::MidiMessage::getMidiNoteName(detectedRegions.front().rootMidiNote, true, true, 4) + " to "
                + juce::MidiMessage::getMidiNoteName(detectedRegions.back().rootMidiNote, true, true, 4)
                + " using " + juce::String(static_cast<int>(detectedRegions.size())) + " detected regions.";
        }
    }

    {
        const juce::ScopedLock lock(synthLock);
        synth.clearSounds();
        synth.addSound(new MappedSampleSound(newAssignments, newBuffer));
    }

    loadingProgress.store(1.0f);
    loadingSource.store(false);
}

std::vector<AggregaMapAudioProcessor::Region> AggregaMapAudioProcessor::analyseSource(const juce::AudioBuffer<float>& source, double sourceSampleRate)
{
    auto analyseWithSettings = [this](const juce::AudioBuffer<float>& inputSource, double inputSampleRate, int minSegmentSamples, int maxSegmentSamples)
    {
        std::vector<Region> localRegions;

        if (inputSource.getNumSamples() < 4096)
            return localRegions;

        juce::AudioBuffer<float> mono(1, inputSource.getNumSamples());
        mono.copyFrom(0, 0, inputSource, 0, 0, inputSource.getNumSamples());
        if (inputSource.getNumChannels() > 1)
            mono.addFrom(0, 0, inputSource, 1, 0, inputSource.getNumSamples(), 1.0f);
        mono.applyGain(0.5f);

        constexpr int frameSize = 2048;
        constexpr int hopSize = 1024;
        const auto* data = mono.getReadPointer(0);

        struct FrameInfo { int midi = -1; int start = 0; int length = 0; float rms = 0.0f; };
        std::vector<FrameInfo> frames;
        frames.reserve(static_cast<size_t>(mono.getNumSamples() / hopSize) + 1);
        const auto totalFrames = std::max(1, (mono.getNumSamples() - frameSize) / hopSize);
        int frameCounter = 0;

        for (int start = 0; start + frameSize < mono.getNumSamples(); start += hopSize)
        {
            ++frameCounter;
            if ((frameCounter % 32) == 0)
            {
                const auto frameProgress = static_cast<float>(frameCounter) / static_cast<float>(totalFrames);
                loadingProgress.store(0.60f + 0.28f * juce::jlimit(0.0f, 1.0f, frameProgress));
                setAnalysisSummary("Analysing pitches... " + juce::String(juce::roundToInt(frameProgress * 100.0f)) + "%");
            }

            const auto rms = estimateRms(data, start, frameSize);
            if (rms < 0.015f)
            {
                frames.push_back({ -1, start, frameSize, rms });
                continue;
            }

            const auto pitchHz = estimatePitchHz(data, start, frameSize, inputSampleRate);
            if (pitchHz <= 0.0f)
            {
                frames.push_back({ -1, start, frameSize, rms });
                continue;
            }

            const auto midi = juce::roundToInt(69.0 + 12.0 * std::log2(static_cast<double>(pitchHz) / 440.0));
            frames.push_back({ juce::jlimit(0, 127, midi), start, frameSize, rms });
        }

        if (frames.empty())
            return localRegions;

        int currentMidi = frames.front().midi;
        int regionStart = frames.front().start;
        float rmsAccumulator = currentMidi >= 0 ? frames.front().rms : 0.0f;
        int rmsCount = currentMidi >= 0 ? 1 : 0;

        auto flushRegion = [&](int midi, int startSample, int endSample, float rmsValue)
        {
            const auto clampedStart = juce::jlimit(0, mono.getNumSamples() - 1, startSample);
            const auto clampedEnd = juce::jlimit(clampedStart + 1, mono.getNumSamples(), endSample);
            const auto length = clampedEnd - clampedStart;
            if (midi < 0 || length < minSegmentSamples)
                return;

            for (int chunkStart = clampedStart; chunkStart < clampedEnd; chunkStart += maxSegmentSamples)
            {
                const auto chunkEnd = juce::jmin(clampedEnd, chunkStart + maxSegmentSamples);
                const auto chunkLength = chunkEnd - chunkStart;
                if (chunkLength < minSegmentSamples)
                    break;

                Region region;
                region.rootMidiNote = midi;
                region.startSample = chunkStart;
                region.endSample = chunkEnd;
                region.rms = rmsValue;
                localRegions.push_back(region);
            }
        };

        for (size_t i = 1; i < frames.size(); ++i)
        {
            const auto& frame = frames[i];
            if (frame.midi == currentMidi)
            {
                if (frame.midi >= 0)
                {
                    rmsAccumulator += frame.rms;
                    ++rmsCount;
                }
                continue;
            }

            flushRegion(currentMidi, regionStart, frame.start, rmsCount > 0 ? rmsAccumulator / static_cast<float>(rmsCount) : 0.0f);
            currentMidi = frame.midi;
            regionStart = frame.start;
            rmsAccumulator = frame.midi >= 0 ? frame.rms : 0.0f;
            rmsCount = frame.midi >= 0 ? 1 : 0;
        }

        const auto& lastFrame = frames.back();
        flushRegion(currentMidi, regionStart, lastFrame.start + lastFrame.length, rmsCount > 0 ? rmsAccumulator / static_cast<float>(rmsCount) : 0.0f);

        std::sort(localRegions.begin(), localRegions.end(), [](const Region& a, const Region& b)
        {
            if (a.rootMidiNote != b.rootMidiNote)
                return a.rootMidiNote < b.rootMidiNote;

            if (a.startSample != b.startSample)
                return a.startSample < b.startSample;

            return a.rms > b.rms;
        });

        return localRegions;
    };

    const auto configuredMinSegmentSamples = juce::jmax(1, static_cast<int>(sourceSampleRate * (getMinSegmentMs() * 0.001f)));
    const auto configuredMaxSegmentSamples = juce::jmax(configuredMinSegmentSamples, static_cast<int>(sourceSampleRate * (getMaxSegmentMs() * 0.001f)));

    auto regions = analyseWithSettings(source, sourceSampleRate, configuredMinSegmentSamples, configuredMaxSegmentSamples);
    if (! regions.empty())
        return regions;

    const auto fallbackMinSegmentSamples = juce::jmax(1, static_cast<int>(sourceSampleRate * 0.04));
    const auto fallbackMaxSegmentSamples = juce::jmax(fallbackMinSegmentSamples, static_cast<int>(sourceSampleRate * 1.0));
    setAnalysisSummary("Configured segment limits found nothing. Retrying with safer defaults...");
    return analyseWithSettings(source, sourceSampleRate, fallbackMinSegmentSamples, fallbackMaxSegmentSamples);
}

void AggregaMapAudioProcessor::rebuildAssignments(const std::vector<Region>& regions, std::array<NoteAssignment, 128>& assignments) const
{
    for (auto& assignment : assignments)
        assignment = {};

    if (regions.empty())
        return;

    std::vector<float> sortedRms;
    sortedRms.reserve(regions.size());
    for (const auto& region : regions)
        sortedRms.push_back(region.rms);

    std::sort(sortedRms.begin(), sortedRms.end());
    const auto referenceRms = sortedRms[sortedRms.size() / 2];

    for (int midi = 0; midi < static_cast<int>(assignments.size()); ++midi)
    {
        int bestDistance = std::numeric_limits<int>::max();
        std::vector<const Region*> candidates;

        for (const auto& region : regions)
        {
            const auto distance = std::abs(region.rootMidiNote - midi);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                candidates.clear();
                candidates.push_back(&region);
            }
            else if (distance == bestDistance)
            {
                candidates.push_back(&region);
            }
        }

        if (candidates.empty())
            continue;

        const auto candidateIndex = static_cast<size_t>(std::abs(midi)) % candidates.size();
        const auto* best = candidates[candidateIndex];
        const auto safeRegionRms = juce::jmax(0.0001f, best->rms);
        const auto levelCompensation = juce::jlimit(0.35f, 3.0f, referenceRms / safeRegionRms);
        assignments[static_cast<size_t>(midi)] = { best->rootMidiNote, best->startSample, best->endSample, levelCompensation, true };
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout AggregaMapAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gain", "Gain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.85f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("release", "Release", juce::NormalisableRange<float>(0.02f, 2.5f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("levelMatch", "Match Levels", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("minSegmentMs", "Min Segment", juce::NormalisableRange<float>(40.0f, 1200.0f, 10.0f), 80.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("maxSegmentMs", "Max Segment", juce::NormalisableRange<float>(80.0f, 4000.0f, 10.0f), 700.0f));
    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AggregaMapAudioProcessor();
}

