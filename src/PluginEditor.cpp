#include "PluginEditor.h"

namespace
{
constexpr std::array<char, 17> computerKeyboardCharacters { 'a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k', 'o', 'l', 'p', ';' };

int keyPressToComputerKeyboardNote(const juce::KeyPress& key) noexcept
{
    const auto character = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());

    for (size_t index = 0; index < computerKeyboardCharacters.size(); ++index)
        if (character == static_cast<juce::juce_wchar>(computerKeyboardCharacters[index]))
            return 48 + static_cast<int>(index);

    return -1;
}

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

void styleModeButton(juce::ToggleButton& button)
{
    button.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    button.setColour(juce::ToggleButton::tickColourId, juce::Colours::deepskyblue.withAlpha(0.85f));
    button.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::white.withAlpha(0.35f));
}

void setupSectionLabel(OutlinedLabel& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.92f));
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

class AggregatronKeysAudioProcessorEditor::ImageComboBoxLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ImageComboBoxLookAndFeel()
        : comboFont(juce::FontOptions(18.0f))
    {
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return comboFont;
    }

    juce::Font getPopupMenuFont() override
    {
        return comboFont;
    }

private:
    juce::Font comboFont;
};

class AggregatronKeysAudioProcessorEditor::ImageButtonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto fill = juce::Colours::black.withAlpha(shouldDrawButtonAsDown ? 0.26f : shouldDrawButtonAsHighlighted ? 0.18f : 0.12f);
        g.setColour(fill);
        g.fillRect(bounds);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawRect(bounds, 1.0f);
    }
};

class AggregatronKeysAudioProcessorEditor::WaveformDisplay final : public juce::Component
{
public:
    void setWaveform(const std::vector<float>& newSamples)
    {
        samples = newSamples;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.20f));
        g.fillRect(bounds);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRect(bounds, 1.0f);

        auto inner = bounds.reduced(14.0f, 10.0f);
        const auto centreY = inner.getCentreY();

        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawHorizontalLine(static_cast<int>(centreY), inner.getX(), inner.getRight());

        if (samples.empty())
            return;

        juce::Path waveform;
        for (size_t i = 0; i < samples.size(); ++i)
        {
            const auto x = juce::jmap(static_cast<float>(i), 0.0f, static_cast<float>(samples.size() - 1), inner.getX(), inner.getRight());
            const auto y = centreY - juce::jlimit(-1.0f, 1.0f, samples[i]) * inner.getHeight() * 0.40f;

            if (i == 0)
                waveform.startNewSubPath(x, y);
            else
                waveform.lineTo(x, y);
        }

        g.setColour(juce::Colours::deepskyblue.withAlpha(0.18f));
        g.strokePath(waveform, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.strokePath(waveform, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    std::vector<float> samples;
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
      sliderLookAndFeel(std::make_unique<ImageSliderLookAndFeel>(sliderImage)),
      comboBoxLookAndFeel(std::make_unique<ImageComboBoxLookAndFeel>()),
      buttonLookAndFeel(std::make_unique<ImageButtonLookAndFeel>()),
      waveformDisplay(std::make_unique<WaveformDisplay>())
{
    setSize(1100, 660);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    addKeyListener(this);

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

    setupSectionLabel(oscSectionLabel, "Oscillator");
    setupSectionLabel(ampSectionLabel, "Amp");
    setupSectionLabel(filterSectionLabel, "Filter");
    setupSectionLabel(motionSectionLabel, "Motion");
    setupSectionLabel(performanceSectionLabel, "Performance");
    setupSectionLabel(fxSectionLabel, "Space");
    addAndMakeVisible(oscSectionLabel);
    addAndMakeVisible(ampSectionLabel);
    addAndMakeVisible(filterSectionLabel);
    addAndMakeVisible(motionSectionLabel);
    addAndMakeVisible(performanceSectionLabel);
    addAndMakeVisible(fxSectionLabel);

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
    savePresetButton.setLookAndFeel(buttonLookAndFeel.get());
    loadPresetButton.setLookAndFeel(buttonLookAndFeel.get());
    savePresetButton.setTooltip("Save current sound to a preset file");
    loadPresetButton.setTooltip("Load a preset file");
    disableKeyboardFocus(savePresetButton);
    disableKeyboardFocus(loadPresetButton);
    addAndMakeVisible(savePresetButton);
    addAndMakeVisible(loadPresetButton);

    presetMenu.setLookAndFeel(comboBoxLookAndFeel.get());
    presetMenu.setTextWhenNothingSelected("Presets");
    disableKeyboardFocus(presetMenu);
    presetMenu.addItem("Init", 1);
    presetMenu.addItem("Soft Pad", 2);
    presetMenu.addItem("Bass Punch", 3);
    presetMenu.addItem("Glass Mono", 4);
    presetMenu.addItem("Wide Lead", 5);
    presetMenu.onChange = [this]
    {
        applyFactoryPreset(presetMenu.getText());
    };
    addAndMakeVisible(presetMenu);

    addAndMakeVisible(*waveformDisplay);

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
    configureSlider(velocityAmpControl, "velocityAmp", "Vel Amp");
    configureSlider(ampAttackControl, "attack", "Amp A");
    configureSlider(ampDecayControl, "decay", "Amp D");
    configureSlider(ampSustainControl, "sustain", "Amp S");
    configureSlider(ampReleaseControl, "release", "Amp R");
    configureChoice(osc1WaveControl, "osc1Wave", "Osc 1", { "Saw", "Square", "Triangle", "Sine" });
    configureChoice(osc2WaveControl, "osc2Wave", "Osc 2", { "Saw", "Square", "Triangle", "Sine" });
    configureSlider(oscMixControl, "oscMix", "Osc Mix");
    configureSlider(osc2DetuneControl, "osc2Detune", "Detune");
    configureSlider(osc2FineControl, "osc2Fine", "Fine");
    configureSlider(filterCutoffControl, "filterCutoff", "Cutoff");
    configureSlider(filterResonanceControl, "filterResonance", "Reso");
    configureSlider(filterEnvAmountControl, "filterEnvAmount", "Env Amt");
    configureSlider(velocityFilterControl, "velocityFilter", "Vel Filt");
    configureSlider(filterAttackControl, "filterAttack", "Filt A");
    configureSlider(filterDecayControl, "filterDecay", "Filt D");
    configureSlider(filterSustainControl, "filterSustain", "Filt S");
    configureSlider(filterReleaseControl, "filterRelease", "Filt R");
    configureSlider(lfoRateControl, "lfoRate", "LFO Rate");
    configureSlider(lfoPitchDepthControl, "lfoPitchDepth", "Pitch LFO");
    configureSlider(lfoFilterDepthControl, "lfoFilterDepth", "Filt LFO");
    configureSlider(glideControl, "glide", "Glide");
    configureSlider(polyphonyControl, "polyphony", "Voices");
    configureSlider(driveControl, "drive", "Drive");
    configureSlider(reverbMixControl, "reverbMix", "Rev Mix");
    configureSlider(reverbSizeControl, "reverbSize", "Rev Size");
    configureSlider(reverbDampingControl, "reverbDamping", "Rev Damp");

    styleModeButton(monoModeButton);
    monoModeButton.setClickingTogglesState(true);
    monoModeButton.setTooltip("Play monophonically with legato glide");
    disableKeyboardFocus(monoModeButton);
    monoModeAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, "monoMode", monoModeButton);
    addAndMakeVisible(monoModeButton);

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
    keyboardComponent.setKeyPressBaseOctave(4);
    keyboardComponent.clearKeyMappings();
    keyboardComponent.setWantsKeyboardFocus(false);
    keyboardComponent.setMouseClickGrabsKeyboardFocus(false);
    addAndMakeVisible(keyboardComponent);

    grabKeyboardFocus();
    startTimerHz(120);
}

AggregatronKeysAudioProcessorEditor::~AggregatronKeysAudioProcessorEditor()
{
    keyboardComponent.removeKeyListener(this);
    removeKeyListener(this);
    releaseComputerKeyboardNotes();
    savePresetButton.setLookAndFeel(nullptr);
    loadPresetButton.setLookAndFeel(nullptr);
    presetMenu.setLookAndFeel(nullptr);
    osc1WaveControl.comboBox.setLookAndFeel(nullptr);
    osc2WaveControl.comboBox.setLookAndFeel(nullptr);

    for (auto* slider : {
             &gainControl.slider, &velocityAmpControl.slider, &ampAttackControl.slider, &ampDecayControl.slider, &ampSustainControl.slider, &ampReleaseControl.slider,
             &oscMixControl.slider, &osc2DetuneControl.slider, &osc2FineControl.slider,
             &filterCutoffControl.slider, &filterResonanceControl.slider, &filterEnvAmountControl.slider, &velocityFilterControl.slider,
             &filterAttackControl.slider, &filterDecayControl.slider, &filterSustainControl.slider, &filterReleaseControl.slider,
             &lfoRateControl.slider, &lfoPitchDepthControl.slider, &lfoFilterDepthControl.slider, &glideControl.slider,
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
    disableKeyboardFocus(control.slider);
    addAndMakeVisible(control.slider);

    control.label.setText(text, juce::dontSendNotification);
    control.label.setColour(juce::Label::textColourId, juce::Colours::white);
    control.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(control.label);

    control.attachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, parameterId, control.slider);
}

void AggregatronKeysAudioProcessorEditor::configureChoice(ChoiceControl& control,
                                                          const juce::String& parameterId,
                                                          const juce::String& text,
                                                          const juce::StringArray& items)
{
    control.comboBox.addItemList(items, 1);
    control.comboBox.setJustificationType(juce::Justification::centred);
    control.comboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.20f));
    control.comboBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha(0.16f));
    control.comboBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    control.comboBox.setColour(juce::ComboBox::arrowColourId, juce::Colours::white.withAlpha(0.85f));
    control.comboBox.setLookAndFeel(comboBoxLookAndFeel.get());
    disableKeyboardFocus(control.comboBox);
    addAndMakeVisible(control.comboBox);

    control.label.setText(text, juce::dontSendNotification);
    control.label.setColour(juce::Label::textColourId, juce::Colours::white);
    control.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(control.label);

    control.attachment = std::make_unique<ComboBoxAttachment>(audioProcessor.parameters, parameterId, control.comboBox);
}

void AggregatronKeysAudioProcessorEditor::configureWheel(juce::Slider& slider, OutlinedLabel& label, const juce::String& text, double min, double max, double initial)
{
    slider.setLookAndFeel(sliderLookAndFeel.get());
    slider.setRange(min, max);
    slider.setValue(initial, juce::dontSendNotification);
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour(juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    disableKeyboardFocus(slider);
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
    oscSectionLabel.setFont(bodyFont);
    ampSectionLabel.setFont(bodyFont);
    filterSectionLabel.setFont(bodyFont);
    motionSectionLabel.setFont(bodyFont);
    performanceSectionLabel.setFont(bodyFont);
    fxSectionLabel.setFont(bodyFont);
    savePresetLabel.setFont(bodyFont);
    loadPresetLabel.setFont(bodyFont);
    presetMenu.setTextWhenNothingSelected("Presets");
    savePresetButton.setTooltip("Save current sound");
    loadPresetButton.setTooltip("Load saved sound");
    for (auto* label : {
             &gainControl.label, &velocityAmpControl.label, &ampAttackControl.label, &ampDecayControl.label, &ampSustainControl.label, &ampReleaseControl.label,
             &oscMixControl.label, &osc2DetuneControl.label, &osc2FineControl.label,
             &filterCutoffControl.label, &filterResonanceControl.label, &filterEnvAmountControl.label, &velocityFilterControl.label,
             &filterAttackControl.label, &filterDecayControl.label, &filterSustainControl.label, &filterReleaseControl.label,
             &lfoRateControl.label, &lfoPitchDepthControl.label, &lfoFilterDepthControl.label, &glideControl.label,
             &polyphonyControl.label, &driveControl.label, &reverbMixControl.label, &reverbSizeControl.label, &reverbDampingControl.label,
             &osc1WaveControl.label, &osc2WaveControl.label, &pitchWheelLabel, &modWheelLabel })
        label->setFont(bodyFont);
}

void AggregatronKeysAudioProcessorEditor::setParameterValue(const juce::String& parameterId, float value)
{
    if (auto* parameter = audioProcessor.parameters.getParameter(parameterId))
    {
        const auto range = audioProcessor.parameters.getParameterRange(parameterId);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(range.convertTo0to1(value));
        parameter->endChangeGesture();
    }
}

void AggregatronKeysAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    AudioProcessorEditor::mouseDown(event);
    grabKeyboardFocus();
}

void AggregatronKeysAudioProcessorEditor::disableKeyboardFocus(juce::Component& component)
{
    component.setWantsKeyboardFocus(false);
    component.setMouseClickGrabsKeyboardFocus(false);
}

void AggregatronKeysAudioProcessorEditor::focusLost(FocusChangeType cause)
{
    releaseComputerKeyboardNotes();
    AudioProcessorEditor::focusLost(cause);
}

bool AggregatronKeysAudioProcessorEditor::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused(originatingComponent);

    const auto noteNumber = keyPressToComputerKeyboardNote(key);
    if (noteNumber < 0)
        return false;

    const auto keyCode = key.getKeyCode();
    if (heldComputerKeys.find(keyCode) != heldComputerKeys.end())
        return true;

    heldComputerKeys[keyCode] = noteNumber;
    audioProcessor.appendDebugLog("[QWERTY] editor keyDown keyCode=" + juce::String(keyCode) + " note=" + juce::String(noteNumber));
    audioProcessor.getKeyboardState().noteOn(1, noteNumber, 0.9f);
    return true;
}

bool AggregatronKeysAudioProcessorEditor::keyStateChanged(bool isKeyDown, juce::Component* originatingComponent)
{
    juce::ignoreUnused(isKeyDown, originatingComponent);
    releaseStaleHeldKeys();
    return false;
}

void AggregatronKeysAudioProcessorEditor::syncComputerKeyboardState()
{
    if (! isShowing() || ! juce::Process::isForegroundProcess())
    {
        releaseComputerKeyboardNotes();
        return;
    }
}

void AggregatronKeysAudioProcessorEditor::releaseStaleHeldKeys()
{
    std::vector<int> releasedKeys;
    releasedKeys.reserve(heldComputerKeys.size());

    for (const auto& entry : heldComputerKeys)
        if (! juce::KeyPress::isKeyCurrentlyDown(entry.first))
            releasedKeys.push_back(entry.first);

    for (const auto keyCode : releasedKeys)
    {
        const auto noteNumber = heldComputerKeys[keyCode];
        audioProcessor.appendDebugLog("[QWERTY] editor keyUp keyCode=" + juce::String(keyCode) + " note=" + juce::String(noteNumber));
        audioProcessor.getKeyboardState().noteOff(1, noteNumber, 0.0f);
        heldComputerKeys.erase(keyCode);
    }
}

void AggregatronKeysAudioProcessorEditor::releaseComputerKeyboardNotes()
{
    for (const auto& entry : heldComputerKeys)
    {
        audioProcessor.appendDebugLog("[QWERTY] editor release keyCode=" + juce::String(entry.first) + " note=" + juce::String(entry.second));
        audioProcessor.getKeyboardState().noteOff(1, entry.second, 0.0f);
    }

    heldComputerKeys.clear();
}

void AggregatronKeysAudioProcessorEditor::applyFactoryPreset(const juce::String& presetName)
{
    if (presetName == "Init")
    {
        setParameterValue("osc1Wave", 0.0f);
        setParameterValue("osc2Wave", 1.0f);
        setParameterValue("oscMix", 0.35f);
        setParameterValue("osc2Detune", 0.0f);
        setParameterValue("osc2Fine", 0.0f);
        setParameterValue("gain", 0.7f);
        setParameterValue("velocityAmp", 0.0f);
        setParameterValue("attack", 0.02f);
        setParameterValue("decay", 0.15f);
        setParameterValue("sustain", 0.8f);
        setParameterValue("release", 0.35f);
        setParameterValue("filterCutoff", 4000.0f);
        setParameterValue("filterResonance", 0.3f);
        setParameterValue("filterEnvAmount", 2500.0f);
        setParameterValue("velocityFilter", 0.0f);
        setParameterValue("filterAttack", 0.01f);
        setParameterValue("filterDecay", 0.2f);
        setParameterValue("filterSustain", 0.1f);
        setParameterValue("filterRelease", 0.3f);
        setParameterValue("lfoRate", 5.0f);
        setParameterValue("lfoPitchDepth", 0.0f);
        setParameterValue("lfoFilterDepth", 0.0f);
        setParameterValue("glide", 0.0f);
        setParameterValue("polyphony", 8.0f);
        setParameterValue("monoMode", 0.0f);
        setParameterValue("drive", 0.15f);
        setParameterValue("reverbMix", 0.18f);
        setParameterValue("reverbSize", 0.45f);
        setParameterValue("reverbDamping", 0.3f);
        return;
    }

    if (presetName == "Soft Pad")
    {
        setParameterValue("osc1Wave", 2.0f);
        setParameterValue("osc2Wave", 3.0f);
        setParameterValue("oscMix", 0.52f);
        setParameterValue("osc2Detune", 0.2f);
        setParameterValue("gain", 0.62f);
        setParameterValue("velocityAmp", 0.35f);
        setParameterValue("attack", 0.65f);
        setParameterValue("decay", 1.2f);
        setParameterValue("sustain", 0.72f);
        setParameterValue("release", 1.8f);
        setParameterValue("filterCutoff", 2600.0f);
        setParameterValue("filterEnvAmount", 1800.0f);
        setParameterValue("velocityFilter", 900.0f);
        setParameterValue("reverbMix", 0.34f);
        setParameterValue("reverbSize", 0.72f);
        setParameterValue("reverbDamping", 0.42f);
        return;
    }

    if (presetName == "Bass Punch")
    {
        setParameterValue("osc1Wave", 0.0f);
        setParameterValue("osc2Wave", 1.0f);
        setParameterValue("oscMix", 0.22f);
        setParameterValue("osc2Detune", -0.08f);
        setParameterValue("gain", 0.74f);
        setParameterValue("velocityAmp", 0.55f);
        setParameterValue("attack", 0.005f);
        setParameterValue("decay", 0.12f);
        setParameterValue("sustain", 0.55f);
        setParameterValue("release", 0.18f);
        setParameterValue("filterCutoff", 1200.0f);
        setParameterValue("filterResonance", 0.46f);
        setParameterValue("filterEnvAmount", 4200.0f);
        setParameterValue("velocityFilter", 2200.0f);
        setParameterValue("drive", 0.32f);
        setParameterValue("reverbMix", 0.06f);
        return;
    }

    if (presetName == "Glass Mono")
    {
        setParameterValue("osc1Wave", 3.0f);
        setParameterValue("osc2Wave", 2.0f);
        setParameterValue("oscMix", 0.64f);
        setParameterValue("osc2Detune", 7.0f);
        setParameterValue("osc2Fine", 6.0f);
        setParameterValue("gain", 0.58f);
        setParameterValue("velocityAmp", 0.48f);
        setParameterValue("attack", 0.01f);
        setParameterValue("decay", 0.24f);
        setParameterValue("sustain", 0.62f);
        setParameterValue("release", 0.22f);
        setParameterValue("filterCutoff", 6200.0f);
        setParameterValue("filterEnvAmount", 1400.0f);
        setParameterValue("velocityFilter", 2600.0f);
        setParameterValue("glide", 0.22f);
        setParameterValue("monoMode", 1.0f);
        setParameterValue("reverbMix", 0.16f);
        return;
    }

    if (presetName == "Wide Lead")
    {
        setParameterValue("osc1Wave", 0.0f);
        setParameterValue("osc2Wave", 0.0f);
        setParameterValue("oscMix", 0.48f);
        setParameterValue("osc2Detune", 0.35f);
        setParameterValue("osc2Fine", 8.0f);
        setParameterValue("gain", 0.68f);
        setParameterValue("velocityAmp", 0.42f);
        setParameterValue("attack", 0.015f);
        setParameterValue("decay", 0.18f);
        setParameterValue("sustain", 0.74f);
        setParameterValue("release", 0.38f);
        setParameterValue("filterCutoff", 5400.0f);
        setParameterValue("filterResonance", 0.36f);
        setParameterValue("filterEnvAmount", 2600.0f);
        setParameterValue("velocityFilter", 1800.0f);
        setParameterValue("lfoRate", 5.8f);
        setParameterValue("lfoPitchDepth", 0.12f);
        setParameterValue("reverbMix", 0.2f);
        setParameterValue("reverbSize", 0.54f);
    }
}

void AggregatronKeysAudioProcessorEditor::timerCallback()
{
    syncComputerKeyboardState();
    releaseStaleHeldKeys();

    if (! pitchWheelSlider.isMouseButtonDown())
        pitchWheelSlider.setValue(static_cast<double>(audioProcessor.getUiPitchWheel()), juce::dontSendNotification);

    if (! modWheelSlider.isMouseButtonDown())
        modWheelSlider.setValue(static_cast<double>(audioProcessor.getUiModWheelNormalized()), juce::dontSendNotification);

    std::vector<float> waveform;
    if (audioProcessor.getWaveformSnapshot(waveform))
        waveformDisplay->setWaveform(waveform);

    // Drain debug log from processor and append to a file for external inspection
    {
        auto logs = audioProcessor.drainDebugLog();
        if (! logs.empty())
        {
            const auto logFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("AggregaKeys_qwerty_log.txt");
            juce::FileOutputStream out(logFile);
            if (out.openedOk())
            {
                out.setPosition(logFile.getSize());
                for (auto& line : logs)
                {
                    out.writeString(line + "\r\n");
                }
            }
        }
    }
}

void AggregatronKeysAudioProcessorEditor::paint(juce::Graphics& g)
{
    if (backgroundImage.isValid())
        g.drawImageWithin(backgroundImage, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::fillDestination);
    else
        g.fillAll(juce::Colour::fromRGB(20, 25, 35));

    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRect(getLocalBounds().toFloat().reduced(12.0f));

    auto paintGroup = [&g](juce::Rectangle<int> bounds)
    {
        if (bounds.isEmpty())
            return;

        auto groupBounds = bounds.toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.12f));
        g.fillRect(groupBounds);
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.drawRect(groupBounds, 1.0f);
    };

    paintGroup(oscGroupBounds);
    paintGroup(ampGroupBounds);
    paintGroup(filterGroupBounds);
    paintGroup(motionGroupBounds);
    paintGroup(performanceGroupBounds);
    paintGroup(fxGroupBounds);
}

void AggregatronKeysAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto header = area.removeFromTop(76);
    auto headerTop = header.removeFromTop(38);

    titleLabel.setBounds(headerTop.removeFromLeft(260));
    auto buttonArea = headerTop.removeFromRight(510);
    versionLabel.setBounds(buttonArea.removeFromRight(110));
    auto loadBounds = buttonArea.removeFromRight(110).reduced(4, 2);
    loadPresetButton.setBounds(loadBounds);
    loadPresetLabel.setBounds(loadBounds);
    auto saveBounds = buttonArea.removeFromRight(110).reduced(4, 2);
    savePresetButton.setBounds(saveBounds);
    savePresetLabel.setBounds(saveBounds);
    auto presetBounds = buttonArea.removeFromRight(190).reduced(4, 2);
    presetMenu.setBounds(presetBounds);
    auto monoBounds = buttonArea.removeFromRight(116).reduced(8, 6);
    monoModeButton.setBounds(monoBounds);
    hintLabel.setBounds(header);

    waveformBounds = area.removeFromTop(86);
    waveformDisplay->setBounds(waveformBounds.reduced(8, 4));
    area.removeFromTop(8);

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

    auto splitGroupedRow = [](juce::Rectangle<int> row, std::initializer_list<juce::Rectangle<int>*> groups)
    {
        const int count = static_cast<int>(groups.size());
        const int gap = 10;
        const int totalGap = gap * juce::jmax(0, count - 1);
        const int groupWidth = (row.getWidth() - totalGap) / juce::jmax(1, count);
        int x = row.getX();

        for (auto* bounds : groups)
        {
            *bounds = juce::Rectangle<int>(x, row.getY(), groupWidth, row.getHeight());
            x += groupWidth + gap;
        }
    };

    splitGroupedRow(row1, { &oscGroupBounds, &ampGroupBounds });
    splitGroupedRow(row3, { &motionGroupBounds, &performanceGroupBounds, &fxGroupBounds });
    filterGroupBounds = row2;

    oscSectionLabel.setBounds(oscGroupBounds.removeFromTop(26).reduced(8, 0));
    ampSectionLabel.setBounds(ampGroupBounds.removeFromTop(26).reduced(8, 0));
    filterSectionLabel.setBounds(filterGroupBounds.removeFromTop(26).reduced(8, 0));
    motionSectionLabel.setBounds(motionGroupBounds.removeFromTop(26).reduced(8, 0));
    performanceSectionLabel.setBounds(performanceGroupBounds.removeFromTop(26).reduced(8, 0));
    fxSectionLabel.setBounds(fxGroupBounds.removeFromTop(26).reduced(8, 0));

    auto oscContent = oscGroupBounds.reduced(8, 0);
    auto oscChoiceRow = oscContent.removeFromTop(56);
    auto oscChoiceWidth = (oscChoiceRow.getWidth() - 12) / 2;
    auto osc1ChoiceArea = oscChoiceRow.removeFromLeft(oscChoiceWidth).reduced(2, 0);
    oscChoiceRow.removeFromLeft(12);
    auto osc2ChoiceArea = oscChoiceRow.reduced(2, 0);
    osc1WaveControl.label.setBounds(osc1ChoiceArea.removeFromTop(22));
    osc1WaveControl.comboBox.setBounds(osc1ChoiceArea.reduced(4, 2));
    osc2WaveControl.label.setBounds(osc2ChoiceArea.removeFromTop(22));
    osc2WaveControl.comboBox.setBounds(osc2ChoiceArea.reduced(4, 2));
    layoutRow(oscContent.reduced(0, 4), { &oscMixControl, &osc2DetuneControl, &osc2FineControl });
    layoutRow(ampGroupBounds.reduced(8, 0), { &gainControl, &velocityAmpControl, &ampAttackControl, &ampDecayControl, &ampSustainControl, &ampReleaseControl });
    layoutRow(filterGroupBounds.reduced(8, 0), { &filterCutoffControl, &filterResonanceControl, &filterEnvAmountControl, &velocityFilterControl, &filterAttackControl, &filterDecayControl, &filterSustainControl, &filterReleaseControl });
    layoutRow(motionGroupBounds.reduced(8, 0), { &lfoRateControl, &lfoPitchDepthControl, &lfoFilterDepthControl });
    layoutRow(performanceGroupBounds.reduced(8, 0), { &glideControl, &polyphonyControl, &driveControl });
    layoutRow(fxGroupBounds.reduced(8, 0), { &reverbMixControl, &reverbSizeControl, &reverbDampingControl });

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
