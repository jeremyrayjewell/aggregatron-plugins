#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
double noteNumberToFrequency(double noteNumber)
{
    return 440.0 * std::pow(2.0, (noteNumber - 69.0) / 12.0);
}

float sawWave(float phase)
{
    return (phase / juce::MathConstants<float>::pi) - 1.0f;
}

float squareWave(float phase)
{
    return phase < 0.0f ? -1.0f : 1.0f;
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
    explicit SynthVoice(juce::AudioProcessorValueTreeState& state)
        : parameters(state)
    {
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SynthSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        currentMidiNote = midiNoteNumber;
        level = velocity;
        osc1Phase = 0.0f;
        osc2Phase = 0.0f;
        lfoPhase = 0.0f;
        filter.reset();
        updatePitch();
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

        if (! isVoiceActive())
            return;

        auto* left = outputBuffer.getWritePointer(0);
        auto* right = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer(1) : nullptr;

        while (--numSamples >= 0)
        {
            const auto lfoValue = std::sin(lfoPhase);
            const auto vibratoSemitones = lfoValue * (lfoDepthSemitones + (modWheelValue * 0.35f));
            const auto noteWithMod = static_cast<double>(currentMidiNote) + static_cast<double>(pitchBendSemitones + vibratoSemitones);
            const auto osc1Frequency = noteNumberToFrequency(noteWithMod);
            const auto osc2Frequency = noteNumberToFrequency(noteWithMod + static_cast<double>(osc2DetuneSemitones) + static_cast<double>(osc2FineCents / 100.0f));

            osc1Delta = static_cast<float>(juce::MathConstants<double>::twoPi * osc1Frequency / getSampleRate());
            osc2Delta = static_cast<float>(juce::MathConstants<double>::twoPi * osc2Frequency / getSampleRate());

            const auto osc1Sample = sawWave(osc1Phase);
            const auto osc2Sample = squareWave(std::sin(osc2Phase));
            auto sample = juce::jmap(oscMix, osc1Sample, osc2Sample);

            osc1Phase = wrapPhase(osc1Phase + osc1Delta);
            osc2Phase = wrapPhase(osc2Phase + osc2Delta);
            lfoPhase = wrapPhase(lfoPhase + static_cast<float>(juce::MathConstants<double>::twoPi * lfoRateHz / getSampleRate()));

            const auto filterEnvValue = filterEnvelope.getNextSample();
            const auto cutoffMod = (filterEnvValue * filterEnvAmountHz) + (lfoValue * lfoFilterAmountHz);
            const auto cutoff = juce::jlimit(20.0f, 18000.0f, filterCutoffHz + cutoffMod);
            filter.setCutoffFrequency(cutoff);
            filter.setResonance(filterResonance);

            sample = filter.processSample(0, sample);
            sample = std::tanh(sample * driveAmount);
            sample *= ampEnvelope.getNextSample() * level;

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
        while (phase > juce::MathConstants<float>::pi)
            phase -= juce::MathConstants<float>::twoPi;

        while (phase < -juce::MathConstants<float>::pi)
            phase += juce::MathConstants<float>::twoPi;

        return phase;
    }

    void updatePitch()
    {
        const auto normalizedWheel = (static_cast<float>(pitchWheelValue) - 8192.0f) / 8192.0f;
        pitchBendSemitones = normalizedWheel * pitchBendRangeSemitones;
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

        oscMix = parameters.getRawParameterValue("oscMix")->load();
        osc2DetuneSemitones = parameters.getRawParameterValue("osc2Detune")->load();
        osc2FineCents = parameters.getRawParameterValue("osc2Fine")->load();
        filterCutoffHz = parameters.getRawParameterValue("filterCutoff")->load();
        filterResonance = parameters.getRawParameterValue("filterResonance")->load();
        filterEnvAmountHz = parameters.getRawParameterValue("filterEnvAmount")->load();
        lfoRateHz = parameters.getRawParameterValue("lfoRate")->load();
        lfoDepthSemitones = parameters.getRawParameterValue("lfoPitchDepth")->load();
        lfoFilterAmountHz = parameters.getRawParameterValue("lfoFilterDepth")->load();
        driveAmount = juce::jmap(parameters.getRawParameterValue("drive")->load(), 1.0f, 8.0f);
    }

    juce::AudioProcessorValueTreeState& parameters;
    juce::ADSR ampEnvelope;
    juce::ADSR filterEnvelope;
    juce::dsp::StateVariableTPTFilter<float> filter;
    float osc1Phase = 0.0f;
    float osc2Phase = 0.0f;
    float osc1Delta = 0.0f;
    float osc2Delta = 0.0f;
    float level = 0.0f;
    float lfoPhase = 0.0f;
    float pitchBendSemitones = 0.0f;
    float modWheelValue = 0.0f;
    float oscMix = 0.35f;
    float osc2DetuneSemitones = 0.0f;
    float osc2FineCents = 0.0f;
    float filterCutoffHz = 4000.0f;
    float filterResonance = 0.3f;
    float filterEnvAmountHz = 2000.0f;
    float lfoRateHz = 5.0f;
    float lfoDepthSemitones = 0.0f;
    float lfoFilterAmountHz = 0.0f;
    float driveAmount = 1.0f;
    int currentMidiNote = 60;
    int pitchWheelValue = 8192;

    static constexpr float pitchBendRangeSemitones = 2.0f;
};
} // namespace

AggregatronKeysAudioProcessor::AggregatronKeysAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    syncVoiceCount();
    synth.addSound(new SynthSound());
}

void AggregatronKeysAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate(sampleRate);
    synthBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
    reverb.reset();
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
    synthBuffer.clear();
    synth.renderNextBlock(synthBuffer, midiMessages, 0, buffer.getNumSamples());

    const auto gain = parameters.getRawParameterValue("gain")->load();
    const auto reverbMix = parameters.getRawParameterValue("reverbMix")->load();
    const auto dryMix = 1.0f - reverbMix;

    juce::AudioBuffer<float> wetBuffer;
    wetBuffer.makeCopyOf(synthBuffer, true);

    reverb.processStereo(wetBuffer.getWritePointer(0), wetBuffer.getWritePointer(1), wetBuffer.getNumSamples());

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        buffer.copyFrom(channel, 0, synthBuffer, channel, 0, buffer.getNumSamples());
        buffer.applyGain(channel, 0, buffer.getNumSamples(), dryMix * gain);
        buffer.addFrom(channel, 0, wetBuffer, channel, 0, buffer.getNumSamples(), reverbMix * gain);
    }
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

void AggregatronKeysAudioProcessor::syncVoiceCount()
{
    const auto desiredVoices = juce::jlimit(1, 16, juce::roundToInt(parameters.getRawParameterValue("polyphony")->load()));

    if (desiredVoices == currentVoiceCount)
        return;

    synth.clearVoices();
    for (int i = 0; i < desiredVoices; ++i)
        synth.addVoice(new SynthVoice(parameters));

    currentVoiceCount = desiredVoices;
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

juce::AudioProcessorValueTreeState::ParameterLayout AggregatronKeysAudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using IntParam = juce::AudioParameterInt;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;
    layout.push_back(std::make_unique<FloatParam>("gain", "Gain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    layout.push_back(std::make_unique<FloatParam>("attack", "Attack", juce::NormalisableRange<float>(0.001f, 3.0f, 0.001f, 0.4f), 0.02f));
    layout.push_back(std::make_unique<FloatParam>("decay", "Decay", juce::NormalisableRange<float>(0.001f, 3.0f, 0.001f, 0.4f), 0.15f));
    layout.push_back(std::make_unique<FloatParam>("sustain", "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    layout.push_back(std::make_unique<FloatParam>("release", "Release", juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.4f), 0.35f));
    layout.push_back(std::make_unique<FloatParam>("oscMix", "Osc Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f));
    layout.push_back(std::make_unique<FloatParam>("osc2Detune", "Osc 2 Detune", juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f));
    layout.push_back(std::make_unique<FloatParam>("osc2Fine", "Osc 2 Fine", juce::NormalisableRange<float>(-50.0f, 50.0f), 0.0f));
    layout.push_back(std::make_unique<FloatParam>("filterCutoff", "Filter Cutoff", juce::NormalisableRange<float>(40.0f, 18000.0f, 1.0f, 0.25f), 4000.0f));
    layout.push_back(std::make_unique<FloatParam>("filterResonance", "Filter Resonance", juce::NormalisableRange<float>(0.1f, 1.2f), 0.3f));
    layout.push_back(std::make_unique<FloatParam>("filterEnvAmount", "Filter Env Amount", juce::NormalisableRange<float>(0.0f, 12000.0f), 2500.0f));
    layout.push_back(std::make_unique<FloatParam>("filterAttack", "Filter Attack", juce::NormalisableRange<float>(0.001f, 3.0f, 0.001f, 0.4f), 0.01f));
    layout.push_back(std::make_unique<FloatParam>("filterDecay", "Filter Decay", juce::NormalisableRange<float>(0.001f, 3.0f, 0.001f, 0.4f), 0.2f));
    layout.push_back(std::make_unique<FloatParam>("filterSustain", "Filter Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.1f));
    layout.push_back(std::make_unique<FloatParam>("filterRelease", "Filter Release", juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.4f), 0.3f));
    layout.push_back(std::make_unique<FloatParam>("lfoRate", "LFO Rate", juce::NormalisableRange<float>(0.05f, 20.0f, 0.01f, 0.3f), 5.0f));
    layout.push_back(std::make_unique<FloatParam>("lfoPitchDepth", "LFO Pitch Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.push_back(std::make_unique<FloatParam>("lfoFilterDepth", "LFO Filter Depth", juce::NormalisableRange<float>(0.0f, 6000.0f), 0.0f));
    layout.push_back(std::make_unique<IntParam>("polyphony", "Polyphony", 1, 16, 8));
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
