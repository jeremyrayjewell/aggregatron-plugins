#include "SampleMapEditor.h"

namespace
{
juce::Font createFontFromBinary(const char* data, int size, float height)
{
    auto typeface = juce::Typeface::createSystemTypefaceFor(data, static_cast<size_t>(size));
    if (typeface != nullptr)
        return juce::Font(juce::FontOptions(typeface).withHeight(height));

    return juce::Font(juce::FontOptions(height));
}

int keyPressToMidiNote(const juce::KeyPress& key)
{
    static const std::vector<std::pair<juce::juce_wchar, int>> mapping
    {
        { 'a', 60 }, { 'w', 61 }, { 's', 62 }, { 'e', 63 }, { 'd', 64 },
        { 'f', 65 }, { 't', 66 }, { 'g', 67 }, { 'y', 68 }, { 'h', 69 },
        { 'u', 70 }, { 'j', 71 }, { 'k', 72 }, { 'o', 73 }, { 'l', 74 },
        { 'p', 75 }, { ';', 76 }, { '\'', 77 }
    };

    const auto c = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
    for (const auto& entry : mapping)
        if (entry.first == c)
            return entry.second;

    return -1;
}
}

void OutlinedMapLabel::paint(juce::Graphics& g)
{
    auto labelText = getText();
    if (labelText.isEmpty())
        return;

    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(getFont(), labelText, 0.0f, getFont().getAscent());

    juce::Path path;
    glyphs.createPath(path);

    auto pathBounds = path.getBounds();
    auto area = getLocalBounds().toFloat().reduced(8.0f, 2.0f);
    auto x = area.getX();

    if (getJustificationType().testFlags(juce::Justification::horizontallyCentred))
        x = area.getCentreX() - pathBounds.getWidth() * 0.5f;
    else if (getJustificationType().testFlags(juce::Justification::right))
        x = area.getRight() - pathBounds.getWidth();

    const auto y = area.getCentreY() - pathBounds.getHeight() * 0.5f;
    auto transformed = path;
    transformed.applyTransform(juce::AffineTransform::translation(x - pathBounds.getX(), y - pathBounds.getY()));

    g.setColour(juce::Colours::black);
    g.strokePath(transformed, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(findColour(juce::Label::textColourId));
    g.fillPath(transformed);
}

AggregaMapAudioProcessorEditor::AggregaMapAudioProcessorEditor(AggregaMapAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      backgroundImage(juce::ImageCache::getFromMemory(BinaryData::bg_png, BinaryData::bg_pngSize)),
      titleFont(createFontFromBinary(BinaryData::font_ttf, BinaryData::font_ttfSize, 30.0f)),
      bodyFont(createFontFromBinary(BinaryData::font_ttf, BinaryData::font_ttfSize, 18.0f))
{
    setSize(1080, 620);
    setWantsKeyboardFocus(true);
    addKeyListener(this);

    titleLabel.setText("AggregaMap", juce::dontSendNotification);
    titleLabel.setFont(titleFont);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Load a WAV or MP3, wait for mapping to finish, then play from QWERTY, MIDI controller, or the on-screen keyboard.", juce::dontSendNotification);
    hintLabel.setFont(bodyFont);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(hintLabel);

    versionLabel.setText(audioProcessor.getVersionString(), juce::dontSendNotification);
    versionLabel.setFont(bodyFont);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(versionLabel);

    statusLabel.setFont(bodyFont);
    statusLabel.setJustificationType(juce::Justification::topLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setText(audioProcessor.getAnalysisSummary(), juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    minSegmentLabel.setText("Min Segment", juce::dontSendNotification);
    minSegmentLabel.setFont(bodyFont);
    minSegmentLabel.setJustificationType(juce::Justification::centredLeft);
    minSegmentLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(minSegmentLabel);

    maxSegmentLabel.setText("Max Segment", juce::dontSendNotification);
    maxSegmentLabel.setFont(bodyFont);
    maxSegmentLabel.setJustificationType(juce::Justification::centredLeft);
    maxSegmentLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(maxSegmentLabel);

    auto configureSegmentSlider = [this](juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 24);
        slider.setTextValueSuffix(" ms");
        slider.setColour(juce::Slider::trackColourId, juce::Colours::deepskyblue.withAlpha(0.85f));
        slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha(0.30f));
        addAndMakeVisible(slider);
    };

    configureSegmentSlider(minSegmentSlider);
    configureSegmentSlider(maxSegmentSlider);
    minSegmentAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.getParameters(), "minSegmentMs", minSegmentSlider);
    maxSegmentAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.getParameters(), "maxSegmentMs", maxSegmentSlider);

    loadButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black.withAlpha(0.25f));
    loadButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::black.withAlpha(0.35f));
    loadButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    loadButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible(loadButton);

    levelMatchToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    levelMatchToggle.setClickingTogglesState(true);
    levelMatchToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(levelMatchToggle);
    levelMatchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.getParameters(), "levelMatch", levelMatchToggle);

    loadButton.onClick = [this]
    {
        if (audioProcessor.isLoadingSource())
            return;

        auto chooser = std::make_shared<juce::FileChooser>("Load source audio", juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.wav;*.mp3;*.aif;*.aiff");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this, chooser](const juce::FileChooser& fc)
                             {
                                 auto file = fc.getResult();
                                 if (file != juce::File())
                                     audioProcessor.startLoadingSourceFile(file);
                             });
    };

    keyboardComponent.setAvailableRange(24, 96);
    keyboardComponent.setLowestVisibleKey(24);
    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colours::white.withAlpha(0.94f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colours::black.withAlpha(0.82f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colours::deepskyblue.withAlpha(0.42f));
    addAndMakeVisible(keyboardComponent);

    startTimerHz(12);
}

AggregaMapAudioProcessorEditor::~AggregaMapAudioProcessorEditor()
{
    removeKeyListener(this);
}

bool AggregaMapAudioProcessorEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    if (audioProcessor.isLoadingSource())
        return false;

    const auto midiNote = keyPressToMidiNote(key);
    if (midiNote < 0)
        return false;

    const auto keyCode = key.getKeyCode();
    if (heldKeys.find(keyCode) != heldKeys.end())
        return true;

    heldKeys[keyCode] = midiNote;
    audioProcessor.getKeyboardState().noteOn(1, midiNote, 0.85f);
    return true;
}

bool AggregaMapAudioProcessorEditor::keyStateChanged(bool isKeyDown, juce::Component*)
{
    juce::ignoreUnused(isKeyDown);
    releaseStaleHeldKeys();
    return false;
}

void AggregaMapAudioProcessorEditor::releaseStaleHeldKeys()
{
    std::vector<int> released;
    for (const auto& entry : heldKeys)
        if (! juce::KeyPress::isKeyCurrentlyDown(entry.first))
            released.push_back(entry.first);

    for (const auto keyCode : released)
    {
        const auto midiNote = heldKeys[keyCode];
        audioProcessor.getKeyboardState().noteOff(1, midiNote, 0.0f);
        heldKeys.erase(keyCode);
    }
}

void AggregaMapAudioProcessorEditor::timerCallback()
{
    const auto loading = audioProcessor.isLoadingSource();
    const auto statusText = audioProcessor.getAnalysisSummary();
    const auto progressPercent = juce::roundToInt(audioProcessor.getLoadingProgress() * 100.0f);
    bool needsRepaint = false;

    if (! loading)
        releaseStaleHeldKeys();

    if (statusText != lastStatusText)
    {
        statusLabel.setText(statusText, juce::dontSendNotification);
        lastStatusText = statusText;
        needsRepaint = true;
    }

    if (loading != wasLoading)
    {
        titleLabel.setVisible(! loading);
        hintLabel.setVisible(! loading);
        versionLabel.setVisible(! loading);
        statusLabel.setVisible(! loading);
        minSegmentLabel.setVisible(! loading);
        maxSegmentLabel.setVisible(! loading);
        minSegmentSlider.setVisible(! loading);
        maxSegmentSlider.setVisible(! loading);
        loadButton.setVisible(! loading);
        levelMatchToggle.setVisible(! loading);
        keyboardComponent.setVisible(! loading);
        wasLoading = loading;
        needsRepaint = true;
    }

    if (loading)
    {
        if (progressPercent != lastProgressPercent)
        {
            loadButton.setButtonText("Loading " + juce::String(progressPercent) + "%");
            lastProgressPercent = progressPercent;
            needsRepaint = true;
        }

        if (loadButton.isEnabled())
            loadButton.setEnabled(false);
    }
    else
    {
        if (loadButton.getButtonText() != "Load Source")
            loadButton.setButtonText("Load Source");

        if (! loadButton.isEnabled())
            loadButton.setEnabled(true);

        if (lastProgressPercent != progressPercent)
        {
            lastProgressPercent = progressPercent;
            needsRepaint = true;
        }
    }

    if (needsRepaint || loading)
        repaint();
}

void AggregaMapAudioProcessorEditor::paint(juce::Graphics& g)
{
    if (backgroundImage.isValid())
        g.drawImageWithin(backgroundImage, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::fillDestination);
    else
        g.fillAll(juce::Colour::fromRGB(20, 25, 35));

    if (! audioProcessor.isLoadingSource())
    {
        g.setColour(juce::Colours::black.withAlpha(0.20f));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(14.0f), 18.0f);
        return;
    }

    g.setColour(juce::Colours::black.withAlpha(0.68f));
    g.fillAll();

    auto overlay = getLocalBounds().toFloat().reduced(250.0f, 190.0f);
    g.setColour(juce::Colour::fromRGB(14, 18, 24).withAlpha(0.96f));
    g.fillRoundedRectangle(overlay, 24.0f);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(overlay, 24.0f, 1.0f);

    auto titleArea = overlay.removeFromTop(54.0f);
    g.setColour(juce::Colours::white);
    g.setFont(titleFont);
    g.drawFittedText("Loading Source", titleArea.toNearestInt(), juce::Justification::centred, 1);

    auto messageArea = overlay.removeFromTop(88.0f).reduced(26.0f, 8.0f).toNearestInt();
    g.setFont(bodyFont);
    g.drawFittedText(audioProcessor.getAnalysisSummary(), messageArea, juce::Justification::centred, 3);

    auto percentText = juce::String(juce::roundToInt(audioProcessor.getLoadingProgress() * 100.0f)) + "%";
    g.drawFittedText(percentText, overlay.removeFromTop(30.0f).toNearestInt(), juce::Justification::centred, 1);

    auto barArea = overlay.removeFromTop(26.0f).reduced(30.0f, 4.0f);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.fillRoundedRectangle(barArea, 10.0f);
    g.setColour(juce::Colours::deepskyblue.withAlpha(0.94f));
    g.fillRoundedRectangle(barArea.withWidth(barArea.getWidth() * audioProcessor.getLoadingProgress()), 10.0f);
}

void AggregaMapAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto header = area.removeFromTop(92);
    auto topLine = header.removeFromTop(42);

    titleLabel.setBounds(topLine.removeFromLeft(250));
    versionLabel.setBounds(topLine.removeFromRight(90));
    loadButton.setBounds(topLine.removeFromRight(200).reduced(6, 2));
    levelMatchToggle.setBounds(topLine.removeFromRight(180).reduced(6, 4));
    hintLabel.setBounds(header);

    auto infoArea = area.removeFromTop(220);
    auto statusArea = infoArea.removeFromTop(118);
    statusLabel.setBounds(statusArea.reduced(10));

    auto segmentControls = infoArea.reduced(10, 0);
    auto minRow = segmentControls.removeFromTop(42);
    minSegmentLabel.setBounds(minRow.removeFromLeft(180));
    minSegmentSlider.setBounds(minRow);

    segmentControls.removeFromTop(10);
    auto maxRow = segmentControls.removeFromTop(42);
    maxSegmentLabel.setBounds(maxRow.removeFromLeft(180));
    maxSegmentSlider.setBounds(maxRow);

    area.removeFromTop(8);
    keyboardComponent.setBounds(area.withHeight(140).withY(area.getY() + (area.getHeight() - 140) / 2));

    int whiteKeys = 0;
    for (int midi = 24; midi <= 96; ++midi)
        if (! juce::MidiMessage::isMidiNoteBlack(midi))
            ++whiteKeys;

    keyboardComponent.setKeyWidth(static_cast<float>(keyboardComponent.getWidth()) / static_cast<float>(juce::jmax(1, whiteKeys)));
}



