#pragma once

#include "PluginProcessor.h"
#include "TchiskibidiInstrumentUI.h"
#include "TchiskibidiPresets.h"

class AccordsEditor : public tchk::ScaledEditor, private juce::Timer
{
public:
    explicit AccordsEditor (AccordsProcessor&);
    ~AccordsEditor() override;

private:
    void timerCallback() override;
    void paintContent (juce::Graphics& g);
    void layout();
    void regenerate (const std::vector<int>* avoid = nullptr, bool keepHistory = true);
    void refreshFromState();
    void openListen();
    void analyseListen();
    std::vector<std::tuple<int, int, juce::String>> chordLabels() const;

    AccordsProcessor& audioProcessor;
    const juce::Colour gold { 0xffffd34d };
    tchk::Wheel keyWheel;
    juce::OwnedArray<tchk::Chip> scaleChips, barChips, moodChips;
    tchk::Chip seventh { TK_FR ("Accords à 4 notes (7e)"), gold }, listen { TK_FR ("◎ Écouter mes pistes"), gold };
    tchk::Star3D star;
    tchk::Chip undoB { TK_FR ("↶ Revenir"), gold };
    void pushHistory();
    void doUndo();
    void doRedo();
    void shiftOctave (int d);
    tchk::Chip octDown { juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")), gold }, octUp { "+", gold };
    tchk::PianoRoll roll;
    tchk::TransportBadge badge;
    tchk::DragOut dragMidi, dragAudio;
    tchk::Choice soundBox;
    tchk::Knob volume;
    juce::ComboBox presetBox;
    tchk::HeartButton heartButton;
    std::unique_ptr<tchk::PresetSystem> presets;

    // listening panel
    struct ListenPanel : public juce::Component
    {
        juce::String status, keyText; std::vector<juce::String> chords;
        float progress = 0;
        void paint (juce::Graphics& g) override;
    };
    ListenPanel panel;
    tchk::Chip srcMidi { "MIDI entrant", gold }, srcAudio { "Audio (sidechain)", gold }, genMatch { TK_FR ("★ Générer une progression qui matche"), gold }, closeListen { "Fermer", gold };
    int heardRoot = 0; bool heardMinor = true; std::vector<int> heardDegrees;
    int lastVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AccordsEditor)
};
