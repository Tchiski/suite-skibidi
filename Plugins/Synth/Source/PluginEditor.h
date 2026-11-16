#pragma once

#include "PluginProcessor.h"
#include "TchiskibidiInstrumentUI.h"
#include "TchiskibidiPresets.h"

class SynthEditor : public tchk::ScaledEditor, private juce::Timer
{
public:
    explicit SynthEditor (SynthProcessor&);
    ~SynthEditor() override;

private:
    void timerCallback() override;
    void paintContent (juce::Graphics& g);
    void overContent (juce::Graphics& g);
    void layout();
    void refreshPresetUI();
    void roll();      // HASARD
    void undo();
    void demo();
    std::vector<float> snapshot() const;
    void animateTo (const std::vector<float>& target);

    SynthProcessor& audioProcessor;
    const juce::Colour orange { 0xffff8a3d };
    // the big sphere: name of the sound, click = list of all the sounds of the family
    struct Orb : public juce::Component
    {
        juce::Colour accent, accent2;
        juce::String family, name;
        int cat = 0;
        std::function<void()> onClick;
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    };
    Orb orb;
    void openList();
    juce::OwnedArray<tchk::Chip> catChips, octChips;
    tchk::Chip prevB { juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x80")), orange }, nextB { juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")), orange };
    tchk::Chip linkB { TK_FR ("⛓ Jouer mes accords"), orange };
    juce::String lastLinkText;
    tchk::Chip monoB { "Mono", orange }, demoB { TK_FR ("▶ Écouter le preset"), orange }, backB { TK_FR ("↶ Revenir"), orange };
    juce::OwnedArray<tchk::Knob> knobs;
    tchk::Knob volume;
    tchk::Comet comet;
    juce::ComboBox presetBox;
    tchk::HeartButton heartButton;
    std::unique_ptr<tchk::PresetSystem> presets;
    std::vector<std::vector<float>> history;
    std::vector<float> animFrom, animTo; double animStart = -1;
    int lastPreset = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthEditor)
};
