#pragma once

#include "PluginProcessor.h"
#include "TchiskibidiInstrumentUI.h"
#include "TchiskibidiPresets.h"

class BasseEditor : public tchk::ScaledEditor, private juce::Timer
{
public:
    explicit BasseEditor (BasseProcessor&);
    ~BasseEditor() override;

private:
    void timerCallback() override;
    void paintContent (juce::Graphics& g);
    void overContent (juce::Graphics& g);
    void layout();
    void regenerate();
    void regenerateNoHistory();
    void refreshFromState();
    void stepSound (int d);
    std::vector<std::tuple<int, int, juce::String>> chordLabels() const;

    BasseProcessor& audioProcessor;
    const juce::Colour pink { 0xffff4d7d };
    tchk::Wheel keyWheel;
    juce::OwnedArray<tchk::Chip> scaleChips, styleChips, barChips;
    tchk::Chip follow { TK_FR ("◎ Suivre mes accords"), pink };
    tchk::Chip prevB { juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x80")), pink }, nextB { juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")), pink };
    tchk::LavaLamp hole;
    tchk::Choice soundBox;
    tchk::Knob lengthK, glideK, subK, distK, slidesK;
    tchk::Chip undoB { TK_FR ("↶ Revenir"), pink };
    void pushHistory();
    void doUndo();
    void doRedo();
    void shiftOctave (int d);
    tchk::Chip octDown { juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")), pink }, octUp { "+", pink };
    tchk::PianoRoll roll;
    tchk::TransportBadge badge;
    tchk::DragOut dragMidi, dragAudio;
    juce::ComboBox presetBox;
    tchk::HeartButton heartButton;
    std::unique_ptr<tchk::PresetSystem> presets;
    int lastVersion = -1, lastPlayed = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasseEditor)
};
