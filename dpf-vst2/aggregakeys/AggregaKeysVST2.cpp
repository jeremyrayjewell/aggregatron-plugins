#include "DistrhoPlugin.hpp"

#include "AggregaKeysEngine.h"
#include "AggregaKeysParameters.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cstdint>
#include <cstring>

START_NAMESPACE_DISTRHO

namespace
{
struct ParameterDefinition {
    const char* symbol;
    const char* name;
    float minimum;
    float maximum;
    float defaultValue;
    uint32_t hints;
};

constexpr ParameterDefinition kParameterDefinitions[kParameterCount] = {
    { "gain", "Gain", 0.0f, 1.0f, 0.7f, kParameterIsAutomatable },
    { "velocityAmp", "Velocity Amp", 0.0f, 1.0f, 0.0f, kParameterIsAutomatable },
    { "attack", "Attack", 0.001f, 3.0f, 0.02f, kParameterIsAutomatable },
    { "decay", "Decay", 0.001f, 3.0f, 0.15f, kParameterIsAutomatable },
    { "sustain", "Sustain", 0.0f, 1.0f, 0.8f, kParameterIsAutomatable },
    { "release", "Release", 0.001f, 5.0f, 0.35f, kParameterIsAutomatable },
    { "osc1Wave", "Osc 1 Wave", 0.0f, 3.0f, 0.0f, kParameterIsAutomatable | kParameterIsInteger },
    { "osc2Wave", "Osc 2 Wave", 0.0f, 3.0f, 1.0f, kParameterIsAutomatable | kParameterIsInteger },
    { "oscMix", "Osc Mix", 0.0f, 1.0f, 0.35f, kParameterIsAutomatable },
    { "osc2Detune", "Osc 2 Detune", -12.0f, 12.0f, 0.0f, kParameterIsAutomatable },
    { "osc2Fine", "Osc 2 Fine", -50.0f, 50.0f, 0.0f, kParameterIsAutomatable },
    { "filterCutoff", "Filter Cutoff", 40.0f, 18000.0f, 4000.0f, kParameterIsAutomatable },
    { "filterResonance", "Filter Resonance", 0.1f, 1.2f, 0.3f, kParameterIsAutomatable },
    { "filterEnvAmount", "Filter Env Amount", 0.0f, 12000.0f, 2500.0f, kParameterIsAutomatable },
    { "velocityFilter", "Velocity Filter", 0.0f, 6000.0f, 0.0f, kParameterIsAutomatable },
    { "filterAttack", "Filter Attack", 0.001f, 3.0f, 0.01f, kParameterIsAutomatable },
    { "filterDecay", "Filter Decay", 0.001f, 3.0f, 0.2f, kParameterIsAutomatable },
    { "filterSustain", "Filter Sustain", 0.0f, 1.0f, 0.1f, kParameterIsAutomatable },
    { "filterRelease", "Filter Release", 0.001f, 5.0f, 0.3f, kParameterIsAutomatable },
    { "lfoRate", "LFO Rate", 0.05f, 20.0f, 5.0f, kParameterIsAutomatable },
    { "lfoPitchDepth", "LFO Pitch Depth", 0.0f, 1.0f, 0.0f, kParameterIsAutomatable },
    { "lfoFilterDepth", "LFO Filter Depth", 0.0f, 6000.0f, 0.0f, kParameterIsAutomatable },
    { "glide", "Glide", 0.0f, 1.5f, 0.0f, kParameterIsAutomatable },
    { "polyphony", "Polyphony", 1.0f, 16.0f, 8.0f, kParameterIsAutomatable | kParameterIsInteger },
    { "monoMode", "Mono Mode", 0.0f, 1.0f, 0.0f, kParameterIsAutomatable | kParameterIsBoolean },
    { "drive", "Drive", 0.0f, 1.0f, 0.15f, kParameterIsAutomatable },
    { "reverbMix", "Reverb Mix", 0.0f, 1.0f, 0.18f, kParameterIsAutomatable },
    { "reverbSize", "Reverb Size", 0.0f, 1.0f, 0.45f, kParameterIsAutomatable },
    { "reverbDamping", "Reverb Damping", 0.0f, 1.0f, 0.3f, kParameterIsAutomatable }
};

ParameterEnumerationValue* createWaveEnumValues()
{
    auto* values = new ParameterEnumerationValue[4];
    values[0] = { 0.0f, "Saw" };
    values[1] = { 1.0f, "Square" };
    values[2] = { 2.0f, "Triangle" };
    values[3] = { 3.0f, "Sine" };
    return values;
}
} // namespace

class AggregaKeysDPFVST2Plugin : public Plugin
{
public:
    AggregaKeysDPFVST2Plugin()
        : Plugin(kParameterCount, 0, 0)
    {
        synth.addSound(new SynthSound());
        for (int i = 0; i < 16; ++i)
            synth.addVoice(new SynthVoice(synth.getParameterValues()));

        synth.setNoteStealingEnabled(true);
        sampleRateChanged(getSampleRate());
        bufferSizeChanged(getBufferSize());
    }

protected:
    const char* getLabel() const override
    {
        return "AggregaKeysDPFVST2";
    }

    const char* getDescription() const override
    {
        return "Headless DPF VST2 build of the real AggregaKeys synth engine.";
    }

    const char* getMaker() const override
    {
        return "Aggregatron";
    }

    const char* getHomePage() const override
    {
        return "";
    }

    const char* getLicense() const override
    {
        return "See repository and DPF notices";
    }

    uint32_t getVersion() const override
    {
        return d_version(0, 9, 0);
    }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        const auto& definition = kParameterDefinitions[index];
        parameter.hints = definition.hints;
        parameter.name = definition.name;
        parameter.symbol = definition.symbol;
        parameter.ranges.min = definition.minimum;
        parameter.ranges.max = definition.maximum;
        parameter.ranges.def = definition.defaultValue;

        if (index == kParameterOsc1Wave || index == kParameterOsc2Wave)
        {
            parameter.enumValues.count = 4;
            parameter.enumValues.restrictedMode = true;
            parameter.enumValues.values = createWaveEnumValues();
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        switch (index)
        {
        case kParameterGain: return parameters.gain;
        case kParameterVelocityAmp: return parameters.velocityAmp;
        case kParameterAttack: return parameters.attack;
        case kParameterDecay: return parameters.decay;
        case kParameterSustain: return parameters.sustain;
        case kParameterRelease: return parameters.release;
        case kParameterOsc1Wave: return static_cast<float>(parameters.osc1Wave);
        case kParameterOsc2Wave: return static_cast<float>(parameters.osc2Wave);
        case kParameterOscMix: return parameters.oscMix;
        case kParameterOsc2Detune: return parameters.osc2Detune;
        case kParameterOsc2Fine: return parameters.osc2Fine;
        case kParameterFilterCutoff: return parameters.filterCutoff;
        case kParameterFilterResonance: return parameters.filterResonance;
        case kParameterFilterEnvAmount: return parameters.filterEnvAmount;
        case kParameterVelocityFilter: return parameters.velocityFilter;
        case kParameterFilterAttack: return parameters.filterAttack;
        case kParameterFilterDecay: return parameters.filterDecay;
        case kParameterFilterSustain: return parameters.filterSustain;
        case kParameterFilterRelease: return parameters.filterRelease;
        case kParameterLfoRate: return parameters.lfoRate;
        case kParameterLfoPitchDepth: return parameters.lfoPitchDepth;
        case kParameterLfoFilterDepth: return parameters.lfoFilterDepth;
        case kParameterGlide: return parameters.glide;
        case kParameterPolyphony: return static_cast<float>(parameters.polyphony);
        case kParameterMonoMode: return parameters.monoMode ? 1.0f : 0.0f;
        case kParameterDrive: return parameters.drive;
        case kParameterReverbMix: return parameters.reverbMix;
        case kParameterReverbSize: return parameters.reverbSize;
        case kParameterReverbDamping: return parameters.reverbDamping;
        default: return 0.0f;
        }
    }

    void setParameterValue(uint32_t index, float value) override
    {
        switch (index)
        {
        case kParameterGain: parameters.gain = value; break;
        case kParameterVelocityAmp: parameters.velocityAmp = value; break;
        case kParameterAttack: parameters.attack = value; break;
        case kParameterDecay: parameters.decay = value; break;
        case kParameterSustain: parameters.sustain = value; break;
        case kParameterRelease: parameters.release = value; break;
        case kParameterOsc1Wave: parameters.osc1Wave = clampInt(value, 0, 3); break;
        case kParameterOsc2Wave: parameters.osc2Wave = clampInt(value, 0, 3); break;
        case kParameterOscMix: parameters.oscMix = value; break;
        case kParameterOsc2Detune: parameters.osc2Detune = value; break;
        case kParameterOsc2Fine: parameters.osc2Fine = value; break;
        case kParameterFilterCutoff: parameters.filterCutoff = value; break;
        case kParameterFilterResonance: parameters.filterResonance = value; break;
        case kParameterFilterEnvAmount: parameters.filterEnvAmount = value; break;
        case kParameterVelocityFilter: parameters.velocityFilter = value; break;
        case kParameterFilterAttack: parameters.filterAttack = value; break;
        case kParameterFilterDecay: parameters.filterDecay = value; break;
        case kParameterFilterSustain: parameters.filterSustain = value; break;
        case kParameterFilterRelease: parameters.filterRelease = value; break;
        case kParameterLfoRate: parameters.lfoRate = value; break;
        case kParameterLfoPitchDepth: parameters.lfoPitchDepth = value; break;
        case kParameterLfoFilterDepth: parameters.lfoFilterDepth = value; break;
        case kParameterGlide: parameters.glide = value; break;
        case kParameterPolyphony: parameters.polyphony = clampInt(value, 1, 16); break;
        case kParameterMonoMode: parameters.monoMode = value >= 0.5f; break;
        case kParameterDrive: parameters.drive = value; break;
        case kParameterReverbMix: parameters.reverbMix = value; break;
        case kParameterReverbSize: parameters.reverbSize = value; break;
        case kParameterReverbDamping: parameters.reverbDamping = value; break;
        default: break;
        }
    }

    void activate() override
    {
        synth.allNotesOff(0, false);
        reverb.reset();
        synth.setParameterValues(parameters);
        gainSmoothed.setCurrentAndTargetValue(parameters.gain);
        reverbMixSmoothed.setCurrentAndTargetValue(parameters.reverbMix);
    }

    void bufferSizeChanged(uint32_t newBufferSize) override
    {
        ensureWorkingCapacity(newBufferSize);
    }

    void sampleRateChanged(double newSampleRate) override
    {
        currentSampleRate = newSampleRate > 1.0 ? newSampleRate : 44100.0;
        synth.setCurrentPlaybackSampleRate(currentSampleRate);
        gainSmoothed.reset(currentSampleRate, 0.02);
        reverbMixSmoothed.reset(currentSampleRate, 0.02);
        gainSmoothed.setCurrentAndTargetValue(parameters.gain);
        reverbMixSmoothed.setCurrentAndTargetValue(parameters.reverbMix);
        updateEffectParameters(parameters);
    }

    void run(const float**, float** outputs, uint32_t frames,
             const MidiEvent* midiEvents, uint32_t midiEventCount) override
    {
        if (frames == 0)
            return;

        ensureWorkingCapacity(frames);

        std::memset(outputs[0], 0, sizeof(float) * frames);
        std::memset(outputs[1], 0, sizeof(float) * frames);

        midiBuffer.clear();
        fillMidiBuffer(midiEvents, midiEventCount, frames);

        synth.setParameterValues(parameters);
        updateEffectParameters(parameters);
        gainSmoothed.setTargetValue(parameters.gain);
        reverbMixSmoothed.setTargetValue(parameters.reverbMix);

        synthBuffer.clear();
        synth.renderNextBlock(synthBuffer, midiBuffer, 0, static_cast<int>(frames));

        wetBuffer.copyFrom(0, 0, synthBuffer, 0, 0, static_cast<int>(frames));
        wetBuffer.copyFrom(1, 0, synthBuffer, 1, 0, static_cast<int>(frames));
        reverb.processStereo(wetBuffer.getWritePointer(0), wetBuffer.getWritePointer(1), static_cast<int>(frames));

        for (uint32_t sample = 0; sample < frames; ++sample)
        {
            const auto gain = gainSmoothed.getNextValue();
            const auto wetMix = reverbMixSmoothed.getNextValue();
            const auto dryMix = 1.0f - wetMix;
            const auto left = synthBuffer.getSample(0, static_cast<int>(sample)) * dryMix
                            + wetBuffer.getSample(0, static_cast<int>(sample)) * wetMix;
            const auto right = synthBuffer.getSample(1, static_cast<int>(sample)) * dryMix
                             + wetBuffer.getSample(1, static_cast<int>(sample)) * wetMix;

            outputs[0][sample] = left * gain;
            outputs[1][sample] = right * gain;
        }
    }

private:
    static int clampInt(float value, int minimum, int maximum) noexcept
    {
        return juce::jlimit(minimum, maximum, juce::roundToInt(value));
    }

    void ensureWorkingCapacity(uint32_t frames)
    {
        if (frames <= bufferCapacity)
            return;

        bufferCapacity = frames;
        synthBuffer.setSize(2, static_cast<int>(bufferCapacity), false, false, true);
        wetBuffer.setSize(2, static_cast<int>(bufferCapacity), false, false, true);
        midiBuffer.ensureSize(static_cast<int>(juce::jmax<uint32_t>(4096u, bufferCapacity * 64u)));
    }

    void updateEffectParameters(const AggregaKeysParameterValues& values)
    {
        juce::Reverb::Parameters reverbParameters;
        reverbParameters.roomSize = values.reverbSize;
        reverbParameters.damping = values.reverbDamping;
        reverbParameters.wetLevel = 1.0f;
        reverbParameters.dryLevel = 0.0f;
        reverbParameters.width = 1.0f;
        reverbParameters.freezeMode = 0.0f;
        reverb.setParameters(reverbParameters);
    }

    void fillMidiBuffer(const MidiEvent* midiEvents, uint32_t midiEventCount, uint32_t frames)
    {
        const int maxFrameIndex = static_cast<int>(frames - 1u);

        for (uint32_t index = 0; index < midiEventCount; ++index)
        {
            const auto& event = midiEvents[index];
            if (event.size == 0)
                continue;

            const uint8_t* const data = event.size > MidiEvent::kDataSize && event.dataExt != nullptr
                ? event.dataExt
                : event.data;
            if (data == nullptr)
                continue;

            const int sampleOffset = juce::jlimit(0, maxFrameIndex, static_cast<int>(event.frame));
            midiBuffer.addEvent(data, static_cast<int>(event.size), sampleOffset);
        }
    }

    AggregaKeysParameterValues parameters {};
    AggregaKeysSynth synth;
    juce::Reverb reverb;
    juce::AudioBuffer<float> synthBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::MidiBuffer midiBuffer;
    juce::SmoothedValue<float> gainSmoothed;
    juce::SmoothedValue<float> reverbMixSmoothed;
    double currentSampleRate = 44100.0;
    uint32_t bufferCapacity = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregaKeysDPFVST2Plugin)
};

Plugin* createPlugin()
{
    return new AggregaKeysDPFVST2Plugin();
}

END_NAMESPACE_DISTRHO
