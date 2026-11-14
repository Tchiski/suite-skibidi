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

    tchk::Toggle levelOn, deessOn, eqOn, compOn, warmOn, dblOn, delayOn, verbOn;
    tchk::Knob levelAmt, deessFreq, deessAmt, lowcut, mud, presence, air, compAmt, compSpeed, warmth,
               dblAmt, dblWidth, delayMix, delayFb, verbMix, verbSize, input, output;
    tchk::Choice delayDiv;
    tchk::CurveDisplay curve;
    tchk::Meter levelMeter, deessMeter, compMeter, inMeter, outMeter;

    juce::Rectangle<int> levelArea, deessArea, eqArea, compArea, warmArea, dblArea, inArea, delayArea, verbArea, outArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEditor)
};
