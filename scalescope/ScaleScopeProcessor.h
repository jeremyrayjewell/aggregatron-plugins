#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <thread>

class AggregaScaleAudioProcessor : public juce::AudioProcessor
{
public:
    struct ScaleState
    {
        int rootNote = 0;
        juce::String scaleName { "Major" };
        juce::String sourceLabel { "Manual preset" };
        juce::String detailText { "Choose a root and scale, or analyse a file." };
        std::array<bool, 12> enabledPitchClasses {};
        bool derivedFromAnalysis = false;
    };

    struct ScaleTemplate
    {
        const char* name = "";
        std::array<int, 12> intervals {};
    };

    AggregaScaleAudioProcessor();
    ~AggregaScaleAudioProcessor() override;

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

    ScaleState getScaleState() const;
    juce::String getVersionString() const { return "v0.1"; }
    bool startLoadingSourceFile(const juce::File& file);
    bool isLoadingSource() const noexcept { return loadingSource.load(); }
    float getLoadingProgress() const noexcept { return loadingProgress.load(); }
    void applyPresetScale(int rootNote, const juce::String& scaleName);
    void applyCustomScale(int rootNote, const std::array<bool, 12>& enabledPitchClasses, const juce::String& label);
    static std::array<bool, 12> buildMaskFromTemplate(int rootNote, const ScaleTemplate& scaleTemplate);
    static const ScaleTemplate* findScaleTemplate(const juce::String& scaleName);
    static juce::String pitchClassName(int pitchClass);

private:
    void setScaleState(const ScaleState& newState);
    void joinLoaderThread();
    void performLoad(const juce::File& file);

    mutable juce::CriticalSection stateLock;
    juce::AudioFormatManager formatManager;
    ScaleState scaleState;
    std::thread loaderThread;
    std::atomic<bool> loadingSource { false };
    std::atomic<float> loadingProgress { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregaScaleAudioProcessor)
};
