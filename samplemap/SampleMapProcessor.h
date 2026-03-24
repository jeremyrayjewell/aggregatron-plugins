#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <thread>

class AggregaMapAudioProcessor : public juce::AudioProcessor
{
public:
    struct Region
    {
        int rootMidiNote = 60;
        int startSample = 0;
        int endSample = 0;
        float rms = 0.0f;
    };

    struct NoteAssignment
    {
        int rootMidiNote = 60;
        int startSample = 0;
        int endSample = 0;
        float levelCompensation = 1.0f;
        bool valid = false;
    };

    AggregaMapAudioProcessor();
    ~AggregaMapAudioProcessor() override;

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

    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    juce::String getVersionString() const { return "v0.1"; }
    float getMinSegmentMs() const noexcept { return parameters.getRawParameterValue("minSegmentMs")->load(); }
    float getMaxSegmentMs() const noexcept { return parameters.getRawParameterValue("maxSegmentMs")->load(); }
    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }
    juce::String getAnalysisSummary() const;
    bool hasLoadedSource() const noexcept;
    bool startLoadingSourceFile(const juce::File& file);
    bool isLoadingSource() const noexcept { return loadingSource.load(); }
    float getLoadingProgress() const noexcept { return loadingProgress.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    std::vector<Region> analyseSource(const juce::AudioBuffer<float>& source, double sourceSampleRate);
    void rebuildAssignments(const std::vector<Region>& regions, std::array<NoteAssignment, 128>& assignments) const;
    void setAnalysisSummary(const juce::String& summary);
    void joinLoaderThread();
    void performLoad(const juce::File& file);

    juce::AudioProcessorValueTreeState parameters;
    juce::CriticalSection synthLock;
    mutable juce::CriticalSection stateLock;
    juce::Synthesiser synth;
    juce::MidiKeyboardState keyboardState;
    juce::AudioFormatManager formatManager;
    std::shared_ptr<juce::AudioBuffer<float>> loadedBuffer;
    std::array<NoteAssignment, 128> noteAssignments {};
    std::vector<Region> detectedRegions;
    juce::String loadedFileName;
    juce::String analysisSummary { "Load a WAV or MP3 to build the note map." };
    double currentSampleRate = 44100.0;
    std::thread loaderThread;
    std::atomic<bool> loadingSource { false };
    std::atomic<float> loadingProgress { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregaMapAudioProcessor)
};

