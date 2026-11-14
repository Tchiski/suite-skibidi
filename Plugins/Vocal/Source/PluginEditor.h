#pragma once

#include "PluginProcessor.h"
#include "TchiskibidiWidgets.h"

class VocalEditor : public tchk::ScaledEditor, private juce::Timer
{
public:
    explicit VocalEditor (VocalProcessor&);
    ~VocalEditor() override { stopTimer(); audioProcessor.autotune.displayWanted = false; }

private:
    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layout();

    VocalProcessor& audioProcessor;
    juce::ComboBox presetBox;

    // Autotune piano roll: white line = the voice, coloured line = the corrected voice
    struct PitchRoll : public juce::Component
    {
        explicit PitchRoll (tchk::AutoTune& t) : tune (t) { sung.fill (-1.0f); fixed.fill (-1.0f); }
        void push();
        void paint (juce::Graphics&) override;
        tchk::AutoTune& tune;
        static constexpr int N = 210;
        std::array<float, N> sung {}, fixed {};
        int pos = 0;
        float centre = 60.0f;
    };

    // "S" that lights up when the de-esser works
    struct SFlash : public juce::Component
    {
        float amount = 0.0f;
        void paint (juce::Graphics&) override;
    };

    tchk::Toggle tuneOn, levelOn, deessOn, eqOn, compOn, warmOn, dblOn, delayOn, verbOn;
    tchk::Knob tuneSpeed, tuneAmount, levelAmt, deessFreq, deessAmt, lowcut, mud, presence, air, compAmt, compSpeed, warmth,
               dblAmt, dblWidth, delayMix, delayFb, verbMix, verbSize, input, output;
    tchk::Choice delayDiv, tuneKey, tuneScale;
    PitchRoll pitchRoll;
    SFlash sFlash;
    tchk::CurveDisplay curve;
    tchk::Meter levelMeter, deessMeter, compMeter, inMeter, outMeter;

    juce::Rectangle<int> tuneArea, levelArea, deessArea, eqArea, compArea, warmArea, dblArea, inArea, delayArea, verbArea, outArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEditor)
};
