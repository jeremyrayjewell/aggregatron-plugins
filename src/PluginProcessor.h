#pragma once

#include <JuceHeader.h>

class AggregatronKeysAudioProcessor : public juce::AudioProcessor
{
public:
    AggregatronKeysAudioProcessor();
    ~AggregatronKeysAudioProcessor() override = default;

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
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    void setUiPitchWheel(int value) noexcept;
    void setUiModWheel(float normalizedValue) noexcept;
    int getUiPitchWheel() const noexcept { return uiPitchWheelValue.load(); }
    float getUiModWheelNormalized() const noexcept { return static_cast<float>(uiModWheelValue.load()) / 127.0f; }
    juce::String getVersionString() const { return "v0.2"; }
    bool savePresetToFile(const juce::File& file);
    bool loadPresetFromFile(const juce::File& file);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void syncVoiceCount();
    void updateEffectParameters();

    juce::Synthesiser synth;
    juce::MidiKeyboardState keyboardState;
    juce::Reverb reverb;
    juce::AudioBuffer<float> synthBuffer;
    double currentSampleRate = 44100.0;
    int currentVoiceCount = 0;
    std::atomic<int> uiPitchWheelValue { 8192 };
    std::atomic<int> lastPitchWheelValue { 8192 };
    std::atomic<int> uiModWheelValue { 0 };
    std::atomic<int> lastModWheelValue { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregatronKeysAudioProcessor)
};
