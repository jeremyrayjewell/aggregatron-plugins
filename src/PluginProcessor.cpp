#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
double noteNumberToFrequency(double noteNumber)
{
    return 440.0 * std::pow(2.0, (noteNumber - 69.0) / 12.0);
}

float wrapPhase01(float phase) noexcept
{
    phase -= std::floor(phase);
    return phase;
}

float wrapPhaseRadians(float phase) noexcept
{
    while (phase > juce::MathConstants<float>::pi)
        phase -= juce::MathConstants<float>::twoPi;

    while (phase < -juce::MathConstants<float>::pi)
        phase += juce::MathConstants<float>::twoPi;

    return phase;
}

float polyBlep(float phase, float phaseIncrement) noexcept
{
    if (phaseIncrement <= 0.0f)
        return 0.0f;

    if (phase < phaseIncrement)
    {
        const auto t = phase / phaseIncrement;
        return t + t - t * t - 1.0f;
    }

    if (phase > 1.0f - phaseIncrement)
    {
        const auto t = (phase - 1.0f) / phaseIncrement;
        return t * t + t + t + 1.0f;
    }

    return 0.0f;
}

float sawWave(float phase, float phaseIncrement) noexcept
{
    auto sample = 2.0f * phase - 1.0f;
    sample -= polyBlep(phase, phaseIncrement);
    return sample;
}

float squareWave(float phase, float phaseIncrement) noexcept
{
    auto sample = phase < 0.5f ? 1.0f : -1.0f;
    sample += polyBlep(phase, phaseIncrement);
    sample -= polyBlep(wrapPhase01(phase + 0.5f), phaseIncrement);
    return sample;
}

float triangleWave(float phase) noexcept
{
    return 1.0f - 4.0f * std::abs(phase - 0.5f);
}

float sineWave(float phase) noexcept
{
    return std::sin(phase * juce::MathConstants<float>::twoPi);
}

class SynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    enum class Waveform
    {
        saw = 0,
        square,
        triangle,
        sine
    };

    explicit SynthVoice(juce::AudioProcessorValueTreeState& state)
        : parameters(state)
    {
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SynthSound*>(sound) != nullptr;
    }

    void setCurrentPlaybackSampleRate(double newRate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate(newRate);

        if (newRate <= 0.0)
            return;

        ampEnvelope.setSampleRate(newRate);
        filterEnvelope.setSampleRate(newRate);
        ampEnvelope.reset();
        filterEnvelope.reset();

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = newRate;
        spec.maximumBlockSize = 2048;
        spec.numChannels = 1;
        filter.prepare(spec);
        filter.reset();
        lastPreparedSampleRate = 0.0;
    }

    void setLegatoMode(bool shouldUseLegato) noexcept
    {
        pendingLegato = shouldUseLegato;
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        prepareSmoothers();
        const auto shouldGlide = pendingLegato && isVoiceActive() && ampEnvelope.isActive();
        pendingLegato = false;

        currentMidiNote = midiNoteNumber;
        targetMidiNote = static_cast<float>(midiNoteNumber);
        noteVelocity = velocity;
        updatePitch();
        updateGlideTarget(shouldGlide);

        if (shouldGlide)
            return;

        osc1Phase = 0.0f;
        osc2Phase = 0.0f;
        lfoPhase = 0.0f;
        filter.reset();
        ampEnvelope.noteOn();
        filterEnvelope.noteOn();
    }

    void stopNote(float, bool allowTailOff) override
    {
        ampEnvelope.noteOff();
        filterEnvelope.noteOff();

        if (! allowTailOff || ! ampEnvelope.isActive())
            clearCurrentNote();
    }

    void pitchWheelMoved(int newPitchWheelValue) override
    {
        pitchWheelValue = newPitchWheelValue;
        updatePitch();
    }

    void controllerMoved(int controllerNumber, int newControllerValue) override
    {
        if (controllerNumber == 1)
            modWheelValue = static_cast<float>(newControllerValue) / 127.0f;
    }

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        updateParameters();
        prepareSmoothers();

        if (! isVoiceActive())
            return;

        auto* left = outputBuffer.getWritePointer(0);
        auto* right = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer(1) : nullptr;

        while (--numSamples >= 0)
        {
            const auto lfoValue = std::sin(lfoPhase);
            const auto vibratoSemitones = lfoValue * (lfoDepthSemitones + (modWheelValue * 0.35f));
            const auto noteWithMod = static_cast<double>(smoothedMidiNote.getNextValue()) + static_cast<double>(pitchBendSemitones + vibratoSemitones);
            const auto osc1Frequency = noteNumberToFrequency(noteWithMod);
            const auto osc2Frequency = noteNumberToFrequency(noteWithMod + static_cast<double>(osc2DetuneSemitones) + static_cast<double>(osc2FineCents / 100.0f));

            osc1Delta = static_cast<float>(osc1Frequency / getSampleRate());
            osc2Delta = static_cast<float>(osc2Frequency / getSampleRate());

            const auto osc1Sample = renderOscillatorSample(osc1Waveform, osc1Phase, osc1Delta);
            const auto osc2Sample = renderOscillatorSample(osc2Waveform, osc2Phase, osc2Delta);
            auto sample = juce::jmap(oscMix, osc1Sample, osc2Sample);

            osc1Phase = wrapPhase(osc1Phase + osc1Delta);
            osc2Phase = wrapPhase(osc2Phase + osc2Delta);
            lfoPhase = wrapPhaseRadians(lfoPhase + static_cast<float>(juce::MathConstants<double>::twoPi * lfoRateHz / getSampleRate()));

            const auto filterEnvValue = filterEnvelope.getNextSample();
            const auto velocityCutoffBoost = noteVelocity * velocityFilterAmountHz;
            const auto cutoffMod = (filterEnvValue * filterEnvAmountHz) + velocityCutoffBoost + (lfoValue * lfoFilterAmountHz);
            const auto cutoff = juce::jlimit(20.0f, 18000.0f, smoothedFilterCutoffHz.getNextValue() + cutoffMod);
            filter.setCutoffFrequency(cutoff);
            filter.setResonance(filterResonance);

            sample = filter.processSample(0, sample);
            sample = std::tanh(sample * smoothedDriveAmount.getNextValue());
            const auto velocityGain = (1.0f - velocityAmpAmount) + (noteVelocity * velocityAmpAmount);
            sample *= ampEnvelope.getNextSample() * noteVelocity * velocityGain;

            left[startSample] += sample;
            if (right != nullptr)
                right[startSample] += sample;

            ++startSample;
        }

        if (! ampEnvelope.isActive())
            clearCurrentNote();
    }

private:
    static float wrapPhase(float phase)
    {
        return wrapPhase01(phase);
    }

    void updatePitch()
    {
        const auto normalizedWheel = (static_cast<float>(pitchWheelValue) - 8192.0f) / 8192.0f;
        pitchBendSemitones = normalizedWheel * pitchBendRangeSemitones;
    }

    void prepareSmoothers()
    {
        const auto sampleRate = getSampleRate();

        if (sampleRate <= 0.0 || sampleRate == lastPreparedSampleRate)
            return;

        smoothedFilterCutoffHz.reset(sampleRate, 0.02);
        smoothedDriveAmount.reset(sampleRate, 0.02);
        smoothedMidiNote.reset(sampleRate, juce::jmax(0.0, static_cast<double>(glideTimeSeconds)));
        smoothedFilterCutoffHz.setCurrentAndTargetValue(filterCutoffHz);
        smoothedDriveAmount.setCurrentAndTargetValue(driveAmount);
        smoothedMidiNote.setCurrentAndTargetValue(targetMidiNote);
        lastPreparedSampleRate = sampleRate;
        lastPreparedGlideTime = glideTimeSeconds;
    }

    void updateParameters()
    {
        juce::ADSR::Parameters ampParams;
        ampParams.attack = parameters.getRawParameterValue("attack")->load();
        ampParams.decay = parameters.getRawParameterValue("decay")->load();
        ampParams.sustain = parameters.getRawParameterValue("sustain")->load();
        ampParams.release = parameters.getRawParameterValue("release")->load();
        ampEnvelope.setParameters(ampParams);

        juce::ADSR::Parameters filterParams;
        filterParams.attack = parameters.getRawParameterValue("filterAttack")->load();
        filterParams.decay = parameters.getRawParameterValue("filterDecay")->load();
        filterParams.sustain = parameters.getRawParameterValue("filterSustain")->load();
        filterParams.release = parameters.getRawParameterValue("filterRelease")->load();
        filterEnvelope.setParameters(filterParams);

        osc1Waveform = static_cast<Waveform>(juce::jlimit(0, 3, juce::roundToInt(parameters.getRawParameterValue("osc1Wave")->load())));
        osc2Waveform = static_cast<Waveform>(juce::jlimit(0, 3, juce::roundToInt(parameters.getRawParameterValue("osc2Wave")->load())));
        oscMix = parameters.getRawParameterValue("oscMix")->load();
        osc2DetuneSemitones = parameters.getRawParameterValue("osc2Detune")->load();
        osc2FineCents = parameters.getRawParameterValue("osc2Fine")->load();
        filterCutoffHz = parameters.getRawParameterValue("filterCutoff")->load();
        filterResonance = parameters.getRawParameterValue("filterResonance")->load();
        filterEnvAmountHz = parameters.getRawParameterValue("filterEnvAmount")->load();
        velocityFilterAmountHz = parameters.getRawParameterValue("velocityFilter")->load();
        lfoRateHz = parameters.getRawParameterValue("lfoRate")->load();
        lfoDepthSemitones = parameters.getRawParameterValue("lfoPitchDepth")->load();
        lfoFilterAmountHz = parameters.getRawParameterValue("lfoFilterDepth")->load();
        glideTimeSeconds = parameters.getRawParameterValue("glide")->load();
        velocityAmpAmount = parameters.getRawParameterValue("velocityAmp")->load();
        driveAmount = juce::jmap(parameters.getRawParameterValue("drive")->load(), 1.0f, 8.0f);

        if (lastPreparedSampleRate > 0.0)
        {
            if (! juce::approximatelyEqual(glideTimeSeconds, lastPreparedGlideTime))
            {
                const auto currentNote = smoothedMidiNote.getCurrentValue();
                smoothedMidiNote.reset(lastPreparedSampleRate, juce::jmax(0.0, static_cast<double>(glideTimeSeconds)));
                smoothedMidiNote.setCurrentAndTargetValue(currentNote);
                smoothedMidiNote.setTargetValue(targetMidiNote);
                lastPreparedGlideTime = glideTimeSeconds;
            }

            smoothedFilterCutoffHz.setTargetValue(filterCutoffHz);
            smoothedDriveAmount.setTargetValue(driveAmount);
            smoothedMidiNote.setTargetValue(targetMidiNote);
        }
    }

    void updateGlideTarget(bool shouldGlide)
    {
        if (lastPreparedSampleRate <= 0.0)
            return;

        if (! shouldGlide || glideTimeSeconds <= 0.0f)
        {
            smoothedMidiNote.setCurrentAndTargetValue(targetMidiNote);
            return;
        }

        smoothedMidiNote.setTargetValue(targetMidiNote);
    }

    static float renderOscillatorSample(Waveform waveform, float phase, float phaseIncrement) noexcept
    {
        switch (waveform)
        {
            case Waveform::square:
                return squareWave(phase, phaseIncrement);
            case Waveform::triangle:
                return triangleWave(phase);
            case Waveform::sine:
                return sineWave(phase);
            case Waveform::saw:
            default:
                return sawWave(phase, phaseIncrement);
        }
    }

    juce::AudioProcessorValueTreeState& parameters;
    juce::ADSR ampEnvelope;
    juce::ADSR filterEnvelope;
    juce::dsp::StateVariableTPTFilter<float> filter;
    float osc1Phase = 0.0f;
    float osc2Phase = 0.0f;
    float osc1Delta = 0.0f;
    float osc2Delta = 0.0f;
    float noteVelocity = 0.0f;
    float lfoPhase = 0.0f;
    float pitchBendSemitones = 0.0f;
    float modWheelValue = 0.0f;
    Waveform osc1Waveform = Waveform::saw;
    Waveform osc2Waveform = Waveform::square;
    float oscMix = 0.35f;
    float osc2DetuneSemitones = 0.0f;
    float osc2FineCents = 0.0f;
    float filterCutoffHz = 4000.0f;
    float filterResonance = 0.3f;
    float filterEnvAmountHz = 2000.0f;
    float velocityFilterAmountHz = 0.0f;
    float lfoRateHz = 5.0f;
    float lfoDepthSemitones = 0.0f;
    float lfoFilterAmountHz = 0.0f;
    float glideTimeSeconds = 0.0f;
    float velocityAmpAmount = 0.0f;
    float driveAmount = 1.0f;
    juce::SmoothedValue<float> smoothedFilterCutoffHz;
    juce::SmoothedValue<float> smoothedDriveAmount;
    juce::SmoothedValue<float> smoothedMidiNote;
    double lastPreparedSampleRate = 0.0;
    float lastPreparedGlideTime = -1.0f;
    int currentMidiNote = 60;
    float targetMidiNote = 60.0f;
    int pitchWheelValue = 8192;
    bool pendingLegato = false;

    static constexpr float pitchBendRangeSemitones = 2.0f;
};

class AggregaKeysSynth : public juce::Synthesiser
{
public:
    void setMaximumPlayableVoices(int newMaxVoices) noexcept
    {
        maximumPlayableVoices = juce::jlimit(1, 16, newMaxVoices);
    }

    void setMonoMode(bool shouldBeMono)
    {
        if (monoMode == shouldBeMono)
            return;

        monoMode = shouldBeMono;
        heldNotes.clear();

        if (monoMode)
            stopExtraVoices();
    }

    void noteOn(int midiChannel, int midiNoteNumber, float velocity) override
    {
        if (! monoMode)
            return juce::Synthesiser::noteOn(midiChannel, midiNoteNumber, velocity);

        const auto hadHeldNotes = getLastHeldNote() != nullptr;
        updateHeldNote(midiChannel, midiNoteNumber, velocity);

        if (auto* sound = findSoundFor(midiChannel, midiNoteNumber))
        {
            if (auto* voice = getPrimaryVoice())
            {
                voice->setLegatoMode(hadHeldNotes);
                startVoice(voice, sound, midiChannel, midiNoteNumber, velocity);
                stopExtraVoices();
            }
        }
    }

    void noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override
    {
        if (! monoMode)
            return juce::Synthesiser::noteOff(midiChannel, midiNoteNumber, velocity, allowTailOff);

        removeHeldNote(midiChannel, midiNoteNumber);

        auto* voice = getPrimaryVoice();
        if (voice == nullptr || ! voice->isVoiceActive())
            return;

        if (const auto* held = getLastHeldNote())
        {
            if (held->midiNoteNumber != voice->getCurrentlyPlayingNote())
            {
                if (auto* sound = findSoundFor(held->midiChannel, held->midiNoteNumber))
                {
                    voice->setLegatoMode(true);
                    startVoice(voice, sound, held->midiChannel, held->midiNoteNumber, held->velocity);
                }
            }

            return;
        }

        stopVoice(voice, velocity, allowTailOff);
    }

protected:
    SynthesiserVoice* findFreeVoice(juce::SynthesiserSound* soundToPlay,
                                    int midiChannel,
                                    int midiNoteNumber,
                                    bool stealIfNoneAvailable) const override
    {
        juce::ignoreUnused(midiChannel, midiNoteNumber);

        const auto eligibleVoices = juce::jlimit(1, voices.size(), monoMode ? 1 : maximumPlayableVoices);

        for (int i = 0; i < eligibleVoices; ++i)
        {
            auto* voice = voices.getUnchecked(i);

            if ((! voice->isVoiceActive()) && voice->canPlaySound(soundToPlay))
                return voice;
        }

        for (int i = 0; i < eligibleVoices; ++i)
        {
            auto* voice = voices.getUnchecked(i);

            if (voice->isVoiceActive()
                && ! voice->isKeyDown()
                && ! voice->isSustainPedalDown()
                && ! voice->isSostenutoPedalDown()
                && voice->canPlaySound(soundToPlay))
                return voice;
        }

        if (! stealIfNoneAvailable)
            return nullptr;

        SynthesiserVoice* oldestVoice = nullptr;

        for (int i = 0; i < eligibleVoices; ++i)
        {
            auto* voice = voices.getUnchecked(i);

            if (! voice->canPlaySound(soundToPlay))
                continue;

            if (oldestVoice == nullptr || voice->wasStartedBefore(*oldestVoice))
                oldestVoice = voice;
        }

        return oldestVoice;
    }

private:
    struct HeldNote
    {
        int midiChannel = 1;
        int midiNoteNumber = 60;
        float velocity = 1.0f;
    };

    void stopExtraVoices()
    {
        for (int i = 1; i < voices.size(); ++i)
        {
            if (auto* voice = voices.getUnchecked(i); voice->isVoiceActive())
                stopVoice(voice, 0.0f, false);
        }
    }

    void updateHeldNote(int midiChannel, int midiNoteNumber, float velocity)
    {
        removeHeldNote(midiChannel, midiNoteNumber);
        heldNotes.push_back({ midiChannel, midiNoteNumber, velocity });
    }

    void removeHeldNote(int midiChannel, int midiNoteNumber)
    {
        heldNotes.erase(std::remove_if(heldNotes.begin(), heldNotes.end(),
                                       [=](const HeldNote& heldNote)
                                       {
                                           return heldNote.midiChannel == midiChannel
                                               && heldNote.midiNoteNumber == midiNoteNumber;
                                       }),
                        heldNotes.end());
    }

    const HeldNote* getLastHeldNote() const noexcept
    {
        if (heldNotes.empty())
            return nullptr;

        return &heldNotes.back();
    }

    juce::SynthesiserSound* findSoundFor(int midiChannel, int midiNoteNumber) const
    {
        for (auto* sound : sounds)
        {
            if (sound->appliesToNote(midiNoteNumber) && sound->appliesToChannel(midiChannel))
                return sound;
        }

        return nullptr;
    }

    SynthVoice* getPrimaryVoice() const
    {
        if (voices.isEmpty())
            return nullptr;

        return dynamic_cast<SynthVoice*>(voices.getUnchecked(0));
    }

    std::vector<HeldNote> heldNotes;
    int maximumPlayableVoices = 8;
    bool monoMode = false;
};
} // namespace

AggregatronKeysAudioProcessor::AggregatronKeysAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    synth = std::make_unique<AggregaKeysSynth>();
    syncVoiceCount();
    synth->addSound(new SynthSound());

    for (int i = 0; i < 16; ++i)
        synth->addVoice(new SynthVoice(parameters));
}

void AggregatronKeysAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    synth->setCurrentPlaybackSampleRate(sampleRate);
    synth->setNoteStealingEnabled(true);
    synthBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
    wetBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
    reverb.reset();
    gainSmoothed.reset(sampleRate, 0.02);
    reverbMixSmoothed.reset(sampleRate, 0.02);
    gainSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("gain")->load());
    reverbMixSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("reverbMix")->load());
    updateEffectParameters();
}

void AggregatronKeysAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AggregatronKeysAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
#endif

void AggregatronKeysAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    syncVoiceCount();
    updateEffectParameters();

    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (message.isPitchWheel())
        {
            uiPitchWheelValue.store(message.getPitchWheelValue());
            lastPitchWheelValue.store(message.getPitchWheelValue());
        }
        else if (message.isController() && message.getControllerNumber() == 1)
        {
            uiModWheelValue.store(message.getControllerValue());
            lastModWheelValue.store(message.getControllerValue());
        }
    }

    const auto pitchWheel = uiPitchWheelValue.load();
    if (pitchWheel != lastPitchWheelValue.load())
    {
        midiMessages.addEvent(juce::MidiMessage::pitchWheel(1, pitchWheel), 0);
        lastPitchWheelValue.store(pitchWheel);
    }

    const auto modWheel = uiModWheelValue.load();
    if (modWheel != lastModWheelValue.load())
    {
        midiMessages.addEvent(juce::MidiMessage::controllerEvent(1, 1, modWheel), 0);
        lastModWheelValue.store(modWheel);
    }

    synthBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    wetBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    synthBuffer.clear();
    synth->renderNextBlock(synthBuffer, midiMessages, 0, buffer.getNumSamples());

    gainSmoothed.setTargetValue(parameters.getRawParameterValue("gain")->load());
    reverbMixSmoothed.setTargetValue(parameters.getRawParameterValue("reverbMix")->load());

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        wetBuffer.copyFrom(channel, 0, synthBuffer, channel, 0, buffer.getNumSamples());

    if (wetBuffer.getNumChannels() > 1)
    {
        reverb.processStereo(wetBuffer.getWritePointer(0), wetBuffer.getWritePointer(1), wetBuffer.getNumSamples());
    }
    else if (wetBuffer.getNumChannels() == 1)
    {
        reverb.processMono(wetBuffer.getWritePointer(0), wetBuffer.getNumSamples());
    }

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto gain = gainSmoothed.getNextValue();
        const auto wetMix = reverbMixSmoothed.getNextValue();
        const auto dryMix = 1.0f - wetMix;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto drySample = synthBuffer.getSample(channel, sample);
            const auto wetSample = wetBuffer.getSample(channel, sample);
            buffer.setSample(channel, sample, (drySample * dryMix + wetSample * wetMix) * gain);
        }
    }

    captureWaveformSnapshot(buffer);

}

juce::AudioProcessorEditor* AggregatronKeysAudioProcessor::createEditor()
{
    return new AggregatronKeysAudioProcessorEditor(*this);
}

bool AggregatronKeysAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String AggregatronKeysAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AggregatronKeysAudioProcessor::acceptsMidi() const
{
    return true;
}

bool AggregatronKeysAudioProcessor::producesMidi() const
{
    return false;
}

bool AggregatronKeysAudioProcessor::isMidiEffect() const
{
    return false;
}

double AggregatronKeysAudioProcessor::getTailLengthSeconds() const
{
    return 2.0;
}

int AggregatronKeysAudioProcessor::getNumPrograms()
{
    return 1;
}

int AggregatronKeysAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AggregatronKeysAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String AggregatronKeysAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void AggregatronKeysAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void AggregatronKeysAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void AggregatronKeysAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

void AggregatronKeysAudioProcessor::setUiPitchWheel(int value) noexcept
{
    uiPitchWheelValue.store(juce::jlimit(0, 16383, value));
}

void AggregatronKeysAudioProcessor::setUiModWheel(float normalizedValue) noexcept
{
    const auto midiValue = juce::roundToInt(juce::jlimit(0.0f, 1.0f, normalizedValue) * 127.0f);
    uiModWheelValue.store(midiValue);
}

bool AggregatronKeysAudioProcessor::savePresetToFile(const juce::File& file)
{
    if (auto state = parameters.copyState(); auto xml = state.createXml())
        return xml->writeTo(file);

    return false;
}

bool AggregatronKeysAudioProcessor::loadPresetFromFile(const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        return false;

    if (! xml->hasTagName(parameters.state.getType()))
        return false;

    parameters.replaceState(juce::ValueTree::fromXml(*xml));
    return true;
}

bool AggregatronKeysAudioProcessor::getWaveformSnapshot(std::vector<float>& dest) const
{
    const juce::ScopedLock lock(waveformLock);

    if (recentWaveform.empty())
        return false;

    dest = recentWaveform;
    return true;
}

void AggregatronKeysAudioProcessor::syncVoiceCount()
{
    auto* aggregaSynth = dynamic_cast<AggregaKeysSynth*>(synth.get());
    jassert(aggregaSynth != nullptr);

    if (aggregaSynth == nullptr)
        return;

    const auto desiredVoices = juce::jlimit(1, 16, juce::roundToInt(parameters.getRawParameterValue("polyphony")->load()));
    const auto monoModeEnabled = parameters.getRawParameterValue("monoMode")->load() >= 0.5f;
    aggregaSynth->setMaximumPlayableVoices(desiredVoices);
    aggregaSynth->setMonoMode(monoModeEnabled);
}

void AggregatronKeysAudioProcessor::updateEffectParameters()
{
    juce::Reverb::Parameters params;
    params.roomSize = parameters.getRawParameterValue("reverbSize")->load();
    params.damping = parameters.getRawParameterValue("reverbDamping")->load();
    params.wetLevel = 1.0f;
    params.dryLevel = 0.0f;
    params.width = 1.0f;
    params.freezeMode = 0.0f;
    reverb.setParameters(params);
}

void AggregatronKeysAudioProcessor::captureWaveformSnapshot(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    constexpr int snapshotSize = 192;
    const auto* source = buffer.getReadPointer(0);
    std::vector<float> snapshot(static_cast<size_t>(snapshotSize), 0.0f);

    for (int i = 0; i < snapshotSize; ++i)
    {
        const auto index = juce::jmap(i, 0, snapshotSize - 1, 0, juce::jmax(0, buffer.getNumSamples() - 1));
        snapshot[static_cast<size_t>(i)] = source[index];
    }

    const juce::ScopedLock lock(waveformLock);
    recentWaveform = std::move(snapshot);
}

juce::AudioProcessorValueTreeState::ParameterLayout AggregatronKeysAudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using IntParam = juce::AudioParameterInt;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;
    layout.push_back(std::make_unique<FloatParam>("gain", "Gain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    layout.push_back(std::make_unique<FloatParam>("velocityAmp", "Velocity Amp", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.push_back(std::make_unique<FloatParam>("attack", "Attack", juce::NormalisableRange<float>(0.001f, 3.0f, 0.001f, 0.4f), 0.02f));
    layout.push_back(std::make_unique<FloatParam>("decay", "Decay", juce::NormalisableRange<float>(0.001f, 3.0f, 0.001f, 0.4f), 0.15f));
    layout.push_back(std::make_unique<FloatParam>("sustain", "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    layout.push_back(std::make_unique<FloatParam>("release", "Release", juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.4f), 0.35f));
    layout.push_back(std::make_unique<juce::AudioParameterChoice>("osc1Wave", "Osc 1 Wave", juce::StringArray { "Saw", "Square", "Triangle", "Sine" }, 0));
    layout.push_back(std::make_unique<juce::AudioParameterChoice>("osc2Wave", "Osc 2 Wave", juce::StringArray { "Saw", "Square", "Triangle", "Sine" }, 1));
    layout.push_back(std::make_unique<FloatParam>("oscMix", "Osc Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f));
    layout.push_back(std::make_unique<FloatParam>("osc2Detune", "Osc 2 Detune", juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f));
    layout.push_back(std::make_unique<FloatParam>("osc2Fine", "Osc 2 Fine", juce::NormalisableRange<float>(-50.0f, 50.0f), 0.0f));
    layout.push_back(std::make_unique<FloatParam>("filterCutoff", "Filter Cutoff", juce::NormalisableRange<float>(40.0f, 18000.0f, 1.0f, 0.25f), 4000.0f));
    layout.push_back(std::make_unique<FloatParam>("filterResonance", "Filter Resonance", juce::NormalisableRange<float>(0.1f, 1.2f), 0.3f));
    layout.push_back(std::make_unique<FloatParam>("filterEnvAmount", "Filter Env Amount", juce::NormalisableRange<float>(0.0f, 12000.0f), 2500.0f));
    layout.push_back(std::make_unique<FloatParam>("velocityFilter", "Velocity Filter", juce::NormalisableRange<float>(0.0f, 6000.0f), 0.0f));
    layout.push_back(std::make_unique<FloatParam>("filterAttack", "Filter Attack", juce::NormalisableRange<float>(0.001f, 3.0f, 0.001f, 0.4f), 0.01f));
    layout.push_back(std::make_unique<FloatParam>("filterDecay", "Filter Decay", juce::NormalisableRange<float>(0.001f, 3.0f, 0.001f, 0.4f), 0.2f));
    layout.push_back(std::make_unique<FloatParam>("filterSustain", "Filter Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.1f));
    layout.push_back(std::make_unique<FloatParam>("filterRelease", "Filter Release", juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.4f), 0.3f));
    layout.push_back(std::make_unique<FloatParam>("lfoRate", "LFO Rate", juce::NormalisableRange<float>(0.05f, 20.0f, 0.01f, 0.3f), 5.0f));
    layout.push_back(std::make_unique<FloatParam>("lfoPitchDepth", "LFO Pitch Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.push_back(std::make_unique<FloatParam>("lfoFilterDepth", "LFO Filter Depth", juce::NormalisableRange<float>(0.0f, 6000.0f), 0.0f));
    layout.push_back(std::make_unique<FloatParam>("glide", "Glide", juce::NormalisableRange<float>(0.0f, 1.5f, 0.001f, 0.35f), 0.0f));
    layout.push_back(std::make_unique<IntParam>("polyphony", "Polyphony", 1, 16, 8));
    layout.push_back(std::make_unique<juce::AudioParameterBool>("monoMode", "Mono Mode", false));
    layout.push_back(std::make_unique<FloatParam>("drive", "Drive", juce::NormalisableRange<float>(0.0f, 1.0f), 0.15f));
    layout.push_back(std::make_unique<FloatParam>("reverbMix", "Reverb Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.18f));
    layout.push_back(std::make_unique<FloatParam>("reverbSize", "Reverb Size", juce::NormalisableRange<float>(0.0f, 1.0f), 0.45f));
    layout.push_back(std::make_unique<FloatParam>("reverbDamping", "Reverb Damping", juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));

    return { layout.begin(), layout.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AggregatronKeysAudioProcessor();
}
