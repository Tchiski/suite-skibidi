#pragma once

#include "PluginProcessor.h"
#include "TchiskibidiWidgets.h"

class TchiskibidiSpaceEditor : public tchk::ScaledEditor, private juce::Timer
{
public:
    explicit TchiskibidiSpaceEditor (TchiskibidiSpaceProcessor&);
    ~TchiskibidiSpaceEditor() override { stopTimer(); }

private:
    // Drag the dot: left/right = echo time, up/down = feedback
    struct XYPad : public juce::Component
    {
        XYPad (juce::RangedAudioParameter& time, juce::RangedAudioParameter& fb);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void setFrom (juce::Point<float>);
        juce::RangedAudioParameter &timeParam, &fbParam;
        juce::ParameterAttachment timeAtt, fbAtt;
        std::array<juce::Point<float>, 20> trail {};
        int trailPos = 0;
        float pulse = 0.0f;
    };

    // The echoes as bubbles (ping-pong: they bounce between left and right)
    struct EchoView : public juce::Component
    {
        explicit EchoView (TchiskibidiSpaceProcessor& p) : proc (p) {}
        void paint (juce::Graphics&) override;
        TchiskibidiSpaceProcessor& proc;
        double sinceHit = 9.0;
        float phase = 0.0f;
    };

    // The reverb as a nebula: grows with SIZE, darker with DAMPING
    struct NebulaView : public juce::Component
    {
        explicit NebulaView (TchiskibidiSpaceProcessor& p) : proc (p) {}
        void paint (juce::Graphics&) override;
        TchiskibidiSpaceProcessor& proc;
        float phase = 0.0f, pulse = 0.0f;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layout();

    TchiskibidiSpaceProcessor& audioProcessor;
    tchk::Knob time, feedback, wobble, echoLevel, lowCut, highCut;
    tchk::Knob size, damping, width, reverbLevel;
    tchk::Knob mix, output;
    tchk::Toggle syncButton, pingPongButton;
    tchk::Choice divisionBox;
    XYPad xyPad;
    EchoView echoView;
    NebulaView nebulaView;
    tchk::Meter inMeter, outMeter;

    juce::Rectangle<int> echoArea, reverbArea, masterArea;
    float slowIn = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TchiskibidiSpaceEditor)
};
