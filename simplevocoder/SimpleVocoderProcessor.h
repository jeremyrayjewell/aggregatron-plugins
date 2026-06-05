#pragma once

#include <JuceHeader.h>

class SimpleVocoderAudioProcessor : public juce::AudioProcessor
{
public:
    SimpleVocoderAudioProcessor();
    ~SimpleVocoderAudioProcessor() override = default;

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
    float getEnvelopeAmount() const noexcept { return envelopeMeter.load(); }
    float getDetectedPitchHz() const noexcept { return detectedPitchHz.load(); }
    int getTargetMidiNote() const noexcept { return targetMidiNote.load(); }
    static juce::String pitchClassName(int pitchClass);

private:
    static constexpr size_t numBands = 8;
    static constexpr int analysisWindowSize = 1024;

    struct CarrierState
    {
        float phase = 0.0f;
        float subPhase = 0.0f;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static float estimatePitchHz(const float* samples, int length, double sampleRate);
    static float wrapPhase(float phase) noexcept;
    static int findNearestAllowedMidiNote(float midiNote);
    void updateBandFrequencies();
    float renderCarrierSample(CarrierState& state, float frequency, float tone) noexcept;

    juce::AudioBuffer<float> analysisBuffer;
    std::array<float, numBands> bandFrequencies {};
    std::array<float, numBands> envelopes {};
    std::array<juce::dsp::StateVariableTPTFilter<float>, numBands> modulatorFilters;
    std::array<juce::dsp::StateVariableTPTFilter<float>, numBands> carrierFiltersLeft;
    std::array<juce::dsp::StateVariableTPTFilter<float>, numBands> carrierFiltersRight;
    CarrierState leftCarrier;
    CarrierState rightCarrier;
    double currentSampleRate = 44100.0;
    int analysisWritePosition = 0;
    float smoothedCarrierFrequency = 146.83f;
    std::atomic<float> envelopeMeter { 0.0f };
    std::atomic<float> detectedPitchHz { 0.0f };
    std::atomic<int> targetMidiNote { 62 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleVocoderAudioProcessor)
};
