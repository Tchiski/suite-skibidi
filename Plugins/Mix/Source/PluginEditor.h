#pragma once

#include "PluginProcessor.h"
#include "TchiskibidiWidgets.h"

class MixEditor : public tchk::ScaledEditor, private juce::Timer
{
public:
    explicit MixEditor (MixProcessor&);
    ~MixEditor() override { stopTimer(); }

private:
    // EQ you can grab: PARAM = 4 bands + low cut (nodes), MULTI = 10 fixed sliders
    struct EqView : public juce::Component
    {
        explicit EqView (MixProcessor& p);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        struct Node { const char* freqId; const char* gainId; juce::Colour colour; const char* label; };
        bool multi() const;
        float xForFreq (float f) const;
        float freqForX (float x) const;
        float yForDb (float db) const;
        float dbForY (float y) const;
        void setParam (const juce::String& id, float value, bool gestureStart = false, bool gestureEnd = false);

        MixProcessor& proc;
        std::array<Node, 5> nodes;
        int dragNode = -1, dragBand = -1;
    };

    // Compressor transfer curve with a live dot
    struct CompView : public juce::Component
    {
        explicit CompView (MixProcessor& p) : proc (p) {}
        void paint (juce::Graphics&) override;
        MixProcessor& proc;
        float shownIn = -60.0f;
    };

    // Transient shaper: grey = before, colour = after
    struct TransView : public juce::Component
    {
        explicit TransView (MixProcessor& p);
        void paint (juce::Graphics&) override;
        MixProcessor& proc;
        std::array<float, 200> noise {};
        double sinceHit = 9.0;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layout();
    void syncModeButtons();

    MixProcessor& audioProcessor;
    juce::ComboBox presetBox;
    juce::TextButton paramButton { "PARAM" }, multiButton { "MULTI" };
    tchk::Toggle eqOn, compOn, transOn, excOn, clipOn;
    tchk::Knob lowcut, lowGain, lowFreq, midGain, midFreq, midQ, hmidGain, hmidFreq, highGain, highFreq;
    tchk::Knob threshold, ratio, attack, release, compMix;
    tchk::Knob punch, sustain;
    tchk::Knob excDrive, excFreq, excAmount;
    tchk::Knob clip, width, input, output;
    EqView eqView;
    CompView compView;
    TransView transView;
    tchk::Meter grMeter, clipMeter, inMeter, outMeter;

    juce::Rectangle<int> eqArea, compArea, transArea, excArea, clipArea, widthArea, ioArea;
    bool lastMulti = false;
    float slowIn = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixEditor)
};
