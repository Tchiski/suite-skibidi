#pragma once

#include "PluginProcessor.h"
#include "TchiskibidiWidgets.h"

class CleanEditor : public tchk::ScaledEditor, private juce::Timer
{
public:
    explicit CleanEditor (CleanProcessor&);
    ~CleanEditor() override { stopTimer(); }

private:
    struct SpectrumView : public juce::Component
    {
        explicit SpectrumView (CleanProcessor& p) : proc (p) { inSm.fill (-120.0f); nzSm.fill (-120.0f); }
        void paint (juce::Graphics&) override;
        void update();
        CleanProcessor& proc;
        std::array<float, CleanProcessor::displayBins> inSm {}, nzSm {};
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layout();

    CleanProcessor& audioProcessor;
    SpectrumView spectrum;

    tchk::Toggle dnOn, humOn, clickOn, adaptive, listen;
    tchk::Knob dnAmount, dnSens, humAmt, clickSens, lowcut, output;
    tchk::Choice humFreq, humHarm;
    juce::TextButton learnButton { "LEARN" }, clearButton { "EFFACER" };
    std::unique_ptr<tchk::APVTS::ButtonAttachment> learnAttachment;

    juce::Rectangle<int> spectrumArea, dnArea, humArea, clickArea, lowArea, listenArea, outArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CleanEditor)
};
