#pragma once

#include "VocoderProcessor.h"

class AggregaVocoderAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit AggregaVocoderAudioProcessorEditor(AggregaVocoderAudioProcessor&);
    ~AggregaVocoderAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void configureKnob(Knob& knob, const juce::String& parameterId, const juce::String& labelText);
    void timerCallback() override;

    AggregaVocoderAudioProcessor& audioProcessor;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label versionLabel;
    juce::Label statusLabel;
    juce::Label rootLabel;
    juce::Label scaleLabel;
    juce::Label octaveLabel;
    juce::Label modeLabel;
    juce::Label harmonyIntervalLabel;
    juce::ComboBox rootCombo;
    juce::ComboBox scaleCombo;
    juce::ComboBox octaveCombo;
    juce::ComboBox modeCombo;
    juce::ComboBox harmonyIntervalCombo;
    std::unique_ptr<ComboAttachment> rootAttachment;
    std::unique_ptr<ComboAttachment> scaleAttachment;
    std::unique_ptr<ComboAttachment> octaveAttachment;
    std::unique_ptr<ComboAttachment> modeAttachment;
    std::unique_ptr<ComboAttachment> harmonyIntervalAttachment;
    Knob correctionKnob;
    Knob snapKnob;
    Knob trackingKnob;
    Knob toneKnob;
    Knob mixKnob;
    Knob harmonyKnob;
    Knob outputKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregaVocoderAudioProcessorEditor)
};
