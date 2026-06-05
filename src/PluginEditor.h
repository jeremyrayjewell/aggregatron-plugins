#pragma once

#include "PluginProcessor.h"
#include <array>
class OutlinedLabel : public juce::Label
{
public:
    void paint(juce::Graphics& g) override;
};

class AggregatronKeysAudioProcessorEditor : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit AggregatronKeysAudioProcessorEditor(AggregatronKeysAudioProcessor&);
    ~AggregatronKeysAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void focusLost(FocusChangeType cause) override;

private:
    class ImageKnobLookAndFeel;
    class ImageSliderLookAndFeel;
    class ImageComboBoxLookAndFeel;
    class ImageButtonLookAndFeel;
    class WaveformDisplay;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct KnobControl
    {
        juce::Slider slider;
        OutlinedLabel label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct ChoiceControl
    {
        juce::ComboBox comboBox;
        OutlinedLabel label;
        std::unique_ptr<ComboBoxAttachment> attachment;
    };

    void configureSlider(KnobControl& control, const juce::String& parameterId, const juce::String& text);
    void configureChoice(ChoiceControl& control, const juce::String& parameterId, const juce::String& text, const juce::StringArray& items);
    void configureWheel(juce::Slider& slider, OutlinedLabel& label, const juce::String& text, double min, double max, double initial);
    void applyLabelFonts();
    void applyFactoryPreset(const juce::String& presetName);
    void setParameterValue(const juce::String& parameterId, float value);
    void disableKeyboardFocus(juce::Component& component);
    void syncComputerKeyboardState();
    void setComputerKeyboardNoteState(size_t keyIndex, bool isDown);
    void releaseComputerKeyboardNotes();
    void timerCallback() override;

    AggregatronKeysAudioProcessor& audioProcessor;
    juce::Image backgroundImage;
    juce::Image knobImage;
    juce::Image sliderImage;
    juce::Font titleFont;
    juce::Font bodyFont;
    std::unique_ptr<ImageKnobLookAndFeel> knobLookAndFeel;
    std::unique_ptr<ImageSliderLookAndFeel> sliderLookAndFeel;
    std::unique_ptr<ImageComboBoxLookAndFeel> comboBoxLookAndFeel;
    std::unique_ptr<ImageButtonLookAndFeel> buttonLookAndFeel;

    OutlinedLabel titleLabel;
    OutlinedLabel hintLabel;
    OutlinedLabel versionLabel;
    OutlinedLabel oscSectionLabel;
    OutlinedLabel ampSectionLabel;
    OutlinedLabel filterSectionLabel;
    OutlinedLabel motionSectionLabel;
    OutlinedLabel performanceSectionLabel;
    OutlinedLabel fxSectionLabel;
    OutlinedLabel savePresetLabel;
    OutlinedLabel loadPresetLabel;
    juce::TextButton savePresetButton { "" };
    juce::TextButton loadPresetButton { "" };
    juce::ComboBox presetMenu;
    std::unique_ptr<WaveformDisplay> waveformDisplay;
    juce::MidiKeyboardComponent keyboardComponent { audioProcessor.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard };

    KnobControl gainControl;
    KnobControl velocityAmpControl;
    KnobControl ampAttackControl;
    KnobControl ampDecayControl;
    KnobControl ampSustainControl;
    KnobControl ampReleaseControl;
    ChoiceControl osc1WaveControl;
    ChoiceControl osc2WaveControl;
    KnobControl oscMixControl;
    KnobControl osc2DetuneControl;
    KnobControl osc2FineControl;
    KnobControl filterCutoffControl;
    KnobControl filterResonanceControl;
    KnobControl filterEnvAmountControl;
    KnobControl velocityFilterControl;
    KnobControl filterAttackControl;
    KnobControl filterDecayControl;
    KnobControl filterSustainControl;
    KnobControl filterReleaseControl;
    KnobControl lfoRateControl;
    KnobControl lfoPitchDepthControl;
    KnobControl lfoFilterDepthControl;
    KnobControl glideControl;
    KnobControl polyphonyControl;
    KnobControl driveControl;
    KnobControl reverbMixControl;
    KnobControl reverbSizeControl;
    KnobControl reverbDampingControl;
    juce::ToggleButton monoModeButton { "Mono" };
    std::unique_ptr<ButtonAttachment> monoModeAttachment;

    juce::Slider pitchWheelSlider;
    juce::Slider modWheelSlider;
    OutlinedLabel pitchWheelLabel;
    OutlinedLabel modWheelLabel;
    juce::Rectangle<int> waveformBounds;
    juce::Rectangle<int> oscGroupBounds;
    juce::Rectangle<int> ampGroupBounds;
    juce::Rectangle<int> filterGroupBounds;
    juce::Rectangle<int> motionGroupBounds;
    juce::Rectangle<int> performanceGroupBounds;
    juce::Rectangle<int> fxGroupBounds;
    std::array<bool, 17> computerKeyStates {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregatronKeysAudioProcessorEditor)
};
