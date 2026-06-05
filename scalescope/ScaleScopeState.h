#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <thread>

class AggregaScaleState
{
public:
    struct ScaleInfo
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

    AggregaScaleState();
    ~AggregaScaleState();

    ScaleInfo getScaleInfo() const;
    bool startLoadingSourceFile(const juce::File& file);
    bool isLoadingSource() const noexcept { return loadingSource.load(); }
    float getLoadingProgress() const noexcept { return loadingProgress.load(); }
    void applyPresetScale(int rootNote, const juce::String& scaleName);
    void applyCustomScale(int rootNote, const std::array<bool, 12>& enabledPitchClasses, const juce::String& label);

    static std::array<bool, 12> buildMaskFromTemplate(int rootNote, const ScaleTemplate& scaleTemplate);
    static const ScaleTemplate* findScaleTemplate(const juce::String& scaleName);
    static juce::String pitchClassName(int pitchClass);

private:
    void setScaleInfo(const ScaleInfo& newState);
    void joinLoaderThread();
    void performLoad(const juce::File& file);

    mutable juce::CriticalSection stateLock;
    juce::AudioFormatManager formatManager;
    ScaleInfo scaleInfo;
    std::thread loaderThread;
    std::atomic<bool> loadingSource { false };
    std::atomic<float> loadingProgress { 0.0f };
};
