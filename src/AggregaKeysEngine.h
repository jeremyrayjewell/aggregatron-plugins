#pragma once

#include "AggregaKeysParameters.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <vector>

class SynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override;
    bool appliesToChannel(int) override;
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

    explicit SynthVoice(const AggregaKeysParameterValues& values);

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void setCurrentPlaybackSampleRate(double newRate) override;
    void setLegatoMode(bool shouldUseLegato) noexcept;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote(float, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    static float wrapPhase(float phase);
    void updatePitch();
    void prepareSmoothers();
    void updateParameters(const AggregaKeysParameterValues& values);
    void updateGlideTarget(bool shouldGlide);
    static float renderOscillatorSample(Waveform waveform, float phase, float phaseIncrement) noexcept;

    const AggregaKeysParameterValues& parameterValues;
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
    AggregaKeysSynth() = default;

    void setParameterValues(const AggregaKeysParameterValues& values);
    const AggregaKeysParameterValues& getParameterValues() const noexcept { return parameterValues; }
    void setMaximumPlayableVoices(int newMaxVoices) noexcept;
    void setMonoMode(bool shouldBeMono);
    void noteOn(int midiChannel, int midiNoteNumber, float velocity) override;
    void noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override;

protected:
    juce::SynthesiserVoice* findFreeVoice(juce::SynthesiserSound* soundToPlay,
                                          int midiChannel,
                                          int midiNoteNumber,
                                          bool stealIfNoneAvailable) const override;

private:
    struct HeldNote
    {
        int midiChannel = 1;
        int midiNoteNumber = 60;
        float velocity = 1.0f;
    };

    void stopExtraVoices();
    void updateHeldNote(int midiChannel, int midiNoteNumber, float velocity);
    void removeHeldNote(int midiChannel, int midiNoteNumber);
    const HeldNote* getLastHeldNote() const noexcept;
    juce::SynthesiserSound* findSoundFor(int midiChannel, int midiNoteNumber) const;
    SynthVoice* getPrimaryVoice() const;

    std::vector<HeldNote> heldNotes;
    AggregaKeysParameterValues parameterValues {};
    int maximumPlayableVoices = 8;
    bool monoMode = false;
};
