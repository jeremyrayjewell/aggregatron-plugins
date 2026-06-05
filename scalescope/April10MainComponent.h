#pragma once

#include <JuceHeader.h>
#include "ScaleScopeState.h"

class April10MainComponent : public juce::Component
{
public:
    struct TrackPreset
    {
        const char* title = "";
        int bpm = 120;
        int rootNote = 0;
        const char* scaleName = "";
        const char* displayScale = "";
    };

    explicit April10MainComponent(AggregaScaleState&);
    ~April10MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    void applyCurrentPreset();
    static juce::String pitchClassName(int pitchClass);

    AggregaScaleState& scaleState;
    juce::Rectangle<int> prevTrackButton;
    juce::Rectangle<int> nextTrackButton;
    int currentTrackIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(April10MainComponent)
};
