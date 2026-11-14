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

    // Loudness gauge (LUFS) drawn around the mascot
    struct LoudnessGauge : public juce::Component
    {
        explicit LoudnessGauge (MasterProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        MasterProcessor& proc;
        float shown = -30.0f, blink = 0.0f, glow = 0.0f;
        juce::Point<float> look;
        bool bypassed = false;
    };

    // Stereo vectorscope (vertical cloud = mono, wide = stereo, flat/red = phase problem)
    struct Vectorscope : public juce::Component
    {
        explicit Vectorscope (MasterProcessor& p) : proc (p) {}
        void pull();
        void paint (juce::Graphics&) override;
        MasterProcessor& proc;
        static constexpr int N = 600;
        std::array<float, N> l {}, r {};
        int pos = 0;
    };

    // SPICE MOI CA BABY: one slider that doses the whole master
    struct SpiceSlider : public juce::Component
    {
        explicit SpiceSlider (MasterProcessor& p);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void setFromX (float x);
        MasterProcessor& proc;
        juce::ParameterAttachment att;
        float value = 0.35f;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics& g);
    void paintDynamic (juce::Graphics& g);
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
    LoudnessGauge gauge;
    Vectorscope scope;
    SpiceSlider spice;
    tchk::Meter grLow, grMid, grHigh, clipMeter, limMeter;
    tchk::Choice analogMode;
    juce::TextButton resetLufsButton { "RESET" };

    // Areas (design coordinates)
    juce::Rectangle<int> spiceArea, heroArea, specArea, compArea, eqArea, stereoArea, analogArea, clipArea, limArea, meterArea;

    // Cached meter values
    float lufsS = -100.0f, lufsI = -100.0f, peak = -100.0f, plr = 0.0f, corr = 1.0f;
    float blink = 0.0f, glowPhase = 0.0f;
    int blinkCountdown = 90;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterEditor)
};
