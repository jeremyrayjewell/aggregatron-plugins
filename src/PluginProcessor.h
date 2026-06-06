#pragma once

#include <JuceHeader.h>
#include <vector>
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
    void appendDebugLog(const juce::String& s) noexcept;
    std::vector<juce::String> drainDebugLog() noexcept;
    void setUiPitchWheel(int value) noexcept;
    void setUiModWheel(float normalizedValue) noexcept;
    int getUiPitchWheel() const noexcept { return uiPitchWheelValue.load(); }
    float getUiModWheelNormalized() const noexcept { return static_cast<float>(uiModWheelValue.load()) / 127.0f; }
    juce::String getVersionString() const { return "v0.3"; }
    bool savePresetToFile(const juce::File& file);
    bool loadPresetFromFile(const juce::File& file);
    bool getWaveformSnapshot(std::vector<float>& dest) const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void enforceMinimumMidiNoteDurations(juce::MidiBuffer& midiMessages, int numSamples);
    void syncVoiceCount();
    void updateEffectParameters();
    void captureWaveformSnapshot(const juce::AudioBuffer<float>& buffer);

    std::unique_ptr<juce::Synthesiser> synth;
    juce::MidiKeyboardState keyboardState;
    juce::Reverb reverb;
    juce::AudioBuffer<float> synthBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::SmoothedValue<float> gainSmoothed;
    juce::SmoothedValue<float> reverbMixSmoothed;
    mutable juce::CriticalSection waveformLock;
    std::vector<float> recentWaveform;
    double currentSampleRate = 44100.0;
    std::atomic<int> uiPitchWheelValue { 8192 };
    std::atomic<int> lastPitchWheelValue { 8192 };
    std::atomic<int> uiModWheelValue { 0 };
    std::atomic<int> lastModWheelValue { 0 };
    std::array<int, 16 * 128> deferredNoteOffSamples {};

    juce::CriticalSection debugLogLock;
    std::vector<juce::String> debugLog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregatronKeysAudioProcessor)
};
