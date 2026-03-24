#pragma once

#include "PluginProcessor.h"

class OutlinedLabel : public juce::Label
{
public:
    void paint(juce::Graphics& g) override;
};

class AggregatronKeysAudioProcessorEditor : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit AggregatronKeysAudioProcessorEditor(AggregatronKeysAudioProcessor&);
    ~AggregatronKeysAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ImageKnobLookAndFeel;
    class ImageSliderLookAndFeel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct KnobControl
    {
        juce::Slider slider;
        OutlinedLabel label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void configureSlider(KnobControl& control, const juce::String& parameterId, const juce::String& text);
    void configureWheel(juce::Slider& slider, OutlinedLabel& label, const juce::String& text, double min, double max, double initial);
    void applyLabelFonts();
    void timerCallback() override;

    AggregatronKeysAudioProcessor& audioProcessor;
    juce::Image backgroundImage;
    juce::Image knobImage;
    juce::Image sliderImage;
    juce::Font titleFont;
    juce::Font bodyFont;
    std::unique_ptr<ImageKnobLookAndFeel> knobLookAndFeel;
    std::unique_ptr<ImageSliderLookAndFeel> sliderLookAndFeel;

    OutlinedLabel titleLabel;
    OutlinedLabel hintLabel;
    OutlinedLabel versionLabel;
    OutlinedLabel savePresetLabel;
    OutlinedLabel loadPresetLabel;
    juce::TextButton savePresetButton { "" };
    juce::TextButton loadPresetButton { "" };
    juce::MidiKeyboardComponent keyboardComponent { audioProcessor.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard };

    KnobControl gainControl;
    KnobControl ampAttackControl;
    KnobControl ampDecayControl;
    KnobControl ampSustainControl;
    KnobControl ampReleaseControl;
    KnobControl oscMixControl;
    KnobControl osc2DetuneControl;
    KnobControl osc2FineControl;
    KnobControl filterCutoffControl;
    KnobControl filterResonanceControl;
    KnobControl filterEnvAmountControl;
    KnobControl filterAttackControl;
    KnobControl filterDecayControl;
    KnobControl filterSustainControl;
    KnobControl filterReleaseControl;
    KnobControl lfoRateControl;
    KnobControl lfoPitchDepthControl;
    KnobControl lfoFilterDepthControl;
    KnobControl polyphonyControl;
    KnobControl driveControl;
    KnobControl reverbMixControl;
    KnobControl reverbSizeControl;
    KnobControl reverbDampingControl;

    juce::Slider pitchWheelSlider;
    juce::Slider modWheelSlider;
    OutlinedLabel pitchWheelLabel;
    OutlinedLabel modWheelLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregatronKeysAudioProcessorEditor)
};
