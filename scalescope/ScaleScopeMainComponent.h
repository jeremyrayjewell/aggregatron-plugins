#pragma once

#include <JuceHeader.h>
#include "ScaleScopeState.h"

class AggregaScaleMainComponent : public juce::Component
{
public:
    explicit AggregaScaleMainComponent(AggregaScaleState&);
    ~AggregaScaleMainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    void applyScale(int rootNote, const juce::String& scaleName);
    static juce::String pitchClassName(int pitchClass);

    AggregaScaleState& scaleState;
    juce::Rectangle<int> prevRootButton;
    juce::Rectangle<int> nextRootButton;
    juce::Rectangle<int> prevScaleButton;
    juce::Rectangle<int> nextScaleButton;
    juce::StringArray scaleNames;
    int currentScaleIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregaScaleMainComponent)
};
