#pragma once

#include "PluginProcessor.h"
#include "TchiskibidiWidgets.h"

class MixEditor : public tchk::ScaledEditor, private juce::Timer
{
public:
    explicit MixEditor (MixProcessor&);
    ~MixEditor() override { stopTimer(); }

private:
    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layout();

    MixProcessor& audioProcessor;
    juce::ComboBox presetBox;

    tchk::Toggle eqOn, compOn, transOn, excOn, clipOn;
    tchk::Knob lowcut, lowGain, lowFreq, midGain, midFreq, midQ, hmidGain, hmidFreq, highGain, highFreq;
    tchk::Knob threshold, ratio, attack, release, compMix;
    tchk::Knob punch, sustain;
    tchk::Knob excDrive, excFreq, excAmount;
    tchk::Knob clip, width, input, output;
    tchk::CurveDisplay curve;
    tchk::Meter grMeter, clipMeter, inMeter, outMeter;

    juce::Rectangle<int> eqArea, compArea, transArea, excArea, clipArea, widthArea, ioArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixEditor)
};
