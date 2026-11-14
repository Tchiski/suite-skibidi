#pragma once

#include "PluginProcessor.h"
#include "TchiskibidiWidgets.h"

class VocalEditor : public tchk::ScaledEditor, private juce::Timer
{
public:
    explicit VocalEditor (VocalProcessor&);
    ~VocalEditor() override { stopTimer(); }

private:
    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layout();

    VocalProcessor& audioProcessor;
    juce::ComboBox presetBox;

    // Shows the sung note, the note it is tuned to and the correction in cents
    struct NoteDisplay : public juce::Component
    {
        explicit NoteDisplay (tchk::AutoTune& t) : tune (t) {}
        void paint (juce::Graphics&) override;
        tchk::AutoTune& tune;
        float shownCents = 0.0f;
    };

    tchk::Toggle tuneOn, levelOn, deessOn, eqOn, compOn, warmOn, dblOn, delayOn, verbOn;
    tchk::Knob tuneSpeed, tuneAmount, levelAmt, deessFreq, deessAmt, lowcut, mud, presence, air, compAmt, compSpeed, warmth,
               dblAmt, dblWidth, delayMix, delayFb, verbMix, verbSize, input, output;
    tchk::Choice delayDiv, tuneKey, tuneScale;
    NoteDisplay noteDisplay;
    tchk::CurveDisplay curve;
    tchk::Meter levelMeter, deessMeter, compMeter, inMeter, outMeter;

    juce::Rectangle<int> tuneArea, levelArea, deessArea, eqArea, compArea, warmArea, dblArea, inArea, delayArea, verbArea, outArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEditor)
};
