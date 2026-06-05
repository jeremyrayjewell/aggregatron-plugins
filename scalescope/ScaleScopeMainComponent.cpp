#include "ScaleScopeMainComponent.h"

namespace
{
juce::Colour backgroundColour() { return juce::Colour::fromRGB(17, 24, 30); }
juce::Colour panelColour() { return juce::Colour::fromRGB(27, 37, 46); }
juce::Colour accentColour() { return juce::Colour::fromRGB(83, 190, 255); }
juce::Colour accentStrongColour() { return juce::Colour::fromRGB(255, 188, 69); }
}

AggregaScaleMainComponent::AggregaScaleMainComponent(AggregaScaleState& s)
    : scaleState(s),
      scaleNames({ "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian", "Major Pentatonic", "Minor Pentatonic" })
{
    setSize(1080, 640);
    applyScale(0, scaleNames[currentScaleIndex]);
}

AggregaScaleMainComponent::~AggregaScaleMainComponent() = default;

juce::String AggregaScaleMainComponent::pitchClassName(int pitchClass)
{
    return AggregaScaleState::pitchClassName(pitchClass);
}

void AggregaScaleMainComponent::applyScale(int rootNote, const juce::String& scaleName)
{
    scaleState.applyPresetScale(rootNote, scaleName);
    repaint();
}

void AggregaScaleMainComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient gradient(backgroundColour().brighter(0.2f), bounds.getTopLeft(), juce::Colour::fromRGB(10, 14, 18), bounds.getBottomRight(), false);
    g.setGradientFill(gradient);
    g.fillAll();

    g.setColour(panelColour().withAlpha(0.92f));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(18.0f), 24.0f);

    const auto state = scaleState.getScaleInfo();

    auto area = getLocalBounds().reduced(34);
    auto header = area.removeFromTop(140);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(30.0f).withStyle("Bold"));
    g.drawFittedText("AggregaScale", header.removeFromTop(36), juce::Justification::centredLeft, 1);
    g.setFont(juce::FontOptions(16.0f));
    g.drawFittedText("Manual scale view: click arrows to change root and mode.", header.removeFromTop(28), juce::Justification::centredLeft, 1);
    g.setColour(accentStrongColour());
    g.drawFittedText(pitchClassName(state.rootNote) + " " + state.scaleName, header.removeFromTop(32), juce::Justification::centredLeft, 1);
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.drawFittedText(state.detailText, header, juce::Justification::centredLeft, 2);

    auto controlRow = area.removeFromTop(68);
    auto drawButton = [&](juce::Rectangle<int> rect, const juce::String& text)
    {
        g.setColour(accentColour().withAlpha(0.9f));
        g.fillRoundedRectangle(rect.toFloat(), 10.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions(24.0f).withStyle("Bold"));
        g.drawFittedText(text, rect, juce::Justification::centred, 1);
    };

    prevRootButton = controlRow.removeFromLeft(54).reduced(6);
    drawButton(prevRootButton, "-");
    auto rootLabel = controlRow.removeFromLeft(180);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(20.0f).withStyle("Bold"));
    g.drawFittedText("Root: " + pitchClassName(state.rootNote), rootLabel, juce::Justification::centred, 1);
    nextRootButton = controlRow.removeFromLeft(54).reduced(6);
    drawButton(nextRootButton, "+");

    controlRow.removeFromLeft(20);
    prevScaleButton = controlRow.removeFromLeft(54).reduced(6);
    drawButton(prevScaleButton, "-");
    auto scaleLabel = controlRow.removeFromLeft(300);
    g.drawFittedText("Scale: " + state.scaleName, scaleLabel, juce::Justification::centred, 1);
    nextScaleButton = controlRow.removeFromLeft(54).reduced(6);
    drawButton(nextScaleButton, "+");

    area.removeFromTop(12);
    auto keyboardArea = area.toFloat().reduced(0.0f, 8.0f);
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

        auto keyBounds = juce::Rectangle<float>(keyboardArea.getX() + whiteWidth * static_cast<float>(whiteIndex), keyboardArea.getY(), whiteWidth, keyboardArea.getHeight());
        whiteRects[static_cast<size_t>(midi)] = keyBounds;

        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(keyBounds.reduced(0.6f), 5.0f);
        g.setColour(juce::Colours::black.withAlpha(0.34f));
        g.drawRoundedRectangle(keyBounds.reduced(0.6f), 5.0f, 1.0f);

        if (state.enabledPitchClasses[static_cast<size_t>(midi % 12)])
        {
            const auto isRoot = (midi % 12) == state.rootNote;
            const auto dotDiameter = juce::jmin(keyBounds.getWidth() * 0.54f, 34.0f);
            auto dotBounds = juce::Rectangle<float>(dotDiameter, dotDiameter).withCentre({ keyBounds.getCentreX(), keyBounds.getBottom() - 38.0f });
            g.setColour(isRoot ? accentStrongColour() : accentColour());
            g.fillEllipse(dotBounds);
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.drawEllipse(dotBounds, 1.0f);
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
        g.fillRoundedRectangle(keyBounds, 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(keyBounds, 4.0f, 1.0f);

        if (state.enabledPitchClasses[static_cast<size_t>(midi % 12)])
        {
            const auto isRoot = (midi % 12) == state.rootNote;
            const auto dotDiameter = juce::jmin(keyBounds.getWidth() * 0.62f, 24.0f);
            auto dotBounds = juce::Rectangle<float>(dotDiameter, dotDiameter).withCentre({ keyBounds.getCentreX(), keyBounds.getBottom() - 18.0f });
            g.setColour(isRoot ? accentStrongColour() : accentColour());
            g.fillEllipse(dotBounds);
            g.setColour(juce::Colours::white.withAlpha(0.22f));
            g.drawEllipse(dotBounds, 1.0f);
        }
    }

    auto legend = getLocalBounds().removeFromBottom(34).reduced(34, 0);
    auto drawLegendDot = [&](juce::Colour colour, const juce::String& text)
    {
        auto dotArea = legend.removeFromLeft(18).toFloat().withSizeKeepingCentre(12.0f, 12.0f);
        g.setColour(colour);
        g.fillEllipse(dotArea);
        legend.removeFromLeft(8);
        g.setColour(juce::Colours::white.withAlpha(0.82f));
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(text, legend.removeFromLeft(120), juce::Justification::centredLeft);
        legend.removeFromLeft(16);
    };

    drawLegendDot(accentStrongColour(), "Root note");
    drawLegendDot(accentColour(), "In scale");
}

void AggregaScaleMainComponent::resized()
{
}

void AggregaScaleMainComponent::mouseUp(const juce::MouseEvent& event)
{
    const auto state = scaleState.getScaleInfo();
    auto rootNote = state.rootNote;

    if (prevRootButton.contains(event.getPosition()))
        rootNote = juce::negativeAwareModulo(rootNote - 1, 12);
    else if (nextRootButton.contains(event.getPosition()))
        rootNote = juce::negativeAwareModulo(rootNote + 1, 12);
    else if (prevScaleButton.contains(event.getPosition()))
        currentScaleIndex = juce::negativeAwareModulo(currentScaleIndex - 1, scaleNames.size());
    else if (nextScaleButton.contains(event.getPosition()))
        currentScaleIndex = juce::negativeAwareModulo(currentScaleIndex + 1, scaleNames.size());
    else
        return;

    applyScale(rootNote, scaleNames[currentScaleIndex]);
}
