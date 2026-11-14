#pragma once

#include "PluginProcessor.h"
#include "TchiskibidiLookAndFeel.h"

class TchiskibidiSpaceEditor : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit TchiskibidiSpaceEditor (TchiskibidiSpaceProcessor&);
    ~TchiskibidiSpaceEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void timerCallback() override;
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name, juce::Colour accent);
    void setupToggle (juce::ToggleButton&, std::unique_ptr<ButtonAttachment>&,
                      const juce::String& paramId, juce::Colour accent);
    static void placeKnob (Knob&, juce::Rectangle<int> area);

    TchiskibidiSpaceProcessor& audioProcessor;
    TchiskibidiLookAndFeel lookAndFeel; // must be declared before the components

    Knob time, feedback, wobble, echoLevel, lowCut, highCut;
    Knob size, damping, width, reverbLevel;
    Knob mix, output;

    juce::ToggleButton syncButton { "Sync" }, pingPongButton { "Ping-pong" };
    juce::ComboBox divisionBox;
    std::unique_ptr<ButtonAttachment> syncAttachment, pingPongAttachment;
    std::unique_ptr<ComboBoxAttachment> divisionAttachment;

    juce::Rectangle<int> headerArea, echoArea, reverbArea, masterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TchiskibidiSpaceEditor)
};
