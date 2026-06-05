#include "SimpleVocoderEditor.h"

namespace
{
juce::Colour bgA() { return juce::Colour::fromRGB(12, 12, 16); }
juce::Colour bgB() { return juce::Colour::fromRGB(29, 33, 42); }
juce::Colour panel() { return juce::Colour::fromRGB(20, 24, 32); }
juce::Colour accent() { return juce::Colour::fromRGB(123, 241, 193); }
juce::Colour accentSoft() { return juce::Colour::fromRGB(255, 199, 105); }
}

SimpleVocoderAudioProcessorEditor::SimpleVocoderAudioProcessorEditor(SimpleVocoderAudioProcessor& processor)
    : AudioProcessorEditor(&processor), audioProcessor(processor)
{
    setSize(520, 200);

    titleLabel.setText("SimpleVocoder", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("D major robot voice", juce::dontSendNotification);
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);
    subtitleLabel.setFont(juce::FontOptions(11.0f));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    addAndMakeVisible(subtitleLabel);

    statusLabel.setJustificationType(juce::Justification::centredRight);
    statusLabel.setFont(juce::FontOptions(12.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.75f));
    addAndMakeVisible(statusLabel);

    configureKnob(toneKnob, "tone", "Tone");
    configureKnob(robotKnob, "robot", "Robot");
    configureKnob(mixKnob, "mix", "Mix");
    configureKnob(outputKnob, "outputGain", "Output");

    startTimerHz(18);
}

void SimpleVocoderAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient gradient(bgA(), 0.0f, 0.0f, bgB(), static_cast<float>(getWidth()), static_cast<float>(getHeight()), false);
    g.setGradientFill(gradient);
    g.fillAll();

    auto panelBounds = getLocalBounds().toFloat().reduced(10.0f);
    g.setColour(panel().withAlpha(0.96f));
    g.fillRoundedRectangle(panelBounds, 18.0f);

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(panelBounds, 18.0f, 1.0f);

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillRoundedRectangle(meterBounds, 6.0f);

    auto filled = meterBounds;
    filled.setWidth(meterBounds.getWidth() * juce::jlimit(0.0f, 1.0f, displayedEnvelope));
    g.setColour(accent());
    g.fillRoundedRectangle(filled, 6.0f);

    g.setColour(accentSoft().withAlpha(0.9f));
    g.setFont(juce::FontOptions(11.0f));
    g.drawFittedText("voice", meterBounds.getSmallestIntegerContainer(), juce::Justification::centred, 1);
}

void SimpleVocoderAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto header = area.removeFromTop(22);
    titleLabel.setBounds(header.removeFromLeft(170));
    statusLabel.setBounds(header.removeFromRight(190));

    subtitleLabel.setBounds(area.removeFromTop(16));

    area.removeFromTop(4);
    meterBounds = area.removeFromTop(16).toFloat();

    area.removeFromTop(8);
    const int columns = 4;
    const auto cellWidth = area.getWidth() / columns;
    std::array<Knob*, columns> knobs { &toneKnob, &robotKnob, &mixKnob, &outputKnob };

    for (int index = 0; index < columns; ++index)
    {
        auto cell = juce::Rectangle<int>(area.getX() + (index * cellWidth), area.getY(), cellWidth, area.getHeight()).reduced(6, 0);
        knobs[static_cast<size_t>(index)]->label.setBounds(cell.removeFromTop(18));
        knobs[static_cast<size_t>(index)]->slider.setBounds(cell);
    }
}

void SimpleVocoderAudioProcessorEditor::configureKnob(Knob& knob, const juce::String& parameterId, const juce::String& labelText)
{
    knob.label.setText(labelText, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(knob.label);

    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    knob.slider.setColour(juce::Slider::rotarySliderFillColourId, accent());
    knob.slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha(0.15f));
    knob.slider.setColour(juce::Slider::thumbColourId, accentSoft());
    knob.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    knob.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha(0.18f));
    addAndMakeVisible(knob.slider);

    knob.attachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, parameterId, knob.slider);
}

void SimpleVocoderAudioProcessorEditor::timerCallback()
{
    const auto target = audioProcessor.getEnvelopeAmount();
    displayedEnvelope += (target - displayedEnvelope) * 0.2f;
    const auto detectedHz = audioProcessor.getDetectedPitchHz();
    const auto targetMidi = audioProcessor.getTargetMidiNote();
    if (detectedHz > 0.0f)
    {
        const auto pitchClass = ((targetMidi % 12) + 12) % 12;
        const auto octave = (targetMidi / 12) - 1;
        statusLabel.setText(SimpleVocoderAudioProcessor::pitchClassName(pitchClass) + juce::String(octave),
                            juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText("waiting", juce::dontSendNotification);
    }
    repaint(meterBounds.getSmallestIntegerContainer().expanded(2));
}
