#pragma once

#include <JuceHeader.h>

class AggregaVocoderAudioProcessor : public juce::AudioProcessor
{
public:
    struct ScaleTemplate
    {
        const char* name = "";
        std::array<int, 12> intervals {};
    };

    AggregaVocoderAudioProcessor();
    ~AggregaVocoderAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
   #endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;
    juce::String getVersionString() const { return "v0.7"; }

    static juce::String pitchClassName(int pitchClass);
    static const std::array<ScaleTemplate, 11>& getScaleTemplates();
    float getDetectedPitchHz() const noexcept { return detectedPitchHz.load(); }
    int getTargetMidiNote() const noexcept { return targetMidiNote.load(); }

private:
    static constexpr int analysisWindowSize = 1024;
    static constexpr size_t numBands = 12;

    struct CarrierState
    {
        float phaseA = 0.0f;
        float phaseB = 0.0f;
        float phaseSub = 0.0f;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static float estimatePitchHz(const float* samples, int length, double sampleRate);
    static std::array<bool, 12> buildMaskFromTemplate(int rootNote, const ScaleTemplate& scaleTemplate);
    static float wrapPhase(float phase) noexcept;
    int findNearestAllowedMidiNote(float midiNote) const;
    void updateScaleMask();
    void updateBandFrequencies();
    float renderCarrierSample(CarrierState& state, float frequency, float tone, float noiseMix, float harmonyMix, int harmonySemitones) noexcept;

    juce::AudioBuffer<float> analysisBuffer;
    std::array<bool, 12> allowedPitchClasses {};
    std::array<float, numBands> bandFrequencies {};
    std::array<float, numBands> envelopes {};
    std::array<juce::dsp::StateVariableTPTFilter<float>, numBands> modulatorFilters;
    std::array<juce::dsp::StateVariableTPTFilter<float>, numBands> carrierFiltersLeft;
    std::array<juce::dsp::StateVariableTPTFilter<float>, numBands> carrierFiltersRight;
    CarrierState leftCarrier;
    CarrierState rightCarrier;
    double currentSampleRate = 44100.0;
    int analysisWritePosition = 0;
    float smoothedCarrierFrequency = 110.0f;
    std::atomic<float> detectedPitchHz { 0.0f };
    std::atomic<int> targetMidiNote { 60 };
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregaVocoderAudioProcessor)
};
