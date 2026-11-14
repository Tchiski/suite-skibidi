#pragma once

#include "PluginProcessor.h"
#include "TchiskibidiWidgets.h"

class MasterEditor : public tchk::ScaledEditor,
                     private juce::Timer
{
public:
    explicit MasterEditor (MasterProcessor&);
    ~MasterEditor() override;

private:
    // Big round AUTO button
    struct AutoButton : public juce::ToggleButton
    {
        float pulse = 0.0f;
        void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
    };

    // Curve showing what the Spectral module is correcting
    struct SpectralDisplay : public juce::Component
    {
        explicit SpectralDisplay (MasterProcessor& p) : proc (p) {}
        void paint (juce::Graphics& g) override;
        MasterProcessor& proc;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics& g);
    void layout();

    MasterProcessor& audioProcessor;

    // Top bar
    juce::ComboBox presetBox;
    juce::TextButton undoButton { "UNDO" }, redoButton { "REDO" }, aButton { "A" }, bButton { "B" }, copyButton { "A>B" };
    tchk::Toggle bypassButton, matchButton;

    // Hero
    tchk::Knob inputKnob;
    tchk::Meter inputMeter;
    AutoButton autoButton;
    std::unique_ptr<tchk::APVTS::ButtonAttachment> autoAttachment;
    tchk::Choice targetBox;

    // Modules
    tchk::Toggle specOn, compOn, eqOn, stereoOn, analogOn, clipOn, limOn, bassMono;
    tchk::Knob specAmt, glue, detail, bass, air, width, color, clipGain, limGain, ceiling, release, output;
    SpectralDisplay spectralDisplay;
    tchk::Meter grLow, grMid, grHigh, clipMeter, limMeter;
    tchk::Choice analogMode;
    juce::TextButton resetLufsButton { "RESET" };

    // Areas (design coordinates)
    juce::Rectangle<int> heroArea, specArea, compArea, eqArea, stereoArea, analogArea, clipArea, limArea, meterArea;

    // Cached meter values
    float lufsS = -100.0f, lufsI = -100.0f, peak = -100.0f, plr = 0.0f, corr = 1.0f;
    float blink = 0.0f, glowPhase = 0.0f;
    int blinkCountdown = 90;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterEditor)
};
