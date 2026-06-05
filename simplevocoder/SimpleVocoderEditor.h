#pragma once

#include "SimpleVocoderProcessor.h"

class SimpleVocoderAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit SimpleVocoderAudioProcessorEditor(SimpleVocoderAudioProcessor&);
    ~SimpleVocoderAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void configureKnob(Knob& knob, const juce::String& parameterId, const juce::String& labelText);
    void timerCallback() override;

    SimpleVocoderAudioProcessor& audioProcessor;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label statusLabel;
    juce::Rectangle<float> meterBounds;
    float displayedEnvelope = 0.0f;
    Knob toneKnob;
    Knob robotKnob;
    Knob mixKnob;
    Knob outputKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleVocoderAudioProcessorEditor)
};
