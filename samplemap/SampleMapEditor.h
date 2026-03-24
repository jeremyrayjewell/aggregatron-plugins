#pragma once

#include "SampleMapProcessor.h"

class OutlinedMapLabel : public juce::Label
{
public:
    void paint(juce::Graphics& g) override;
};

class AggregaMapAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer,
                                       private juce::KeyListener
{
public:
    explicit AggregaMapAudioProcessorEditor(AggregaMapAudioProcessor&);
    ~AggregaMapAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override;
    bool keyStateChanged(bool isKeyDown, juce::Component*) override;
    void timerCallback() override;
    void releaseStaleHeldKeys();

    AggregaMapAudioProcessor& audioProcessor;
    juce::Image backgroundImage;
    juce::Font titleFont;
    juce::Font bodyFont;
    OutlinedMapLabel titleLabel;
    OutlinedMapLabel hintLabel;
    OutlinedMapLabel versionLabel;
    OutlinedMapLabel statusLabel;
    juce::Label minSegmentLabel;
    juce::Label maxSegmentLabel;
    juce::TextButton loadButton { "Load Source" };
    juce::ToggleButton levelMatchToggle { "Match Levels" };
    juce::Slider minSegmentSlider;
    juce::Slider maxSegmentSlider;
    juce::MidiKeyboardComponent keyboardComponent { audioProcessor.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> levelMatchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> minSegmentAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> maxSegmentAttachment;
    std::map<int, int> heldKeys;
    juce::String lastStatusText;
    bool wasLoading = false;
    int lastProgressPercent = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregaMapAudioProcessorEditor)
};
