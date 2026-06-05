#include "ScaleScopeEditor.h"

namespace
{
juce::Colour backgroundColour() { return juce::Colour::fromRGB(17, 24, 30); }
juce::Colour panelColour() { return juce::Colour::fromRGB(27, 37, 46); }
juce::Colour accentColour() { return juce::Colour::fromRGB(83, 190, 255); }
juce::Colour accentStrongColour() { return juce::Colour::fromRGB(255, 188, 69); }
}

class AggregaScaleAudioProcessorEditor::ScaleKeyboardComponent : public juce::Component
{
public:
    void setScaleState(AggregaScaleAudioProcessor::ScaleState newState)
    {
        state = std::move(newState);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(panelColour().brighter(0.05f));
        g.fillRoundedRectangle(bounds, 22.0f);

        auto keyboardArea = bounds.reduced(18.0f);
        constexpr int startMidi = 36;
        constexpr int endMidi = 84;

        int whiteKeyCount = 0;
        for (int midi = startMidi; midi <= endMidi; ++midi)
            if (! juce::MidiMessage::isMidiNoteBlack(midi))
                ++whiteKeyCount;

        const auto whiteWidth = keyboardArea.getWidth() / static_cast<float>(juce::jmax(1, whiteKeyCount));
        const auto blackWidth = whiteWidth * 0.62f;
        const auto blackHeight = keyboardArea.getHeight() * 0.62f;

        std::array<juce::Rectangle<float>, 128> whiteRects {};
        int whiteIndex = 0;
        for (int midi = startMidi; midi <= endMidi; ++midi)
        {
            if (juce::MidiMessage::isMidiNoteBlack(midi))
                continue;

            auto keyBounds = juce::Rectangle<float>(keyboardArea.getX() + whiteWidth * static_cast<float>(whiteIndex),
                                                    keyboardArea.getY(),
                                                    whiteWidth,
                                                    keyboardArea.getHeight());
            whiteRects[static_cast<size_t>(midi)] = keyBounds;

            const auto inScale = state.enabledPitchClasses[static_cast<size_t>(midi % 12)];
            const auto isRoot = (midi % 12) == state.rootNote;
            g.setColour(isRoot ? accentStrongColour() : (inScale ? accentColour().withAlpha(0.72f) : juce::Colours::white));
            g.fillRoundedRectangle(keyBounds.reduced(0.6f), 5.0f);
            g.setColour(juce::Colours::black.withAlpha(0.34f));
            g.drawRoundedRectangle(keyBounds.reduced(0.6f), 5.0f, 1.0f);

            if (midi <= 72)
            {
                auto labelArea = keyBounds.toNearestInt().removeFromBottom(24);
                g.setColour(juce::Colours::black.withAlpha(0.72f));
                g.setFont(juce::FontOptions(12.5f));
                g.drawFittedText(juce::MidiMessage::getMidiNoteName(midi, true, true, 4), labelArea, juce::Justification::centred, 1);
            }

            ++whiteIndex;
        }

        for (int midi = startMidi; midi <= endMidi; ++midi)
        {
            if (! juce::MidiMessage::isMidiNoteBlack(midi))
                continue;

            int previousWhite = midi - 1;
            while (previousWhite >= startMidi && juce::MidiMessage::isMidiNoteBlack(previousWhite))
                --previousWhite;

            if (previousWhite < startMidi)
                continue;

            auto anchor = whiteRects[static_cast<size_t>(previousWhite)];
            auto keyBounds = juce::Rectangle<float>(anchor.getRight() - blackWidth * 0.5f,
                                                    keyboardArea.getY(),
                                                    blackWidth,
                                                    blackHeight);
            const auto inScale = state.enabledPitchClasses[static_cast<size_t>(midi % 12)];
            const auto isRoot = (midi % 12) == state.rootNote;
            g.setColour(inScale ? (isRoot ? accentStrongColour().darker(0.45f) : accentColour().darker(0.68f)) : juce::Colours::black.withAlpha(0.94f));
            g.fillRoundedRectangle(keyBounds, 4.0f);
            g.setColour(juce::Colours::white.withAlpha(inScale ? 0.30f : 0.08f));
            g.drawRoundedRectangle(keyBounds, 4.0f, 1.0f);
        }

        auto legendArea = getLocalBounds().removeFromBottom(34).reduced(20, 0);
        g.setFont(juce::FontOptions(13.0f));
        g.setColour(accentStrongColour());
        g.drawText("Root", legendArea.removeFromLeft(80), juce::Justification::centredLeft);
        g.setColour(accentColour());
        g.drawText("In scale", legendArea.removeFromLeft(90), juce::Justification::centredLeft);
        g.setColour(juce::Colours::white.withAlpha(0.72f));
        g.drawText("Unlit keys are outside the scale", legendArea, juce::Justification::centredLeft);
    }

private:
    AggregaScaleAudioProcessor::ScaleState state;
};

AggregaScaleAudioProcessorEditor::AggregaScaleAudioProcessorEditor(AggregaScaleAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(1080, 640);

    titleLabel.setText("AggregaScale", juce::dontSendNotification);
    titleLabel.setFont(titleFont);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Type a scale manually or infer one from audio, then see every playable note on the keyboard.", juce::dontSendNotification);
    subtitleLabel.setFont(bodyFont);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.82f));
    addAndMakeVisible(subtitleLabel);

    sourceLabel.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    sourceLabel.setColour(juce::Label::textColourId, accentStrongColour());
    addAndMakeVisible(sourceLabel);

    detailLabel.setFont(bodyFont);
    detailLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    detailLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(detailLabel);

    manualLabel.setText("Manual Scale", juce::dontSendNotification);
    manualLabel.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    manualLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(manualLabel);

    customLabel.setText("Custom Notes", juce::dontSendNotification);
    customLabel.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    customLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(customLabel);

    loadButton.setColour(juce::TextButton::buttonColourId, accentColour());
    loadButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    loadButton.onClick = [this]
    {
        if (audioProcessor.isLoadingSource())
            return;

        auto chooser = std::make_shared<juce::FileChooser>("Analyse a pitched audio file", juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.wav;*.mp3;*.aif;*.aiff");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this, chooser](const juce::FileChooser& fc)
                             {
                                 auto file = fc.getResult();
                                 if (file != juce::File())
                                     audioProcessor.startLoadingSourceFile(file);
                             });
    };
    addAndMakeVisible(loadButton);

    for (int i = 0; i < 12; ++i)
    {
        rootCombo.addItem(pitchClassName(i), i + 1);

        auto button = std::make_unique<juce::ToggleButton>(pitchClassName(i));
        button->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
        button->onClick = [this] { if (! suppressManualCallbacks) applyCurrentCustomMask(); };
        addAndMakeVisible(*button);
        noteButtons[static_cast<size_t>(i)] = std::move(button);
    }

    rootCombo.onChange = [this] { if (! suppressManualCallbacks) applyCurrentPreset(); };
    addAndMakeVisible(rootCombo);

    const juce::StringArray scales { "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian", "Major Pentatonic", "Minor Pentatonic" };
    for (int i = 0; i < scales.size(); ++i)
        scaleCombo.addItem(scales[i], i + 1);
    scaleCombo.onChange = [this] { if (! suppressManualCallbacks) applyCurrentPreset(); };
    addAndMakeVisible(scaleCombo);

    keyboardComponent = std::make_unique<ScaleKeyboardComponent>();
    addAndMakeVisible(*keyboardComponent);

    refreshFromProcessor();
    startTimerHz(8);
}

AggregaScaleAudioProcessorEditor::~AggregaScaleAudioProcessorEditor()
{
}

juce::String AggregaScaleAudioProcessorEditor::pitchClassName(int pitchClass)
{
    static const std::array<const char*, 12> names { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return names[static_cast<size_t>(juce::negativeAwareModulo(pitchClass, 12))];
}

void AggregaScaleAudioProcessorEditor::timerCallback()
{
    refreshFromProcessor();
}

void AggregaScaleAudioProcessorEditor::refreshFromProcessor()
{
    const auto state = audioProcessor.getScaleState();
    const auto loading = audioProcessor.isLoadingSource();

    if (state.rootNote == lastState.rootNote
        && state.scaleName == lastState.scaleName
        && state.sourceLabel == lastState.sourceLabel
        && state.detailText == lastState.detailText
        && state.enabledPitchClasses == lastState.enabledPitchClasses
        && loadButton.isEnabled() == ! loading)
    {
        return;
    }

    suppressManualCallbacks = true;
    rootCombo.setSelectedId(state.rootNote + 1, juce::dontSendNotification);
    for (int i = 1; i <= scaleCombo.getNumItems(); ++i)
    {
        if (scaleCombo.getItemText(i - 1) == state.scaleName)
        {
            scaleCombo.setSelectedId(i, juce::dontSendNotification);
            break;
        }
    }
    syncCustomButtons(state.enabledPitchClasses);
    suppressManualCallbacks = false;

    sourceLabel.setText(state.sourceLabel + "  |  " + pitchClassName(state.rootNote) + " " + state.scaleName, juce::dontSendNotification);
    detailLabel.setText(state.detailText, juce::dontSendNotification);
    keyboardComponent->setScaleState(state);
    loadButton.setButtonText(loading
        ? "Analysing " + juce::String(juce::roundToInt(audioProcessor.getLoadingProgress() * 100.0f)) + "%"
        : "Analyse Audio File");
    loadButton.setEnabled(! loading);
    lastState = state;
}

void AggregaScaleAudioProcessorEditor::applyCurrentPreset()
{
    const auto root = juce::jmax(0, rootCombo.getSelectedId() - 1);
    const auto scaleName = scaleCombo.getText();
    if (scaleName.isNotEmpty())
        audioProcessor.applyPresetScale(root, scaleName);
}

void AggregaScaleAudioProcessorEditor::applyCurrentCustomMask()
{
    const auto root = juce::jmax(0, rootCombo.getSelectedId() - 1);
    audioProcessor.applyCustomScale(root, collectCustomMask(), "Custom");
}

std::array<bool, 12> AggregaScaleAudioProcessorEditor::collectCustomMask() const
{
    std::array<bool, 12> mask {};
    for (int i = 0; i < 12; ++i)
        mask[static_cast<size_t>(i)] = noteButtons[static_cast<size_t>(i)]->getToggleState();
    return mask;
}

void AggregaScaleAudioProcessorEditor::syncCustomButtons(const std::array<bool, 12>& enabledPitchClasses)
{
    for (int i = 0; i < 12; ++i)
        noteButtons[static_cast<size_t>(i)]->setToggleState(enabledPitchClasses[static_cast<size_t>(i)], juce::dontSendNotification);
}

void AggregaScaleAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColour());

    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient gradient(backgroundColour().brighter(0.2f), bounds.getTopLeft(), juce::Colour::fromRGB(10, 14, 18), bounds.getBottomRight(), false);
    g.setGradientFill(gradient);
    g.fillAll();

    g.setColour(juce::Colours::white.withAlpha(0.05f));
    for (int x = 0; x < getWidth(); x += 48)
        g.drawVerticalLine(x, 0.0f, static_cast<float>(getHeight()));

    g.setColour(panelColour().withAlpha(0.92f));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(18.0f), 24.0f);
}

void AggregaScaleAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(34);
    auto header = area.removeFromTop(88);
    auto titleRow = header.removeFromTop(36);
    titleLabel.setBounds(titleRow.removeFromLeft(260));
    loadButton.setBounds(titleRow.removeFromRight(210));
    subtitleLabel.setBounds(header);

    auto infoPanel = area.removeFromTop(96);
    sourceLabel.setBounds(infoPanel.removeFromTop(26));
    detailLabel.setBounds(infoPanel);

    area.removeFromTop(10);
    auto controls = area.removeFromTop(150);
    auto manualArea = controls.removeFromLeft(330);
    manualLabel.setBounds(manualArea.removeFromTop(28));
    rootCombo.setBounds(manualArea.removeFromTop(38).removeFromLeft(100));
    manualArea.removeFromTop(10);
    scaleCombo.setBounds(manualArea.removeFromTop(38).removeFromLeft(240));

    auto customArea = controls.reduced(10, 0);
    customLabel.setBounds(customArea.removeFromTop(28));
    auto rowOne = customArea.removeFromTop(48);
    auto rowTwo = customArea.removeFromTop(48);
    const auto buttonWidthOne = rowOne.getWidth() / 6;
    const auto buttonWidthTwo = rowTwo.getWidth() / 6;
    for (int i = 0; i < 6; ++i)
        noteButtons[static_cast<size_t>(i)]->setBounds(rowOne.removeFromLeft(buttonWidthOne).reduced(6, 8));
    for (int i = 6; i < 12; ++i)
        noteButtons[static_cast<size_t>(i)]->setBounds(rowTwo.removeFromLeft(buttonWidthTwo).reduced(6, 8));

    area.removeFromTop(12);
    keyboardComponent->setBounds(area);
}
