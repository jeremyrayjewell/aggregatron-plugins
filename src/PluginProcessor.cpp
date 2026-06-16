#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    const auto desiredVoices = juce::jlimit(1, 16, juce::roundToInt(parameters.getRawParameterValue("polyphony")->load()));
    const auto monoModeEnabled = parameters.getRawParameterValue("monoMode")->load() >= 0.5f;
    synth->setMaximumPlayableVoices(desiredVoices);
    synth->setMonoMode(monoModeEnabled);
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
