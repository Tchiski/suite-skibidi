#pragma once

#include "PluginProcessor.h"
#include "TchiskibidiInstrumentUI.h"
#include "TchiskibidiPresets.h"
#include "TwistStickers.h"

class TwistEditor : public tchk::ScaledEditor, private juce::Timer, private juce::ScrollBar::Listener
{
public:
    explicit TwistEditor (TwistProcessor&);
    ~TwistEditor() override;

    // the song timeline with the effect zones
    struct Timeline : public juce::Component
    {
        TwistEditor& ed;
        explicit Timeline (TwistEditor& e) : ed (e) { setWantsKeyboardFocus (true); getProperties().set ("keepFocus", true); }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;
        double beatAt (float x) const;
        float xOf (double beat) const;
        juce::Rectangle<float> zoneRect (const twist::Zone& z) const;
        int dragMode = 0; double dragOff = 0, dragLen = 0;
    };

private:
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double) override;
    void paintContent (juce::Graphics& g);
    void overContent (juce::Graphics& g);
    void layout();
    void commit();          // zones -> processor (and into the undo list)
    void undoRedo (bool redo);   // Cmd+Z / Cmd+Shift+Z
    std::vector<juce::String> undoZ, redoZ;
    void select (int i);
    void refreshFromState();

    TwistProcessor& audioProcessor;
    const juce::Colour coral { 0xffff5f4d };
    std::vector<twist::Zone> zones;
    int selected = -1, chosenFx = 0;
    double viewStart = 0;   // first beat shown
    int viewBars = 16;
    Timeline timeline { *this };
    juce::ScrollBar scroll { false };
    juce::OwnedArray<twist::Sticker> fxChips;
    juce::OwnedArray<tchk::Chip> zoomChips, densChips;
    juce::Slider amount, curve, mix;
    tchk::Chip delB { "Effacer", coral }, sprinkleB { TK_FR ("☄ Saupoudrer"), coral }, followB { "Suivre la lecture", coral }, magnetB { "Aimant : sur les coups", coral };
    bool magnet = true;
    tchk::TransportBadge badge;
    juce::ComboBox presetBox;
    tchk::HeartButton heartButton;
    std::unique_ptr<tchk::PresetSystem> presets;
    int density = 1, lastVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwistEditor)
};
