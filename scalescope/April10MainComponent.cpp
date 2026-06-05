#include "April10MainComponent.h"

namespace
{
juce::Colour backgroundColour() { return juce::Colour::fromRGB(16, 18, 24); }
juce::Colour panelColour() { return juce::Colour::fromRGB(24, 29, 38); }
juce::Colour accentColour() { return juce::Colour::fromRGB(122, 226, 184); }
juce::Colour accentStrongColour() { return juce::Colour::fromRGB(255, 194, 92); }

constexpr std::array<April10MainComponent::TrackPreset, 11> trackPresets
{{
    { "AGGREGTRON",      125, 8,  "Major",         "Ab Major" },
    { "EVERY DAY",       130, 11, "Natural Minor", "B Minor" },
    { "TRACE EXECUTION", 120, 9,  "Natural Minor", "A Minor" },
    { "CLEAN SLOPES",    120, 7,  "Major",         "G Major" },
    { "SEGFAULT APORIA", 120, 1,  "Natural Minor", "C# Minor" },
    { "FORGETFUL ME",     80, 7,  "Major",         "G Major" },
    { "MANY MEZZANINES", 109, 5,  "Major",         "F Major" },
    { "OWN DEVICES",      70, 2,  "Major",         "D Major" },
    { "KALIKA DIRGE",     80, 7,  "Major",         "G Major" },
    { "MARVIKHER",       119, 3,  "Natural Minor", "Eb Minor" },
    { "MOQUETTE",         90, 9,  "Natural Minor", "A Minor" }
}};
}

April10MainComponent::April10MainComponent(AggregaScaleState& s)
    : scaleState(s)
{
    setSize(430, 120);
    applyCurrentPreset();
}

April10MainComponent::~April10MainComponent() = default;

juce::String April10MainComponent::pitchClassName(int pitchClass)
{
    return AggregaScaleState::pitchClassName(pitchClass);
}

void April10MainComponent::applyCurrentPreset()
{
    const auto& preset = trackPresets[static_cast<size_t>(currentTrackIndex)];
    scaleState.applyPresetScale(preset.rootNote, preset.scaleName);
    repaint();
}

void April10MainComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient gradient(backgroundColour().brighter(0.16f), bounds.getTopLeft(), juce::Colour::fromRGB(8, 10, 15), bounds.getBottomRight(), false);
    g.setGradientFill(gradient);
    g.fillAll();

    g.setColour(panelColour().withAlpha(0.97f));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(4.0f), 10.0f);

    const auto state = scaleState.getScaleInfo();
    const auto& preset = trackPresets[static_cast<size_t>(currentTrackIndex)];

    auto area = getLocalBounds().reduced(6);
    auto topStrip = area.removeFromTop(14);
    auto drawButton = [&](juce::Rectangle<int> rect, const juce::String& text)
    {
        g.setColour(accentColour().withAlpha(0.92f));
        g.fillRoundedRectangle(rect.toFloat(), 3.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        g.drawFittedText(text, rect, juce::Justification::centred, 1);
    };

    auto contentRow = topStrip.removeFromLeft(getWidth() - 16);

    prevTrackButton = contentRow.removeFromLeft(12);
    drawButton(prevTrackButton, "<");

    contentRow.removeFromLeft(3);
    auto titleArea = contentRow.removeFromLeft(185);
    g.setColour(accentStrongColour());
    g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    g.drawFittedText(preset.title, titleArea, juce::Justification::centredLeft, 1);

    contentRow.removeFromLeft(4);
    auto detailsArea = contentRow.removeFromLeft(92);
    g.setColour(juce::Colours::white.withAlpha(0.86f));
    g.setFont(juce::FontOptions(8.5f));
    g.drawFittedText(preset.displayScale + juce::String(" " ) + juce::String(preset.bpm), detailsArea, juce::Justification::centredLeft, 1);

    contentRow.removeFromLeft(3);
    nextTrackButton = contentRow.removeFromLeft(12);
    drawButton(nextTrackButton, ">");

    area.removeFromTop(2);
    auto keyboardArea = area.removeFromTop(88).toFloat();
    constexpr int startMidi = 48;
    constexpr int endMidi = 72;

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

        auto keyBounds = juce::Rectangle<float>(keyboardArea.getX() + whiteWidth * static_cast<float>(whiteIndex), keyboardArea.getY(), whiteWidth, keyboardArea.getHeight());
        whiteRects[static_cast<size_t>(midi)] = keyBounds;

        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(keyBounds.reduced(0.35f), 3.0f);
        g.setColour(juce::Colours::black.withAlpha(0.34f));
        g.drawRoundedRectangle(keyBounds.reduced(0.35f), 3.0f, 0.8f);

        if (state.enabledPitchClasses[static_cast<size_t>(midi % 12)])
        {
            const auto isRoot = (midi % 12) == state.rootNote;
            const auto dotDiameter = juce::jmin(keyBounds.getWidth() * 0.45f, 16.0f);
            auto dotBounds = juce::Rectangle<float>(dotDiameter, dotDiameter).withCentre({ keyBounds.getCentreX(), keyBounds.getBottom() - 15.0f });
            g.setColour(isRoot ? accentStrongColour() : accentColour());
            g.fillEllipse(dotBounds);
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.drawEllipse(dotBounds, 0.8f);
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
        auto keyBounds = juce::Rectangle<float>(anchor.getRight() - blackWidth * 0.5f, keyboardArea.getY(), blackWidth, blackHeight);
        g.setColour(juce::Colours::black.withAlpha(0.94f));
        g.fillRoundedRectangle(keyBounds, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(keyBounds, 3.0f, 0.8f);

        if (state.enabledPitchClasses[static_cast<size_t>(midi % 12)])
        {
            const auto isRoot = (midi % 12) == state.rootNote;
            const auto dotDiameter = juce::jmin(keyBounds.getWidth() * 0.55f, 10.0f);
            auto dotBounds = juce::Rectangle<float>(dotDiameter, dotDiameter).withCentre({ keyBounds.getCentreX(), keyBounds.getBottom() - 10.0f });
            g.setColour(isRoot ? accentStrongColour() : accentColour());
            g.fillEllipse(dotBounds);
            g.setColour(juce::Colours::white.withAlpha(0.22f));
            g.drawEllipse(dotBounds, 0.8f);
        }
    }

}

void April10MainComponent::resized()
{
}

void April10MainComponent::mouseUp(const juce::MouseEvent& event)
{
    if (prevTrackButton.contains(event.getPosition()))
        currentTrackIndex = juce::negativeAwareModulo(currentTrackIndex - 1, static_cast<int>(trackPresets.size()));
    else if (nextTrackButton.contains(event.getPosition()))
        currentTrackIndex = juce::negativeAwareModulo(currentTrackIndex + 1, static_cast<int>(trackPresets.size()));
    else
        return;

    applyCurrentPreset();
}
