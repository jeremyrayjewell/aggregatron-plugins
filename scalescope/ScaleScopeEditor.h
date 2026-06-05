#pragma once

#include "ScaleScopeProcessor.h"

class AggregaScaleAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit AggregaScaleAudioProcessorEditor(AggregaScaleAudioProcessor&);
    ~AggregaScaleAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ScaleKeyboardComponent;

    void timerCallback() override;
    void refreshFromProcessor();
    void applyCurrentPreset();
    void applyCurrentCustomMask();
    std::array<bool, 12> collectCustomMask() const;
    void syncCustomButtons(const std::array<bool, 12>& enabledPitchClasses);
    static juce::String pitchClassName(int pitchClass);

    AggregaScaleAudioProcessor& audioProcessor;
    juce::Font titleFont { juce::FontOptions(29.0f).withStyle("Bold") };
    juce::Font bodyFont { juce::FontOptions(16.0f) };
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label sourceLabel;
    juce::Label detailLabel;
    juce::Label manualLabel;
    juce::Label customLabel;
    juce::TextButton loadButton { "Analyse Audio File" };
    juce::ComboBox rootCombo;
    juce::ComboBox scaleCombo;
    std::array<std::unique_ptr<juce::ToggleButton>, 12> noteButtons;
    std::unique_ptr<ScaleKeyboardComponent> keyboardComponent;
    AggregaScaleAudioProcessor::ScaleState lastState;
    bool suppressManualCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregaScaleAudioProcessorEditor)
};
