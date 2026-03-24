#include "PluginEditor.h"

namespace
{
int countWhiteKeysInRange(int startNote, int endNote)
{
    int count = 0;

    for (int note = startNote; note <= endNote; ++note)
        if (! juce::MidiMessage::isMidiNoteBlack(note))
            ++count;

    return count;
}

juce::Font createFontFromBinary(const char* data, int size, float height)
{
    auto typeface = juce::Typeface::createSystemTypefaceFor(data, static_cast<size_t>(size));

    if (typeface != nullptr)
        return juce::Font(juce::FontOptions(typeface).withHeight(height));

    return juce::Font(juce::FontOptions(height));
}

void stylePresetButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::transparentWhite);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::transparentWhite);
}
}

void OutlinedLabel::paint(juce::Graphics& g)
{
    auto labelText = getText();
    if (labelText.isEmpty())
        return;

    auto currentFont = getFont();
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(currentFont, labelText, 0.0f, currentFont.getAscent());

    juce::Path path;
    glyphs.createPath(path);

    auto pathBounds = path.getBounds();
    auto area = getLocalBounds().toFloat().reduced(8.0f, 2.0f);
    float x = area.getX();

    const auto currentJustification = getJustificationType();
    if (currentJustification.testFlags(juce::Justification::horizontallyCentred))
        x = area.getCentreX() - pathBounds.getWidth() * 0.5f;
    else if (currentJustification.testFlags(juce::Justification::right))
        x = area.getRight() - pathBounds.getWidth();

    const auto y = area.getCentreY() - pathBounds.getHeight() * 0.5f;
    auto transformed = path;
    transformed.applyTransform(juce::AffineTransform::translation(x - pathBounds.getX(), y - pathBounds.getY()));

    g.setColour(juce::Colours::black);
    g.strokePath(transformed, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(findColour(juce::Label::textColourId));
    g.fillPath(transformed);
}

class AggregatronKeysAudioProcessorEditor::ImageKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit ImageKnobLookAndFeel(juce::Image image)
        : knobImage(std::move(image))
    {
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(3.0f);

        if (! knobImage.isValid())
        {
            LookAndFeel_V4::drawRotarySlider(g, x, y, width, height, sliderPosProportional, rotaryStartAngle, rotaryEndAngle, slider);
            return;
        }

        const auto angle = juce::jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);
        const auto scale = juce::jmin(bounds.getWidth() / static_cast<float>(knobImage.getWidth()),
                                      bounds.getHeight() / static_cast<float>(knobImage.getHeight()));

        auto transform = juce::AffineTransform::translation(-static_cast<float>(knobImage.getWidth()) * 0.5f,
                                                            -static_cast<float>(knobImage.getHeight()) * 0.5f)
                             .scaled(scale, scale)
                             .rotated(angle - juce::MathConstants<float>::halfPi)
                             .translated(bounds.getCentreX(), bounds.getCentreY());

        g.drawImageTransformed(knobImage, transform, false);
    }

private:
    juce::Image knobImage;
};

class AggregatronKeysAudioProcessorEditor::ImageSliderLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit ImageSliderLookAndFeel(juce::Image image)
        : sliderImage(std::move(image))
    {
    }

    void drawLinearSlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        juce::ignoreUnused(minSliderPos, maxSliderPos, style, slider);

        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height));
        auto track = bounds.reduced(bounds.getWidth() * 0.35f, 8.0f);

        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(track, track.getWidth() * 0.5f);

        if (! sliderImage.isValid())
        {
            g.setColour(juce::Colours::white);
            g.fillEllipse(bounds.withCentre({ bounds.getCentreX(), sliderPos }).withSizeKeepingCentre(16.0f, 16.0f));
            return;
        }

        const auto scale = juce::jmin(bounds.getWidth() / static_cast<float>(sliderImage.getWidth()), 0.9f);
        const auto imageWidth = static_cast<float>(sliderImage.getWidth()) * scale;
        const auto imageHeight = static_cast<float>(sliderImage.getHeight()) * scale;
        auto thumbBounds = juce::Rectangle<float>(imageWidth, imageHeight).withCentre({ bounds.getCentreX(), sliderPos });

        g.drawImage(sliderImage,
                    static_cast<int>(thumbBounds.getX()),
                    static_cast<int>(thumbBounds.getY()),
                    static_cast<int>(thumbBounds.getWidth()),
                    static_cast<int>(thumbBounds.getHeight()),
                    0,
                    0,
                    sliderImage.getWidth(),
                    sliderImage.getHeight());
    }

private:
    juce::Image sliderImage;
};

AggregatronKeysAudioProcessorEditor::AggregatronKeysAudioProcessorEditor(AggregatronKeysAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      backgroundImage(juce::ImageCache::getFromMemory(BinaryData::bg_png, BinaryData::bg_pngSize)),
      knobImage(juce::ImageCache::getFromMemory(BinaryData::knob_png, BinaryData::knob_pngSize)),
      sliderImage(juce::ImageCache::getFromMemory(BinaryData::slider_png, BinaryData::slider_pngSize)),
      titleFont(createFontFromBinary(BinaryData::font_ttf, BinaryData::font_ttfSize, 32.0f)),
      bodyFont(createFontFromBinary(BinaryData::font_ttf, BinaryData::font_ttfSize, 20.0f)),
      knobLookAndFeel(std::make_unique<ImageKnobLookAndFeel>(knobImage)),
      sliderLookAndFeel(std::make_unique<ImageSliderLookAndFeel>(sliderImage))
{
    setSize(1100, 660);

    titleLabel.setText("AggregaKeys", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Enable midi-controller in Options>Audio/MIDI Settings", juce::dontSendNotification);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colours::whitesmoke);
    addAndMakeVisible(hintLabel);

    versionLabel.setText(audioProcessor.getVersionString(), juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(versionLabel);

    savePresetLabel.setText("Save", juce::dontSendNotification);
    savePresetLabel.setJustificationType(juce::Justification::centred);
    savePresetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    savePresetLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(savePresetLabel);

    loadPresetLabel.setText("Load", juce::dontSendNotification);
    loadPresetLabel.setJustificationType(juce::Justification::centred);
    loadPresetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    loadPresetLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(loadPresetLabel);

    stylePresetButton(savePresetButton);
    stylePresetButton(loadPresetButton);
    savePresetButton.setTooltip("Save current sound to a preset file");
    loadPresetButton.setTooltip("Load a preset file");
    addAndMakeVisible(savePresetButton);
    addAndMakeVisible(loadPresetButton);

    savePresetButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser>("Save preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("AggregaKeys.preset"), "*.preset");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                             [this, chooser](const juce::FileChooser& fc)
                             {
                                 auto file = fc.getResult();
                                 if (file != juce::File())
                                     audioProcessor.savePresetToFile(file);
                             });
    };

    loadPresetButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser>("Load preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.preset");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this, chooser](const juce::FileChooser& fc)
                             {
                                 auto file = fc.getResult();
                                 if (file != juce::File())
                                     audioProcessor.loadPresetFromFile(file);
                             });
    };

    configureSlider(gainControl, "gain", "Gain");
    configureSlider(ampAttackControl, "attack", "Amp A");
    configureSlider(ampDecayControl, "decay", "Amp D");
    configureSlider(ampSustainControl, "sustain", "Amp S");
    configureSlider(ampReleaseControl, "release", "Amp R");
    configureSlider(oscMixControl, "oscMix", "Osc Mix");
    configureSlider(osc2DetuneControl, "osc2Detune", "Detune");
    configureSlider(osc2FineControl, "osc2Fine", "Fine");
    configureSlider(filterCutoffControl, "filterCutoff", "Cutoff");
    configureSlider(filterResonanceControl, "filterResonance", "Reso");
    configureSlider(filterEnvAmountControl, "filterEnvAmount", "Env Amt");
    configureSlider(filterAttackControl, "filterAttack", "Filt A");
    configureSlider(filterDecayControl, "filterDecay", "Filt D");
    configureSlider(filterSustainControl, "filterSustain", "Filt S");
    configureSlider(filterReleaseControl, "filterRelease", "Filt R");
    configureSlider(lfoRateControl, "lfoRate", "LFO Rate");
    configureSlider(lfoPitchDepthControl, "lfoPitchDepth", "Pitch LFO");
    configureSlider(lfoFilterDepthControl, "lfoFilterDepth", "Filt LFO");
    configureSlider(polyphonyControl, "polyphony", "Voices");
    configureSlider(driveControl, "drive", "Drive");
    configureSlider(reverbMixControl, "reverbMix", "Rev Mix");
    configureSlider(reverbSizeControl, "reverbSize", "Rev Size");
    configureSlider(reverbDampingControl, "reverbDamping", "Rev Damp");

    configureWheel(pitchWheelSlider, pitchWheelLabel, "Pitch", 0.0, 16383.0, 8192.0);
    configureWheel(modWheelSlider, modWheelLabel, "Mod", 0.0, 1.0, 0.0);
    applyLabelFonts();

    pitchWheelSlider.onValueChange = [this]
    {
        if (pitchWheelSlider.isMouseButtonDown())
            audioProcessor.setUiPitchWheel(juce::roundToInt(pitchWheelSlider.getValue()));
    };

    pitchWheelSlider.onDragEnd = [this]
    {
        pitchWheelSlider.setValue(8192.0, juce::sendNotificationSync);
    };

    modWheelSlider.onValueChange = [this]
    {
        if (modWheelSlider.isMouseButtonDown())
            audioProcessor.setUiModWheel(static_cast<float>(modWheelSlider.getValue()));
    };

    keyboardComponent.setAvailableRange(36, 107);
    keyboardComponent.setLowestVisibleKey(36);
    addAndMakeVisible(keyboardComponent);

    startTimerHz(30);
}

AggregatronKeysAudioProcessorEditor::~AggregatronKeysAudioProcessorEditor()
{
    for (auto* slider : {
             &gainControl.slider, &ampAttackControl.slider, &ampDecayControl.slider, &ampSustainControl.slider, &ampReleaseControl.slider,
             &oscMixControl.slider, &osc2DetuneControl.slider, &osc2FineControl.slider,
             &filterCutoffControl.slider, &filterResonanceControl.slider, &filterEnvAmountControl.slider,
             &filterAttackControl.slider, &filterDecayControl.slider, &filterSustainControl.slider, &filterReleaseControl.slider,
             &lfoRateControl.slider, &lfoPitchDepthControl.slider, &lfoFilterDepthControl.slider,
             &polyphonyControl.slider, &driveControl.slider, &reverbMixControl.slider, &reverbSizeControl.slider, &reverbDampingControl.slider })
        slider->setLookAndFeel(nullptr);

    for (auto* slider : { &pitchWheelSlider, &modWheelSlider })
        slider->setLookAndFeel(nullptr);
}

void AggregatronKeysAudioProcessorEditor::configureSlider(KnobControl& control, const juce::String& parameterId, const juce::String& text)
{
    control.slider.setLookAndFeel(knobLookAndFeel.get());
    control.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setRotaryParameters(juce::degreesToRadians(225.0f), juce::degreesToRadians(495.0f), true);
    control.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 18);
    control.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    control.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    control.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha(0.18f));
    addAndMakeVisible(control.slider);

    control.label.setText(text, juce::dontSendNotification);
    control.label.setColour(juce::Label::textColourId, juce::Colours::white);
    control.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(control.label);

    control.attachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, parameterId, control.slider);
}

void AggregatronKeysAudioProcessorEditor::configureWheel(juce::Slider& slider, OutlinedLabel& label, const juce::String& text, double min, double max, double initial)
{
    slider.setLookAndFeel(sliderLookAndFeel.get());
    slider.setRange(min, max);
    slider.setValue(initial, juce::dontSendNotification);
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour(juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void AggregatronKeysAudioProcessorEditor::applyLabelFonts()
{
    titleLabel.setFont(titleFont);
    hintLabel.setFont(bodyFont);
    versionLabel.setFont(bodyFont);
    savePresetLabel.setFont(bodyFont);
    loadPresetLabel.setFont(bodyFont);
    savePresetButton.setTooltip("Save current sound");
    loadPresetButton.setTooltip("Load saved sound");

    for (auto* label : {
             &gainControl.label, &ampAttackControl.label, &ampDecayControl.label, &ampSustainControl.label, &ampReleaseControl.label,
             &oscMixControl.label, &osc2DetuneControl.label, &osc2FineControl.label,
             &filterCutoffControl.label, &filterResonanceControl.label, &filterEnvAmountControl.label,
             &filterAttackControl.label, &filterDecayControl.label, &filterSustainControl.label, &filterReleaseControl.label,
             &lfoRateControl.label, &lfoPitchDepthControl.label, &lfoFilterDepthControl.label,
             &polyphonyControl.label, &driveControl.label, &reverbMixControl.label, &reverbSizeControl.label, &reverbDampingControl.label,
             &pitchWheelLabel, &modWheelLabel })
        label->setFont(bodyFont);
}

void AggregatronKeysAudioProcessorEditor::timerCallback()
{
    if (! pitchWheelSlider.isMouseButtonDown())
        pitchWheelSlider.setValue(static_cast<double>(audioProcessor.getUiPitchWheel()), juce::dontSendNotification);

    if (! modWheelSlider.isMouseButtonDown())
        modWheelSlider.setValue(static_cast<double>(audioProcessor.getUiModWheelNormalized()), juce::dontSendNotification);
}

void AggregatronKeysAudioProcessorEditor::paint(juce::Graphics& g)
{
    if (backgroundImage.isValid())
        g.drawImageWithin(backgroundImage, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::fillDestination);
    else
        g.fillAll(juce::Colour::fromRGB(20, 25, 35));

    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(12.0f), 18.0f);
}

void AggregatronKeysAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto header = area.removeFromTop(76);
    auto headerTop = header.removeFromTop(38);

    titleLabel.setBounds(headerTop.removeFromLeft(260));
    auto buttonArea = headerTop.removeFromRight(300);
    versionLabel.setBounds(buttonArea.removeFromRight(110));
    auto loadBounds = buttonArea.removeFromRight(110).reduced(4, 2);
    loadPresetButton.setBounds(loadBounds);
    loadPresetLabel.setBounds(loadBounds);
    auto saveBounds = buttonArea.removeFromRight(110).reduced(4, 2);
    savePresetButton.setBounds(saveBounds);
    savePresetLabel.setBounds(saveBounds);
    hintLabel.setBounds(header);

    auto row1 = area.removeFromTop(104);
    auto row2 = area.removeFromTop(104);
    auto row3 = area.removeFromTop(104);

    auto layoutRow = [](juce::Rectangle<int> row, std::initializer_list<KnobControl*> controls)
    {
        const int count = static_cast<int>(controls.size());
        if (count == 0)
            return;

        const int cellWidth = row.getWidth() / count;
        int index = 0;
        for (auto* control : controls)
        {
            auto cell = row.withTrimmedLeft(index * cellWidth).removeFromLeft(cellWidth).reduced(1, 0);
            control->label.setBounds(cell.removeFromTop(24));
            control->slider.setBounds(cell.reduced(4, 0));
            ++index;
        }
    };

    layoutRow(row1, { &gainControl, &ampAttackControl, &ampDecayControl, &ampSustainControl, &ampReleaseControl, &oscMixControl, &osc2DetuneControl, &osc2FineControl });
    layoutRow(row2, { &filterCutoffControl, &filterResonanceControl, &filterEnvAmountControl, &filterAttackControl, &filterDecayControl, &filterSustainControl, &filterReleaseControl });
    layoutRow(row3, { &lfoRateControl, &lfoPitchDepthControl, &lfoFilterDepthControl, &polyphonyControl, &driveControl, &reverbMixControl, &reverbSizeControl, &reverbDampingControl });

    area.removeFromTop(8);
    auto performanceArea = area;
    auto wheelColumn = performanceArea.removeFromLeft(118);
    auto wheelWidth = wheelColumn.getWidth() / 2;

    auto pitchArea = wheelColumn.removeFromLeft(wheelWidth).reduced(2, 0);
    pitchWheelLabel.setBounds(pitchArea.removeFromBottom(28));
    pitchWheelSlider.setBounds(pitchArea.reduced(14, 8));

    auto modArea = wheelColumn.reduced(2, 0);
    modWheelLabel.setBounds(modArea.removeFromBottom(28));
    modWheelSlider.setBounds(modArea.reduced(14, 8));

    performanceArea.removeFromLeft(12);
    auto keyboardArea = performanceArea;
    const auto keyboardHeight = juce::jmin(132, keyboardArea.getHeight());
    keyboardArea = keyboardArea.withSizeKeepingCentre(keyboardArea.getWidth(), keyboardHeight);

    constexpr int firstNote = 36;
    constexpr int lastNote = 107;
    const auto whiteKeys = countWhiteKeysInRange(firstNote, lastNote);
    if (whiteKeys > 0)
        keyboardComponent.setKeyWidth(static_cast<float>(keyboardArea.getWidth()) / static_cast<float>(whiteKeys));

    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colours::white.withAlpha(0.94f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colours::black.withAlpha(0.82f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colours::black.withAlpha(0.35f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colours::skyblue.withAlpha(0.22f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colours::deepskyblue.withAlpha(0.42f));
    keyboardComponent.setBounds(keyboardArea);
}



