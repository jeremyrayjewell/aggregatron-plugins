#include "VocoderEditor.h"

AggregaVocoderAudioProcessorEditor::AggregaVocoderAudioProcessorEditor(AggregaVocoderAudioProcessor& processor)
    : AudioProcessorEditor(&processor), audioProcessor(processor)
{
    setSize(1040, 470);

    titleLabel.setText("AggregaVocoder", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Pitch correction with key, scale, octave, robot mode, and harmony stacking.", juce::dontSendNotification);
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);
    subtitleLabel.setFont(juce::FontOptions(15.0f));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::whitesmoke);
    addAndMakeVisible(subtitleLabel);

    versionLabel.setText(audioProcessor.getVersionString(), juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    versionLabel.setFont(juce::FontOptions(14.0f));
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(versionLabel);

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setFont(juce::FontOptions(14.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
    addAndMakeVisible(statusLabel);

    rootLabel.setText("Key", juce::dontSendNotification);
    scaleLabel.setText("Scale", juce::dontSendNotification);
    octaveLabel.setText("Octave", juce::dontSendNotification);
    modeLabel.setText("Mode", juce::dontSendNotification);
    harmonyIntervalLabel.setText("Harmony Int", juce::dontSendNotification);

    for (auto* label : { &rootLabel, &scaleLabel, &octaveLabel, &modeLabel, &harmonyIntervalLabel })
    {
        label->setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(*label);
    }

    for (int i = 0; i < 12; ++i)
        rootCombo.addItem(AggregaVocoderAudioProcessor::pitchClassName(i), i + 1);

    int scaleId = 1;
    for (const auto& scale : AggregaVocoderAudioProcessor::getScaleTemplates())
        scaleCombo.addItem(scale.name, scaleId++);

    for (const auto& option : { "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4" })
        octaveCombo.addItem(option, octaveCombo.getNumItems() + 1);

    modeCombo.addItem("Natural", 1);
    modeCombo.addItem("Robot", 2);

    for (const auto& option : { "-12", "-7", "-5", "+5", "+7", "+12", "+19" })
        harmonyIntervalCombo.addItem(option, harmonyIntervalCombo.getNumItems() + 1);

    for (auto* combo : { &rootCombo, &scaleCombo, &octaveCombo, &modeCombo, &harmonyIntervalCombo })
        addAndMakeVisible(*combo);

    rootAttachment = std::make_unique<ComboAttachment>(audioProcessor.parameters, "rootNote", rootCombo);
    scaleAttachment = std::make_unique<ComboAttachment>(audioProcessor.parameters, "scale", scaleCombo);
    octaveAttachment = std::make_unique<ComboAttachment>(audioProcessor.parameters, "octaveShift", octaveCombo);
    modeAttachment = std::make_unique<ComboAttachment>(audioProcessor.parameters, "mode", modeCombo);
    harmonyIntervalAttachment = std::make_unique<ComboAttachment>(audioProcessor.parameters, "harmonyInterval", harmonyIntervalCombo);

    configureKnob(correctionKnob, "correctionAmount", "Correction");
    configureKnob(snapKnob, "snapSpeed", "Snap");
    configureKnob(trackingKnob, "tracking", "Tracking");
    configureKnob(toneKnob, "tone", "Tone");
    configureKnob(mixKnob, "mix", "Mix");
    configureKnob(harmonyKnob, "harmonyMix", "Harmony");
    configureKnob(outputKnob, "outputGain", "Output");

    startTimerHz(12);
}

void AggregaVocoderAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient background(juce::Colour::fromRGB(14, 20, 34), 0.0f, 0.0f,
                                    juce::Colour::fromRGB(30, 69, 77), static_cast<float>(getWidth()), static_cast<float>(getHeight()),
                                    false);
    background.addColour(0.7, juce::Colour::fromRGB(98, 52, 31));
    g.setGradientFill(background);
    g.fillAll();

    auto panel = getLocalBounds().toFloat().reduced(14.0f);
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(panel, 26.0f);

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(panel, 26.0f, 1.4f);
}

void AggregaVocoderAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);
    auto header = area.removeFromTop(66);
    titleLabel.setBounds(header.removeFromLeft(300));
    versionLabel.setBounds(header.removeFromRight(110));
    subtitleLabel.setBounds(area.removeFromTop(26));
    statusLabel.setBounds(area.removeFromTop(24));

    area.removeFromTop(16);
    auto controlsTop = area.removeFromTop(58);

    auto setComboBlock = [](juce::Rectangle<int> block, juce::Label& label, juce::ComboBox& combo)
    {
        label.setBounds(block.removeFromTop(18));
        combo.setBounds(block.removeFromTop(32));
    };

    setComboBlock(controlsTop.removeFromLeft(110), rootLabel, rootCombo);
    controlsTop.removeFromLeft(10);
    setComboBlock(controlsTop.removeFromLeft(200), scaleLabel, scaleCombo);
    controlsTop.removeFromLeft(10);
    setComboBlock(controlsTop.removeFromLeft(90), octaveLabel, octaveCombo);
    controlsTop.removeFromLeft(10);
    setComboBlock(controlsTop.removeFromLeft(110), modeLabel, modeCombo);
    controlsTop.removeFromLeft(10);
    setComboBlock(controlsTop.removeFromLeft(120), harmonyIntervalLabel, harmonyIntervalCombo);

    area.removeFromTop(12);
    const int columns = 7;
    const auto cellWidth = area.getWidth() / columns;
    std::array<Knob*, 7> knobs { &correctionKnob, &snapKnob, &trackingKnob, &toneKnob, &mixKnob, &harmonyKnob, &outputKnob };

    for (int index = 0; index < static_cast<int>(knobs.size()); ++index)
    {
        auto cell = juce::Rectangle<int>(area.getX() + (index * cellWidth), area.getY(), cellWidth, area.getHeight()).reduced(8, 4);
        knobs[static_cast<size_t>(index)]->label.setBounds(cell.removeFromTop(22));
        knobs[static_cast<size_t>(index)]->slider.setBounds(cell.reduced(0, 4));
    }
}

void AggregaVocoderAudioProcessorEditor::configureKnob(Knob& knob, const juce::String& parameterId, const juce::String& labelText)
{
    knob.label.setText(labelText, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(knob.label);

    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    knob.slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGB(255, 143, 91));
    knob.slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha(0.14f));
    knob.slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    knob.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    knob.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha(0.2f));
    addAndMakeVisible(knob.slider);

    knob.attachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, parameterId, knob.slider);
}

void AggregaVocoderAudioProcessorEditor::timerCallback()
{
    const auto detectedHz = audioProcessor.getDetectedPitchHz();
    const auto targetMidi = audioProcessor.getTargetMidiNote();

    if (detectedHz <= 0.0f)
    {
        statusLabel.setText("Pitch status: waiting for a clear monophonic input signal.", juce::dontSendNotification);
        return;
    }

    const auto targetPitchClass = ((targetMidi % 12) + 12) % 12;
    const auto targetOctave = (targetMidi / 12) - 1;
    statusLabel.setText("Detected: " + juce::String(detectedHz, 1) + " Hz   |   Target: "
                        + AggregaVocoderAudioProcessor::pitchClassName(targetPitchClass)
                        + juce::String(targetOctave),
                        juce::dontSendNotification);
}
