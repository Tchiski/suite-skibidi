#include "PluginEditor.h"

using LNF = TchiskibidiLookAndFeel;

TchiskibidiSpaceEditor::TchiskibidiSpaceEditor (TchiskibidiSpaceProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    // Echo
    setupKnob (time,      "time",      "TIME",     LNF::pink);
    setupKnob (feedback,  "feedback",  "FEEDBACK", LNF::pink);
    setupKnob (wobble,    "wobble",    "WOBBLE",   LNF::pink);
    setupKnob (echoLevel, "echolevel", "LEVEL",    LNF::pink);
    setupKnob (lowCut,    "lowcut",    "LOW CUT",  LNF::pink);
    setupKnob (highCut,   "highcut",   "HIGH CUT", LNF::pink);
    setupToggle (syncButton, syncAttachment, "sync", LNF::pink);
    setupToggle (pingPongButton, pingPongAttachment, "pingpong", LNF::pink);

    divisionBox.addItemList (TchiskibidiSpaceProcessor::getDivisionNames(), 1);
    addAndMakeVisible (divisionBox);
    divisionAttachment = std::make_unique<ComboBoxAttachment> (audioProcessor.apvts, "division", divisionBox);

    // Reverb
    setupKnob (size,        "size",        "SIZE",    LNF::cyan);
    setupKnob (damping,     "damping",     "DAMPING", LNF::cyan);
    setupKnob (width,       "width",       "WIDTH",   LNF::cyan);
    setupKnob (reverbLevel, "reverblevel", "LEVEL",   LNF::cyan);

    // Master
    setupKnob (mix,    "mix",    "MIX",    LNF::lime);
    setupKnob (output, "output", "OUTPUT", LNF::lime);

    setSize (860, 470);
    timerCallback();
    startTimerHz (15);
}

TchiskibidiSpaceEditor::~TchiskibidiSpaceEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TchiskibidiSpaceEditor::setupKnob (Knob& k, const juce::String& paramId,
                                        const juce::String& name, juce::Colour accent)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 18);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (LNF::font (12.0f));
    k.label.setColour (juce::Label::textColourId, LNF::textDim);
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, paramId, k.slider);

    // Double-click a knob to reset it
    if (auto* param = audioProcessor.apvts.getParameter (paramId))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void TchiskibidiSpaceEditor::setupToggle (juce::ToggleButton& button, std::unique_ptr<ButtonAttachment>& attachment,
                                          const juce::String& paramId, juce::Colour accent)
{
    button.setColour (juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible (button);
    attachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, paramId, button);
}

void TchiskibidiSpaceEditor::timerCallback()
{
    const bool sync = audioProcessor.apvts.getRawParameterValue ("sync")->load() > 0.5f;
    time.slider.setEnabled (! sync);
    time.label.setAlpha (sync ? 0.4f : 1.0f);
    divisionBox.setEnabled (sync);
}

//==============================================================================
void TchiskibidiSpaceEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (LNF::backgroundLight, 0.0f, 0.0f,
                                             LNF::background, 0.0f, bounds.getBottom(), false));
    g.fillAll();

    // Header
    auto header = headerArea;
    g.setColour (LNF::textDim);
    g.setFont (LNF::font (13.0f));
    g.drawText ("T C H I S K I B I D I", header.removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (LNF::text);
    g.setFont (LNF::font (34.0f));
    g.drawText ("SPACE", header, juce::Justification::centredLeft);

    g.setColour (LNF::textDim);
    g.setFont (LNF::font (12.0f, false));
    g.drawText ("echo + reverb  |  v0.1", headerArea, juce::Justification::bottomRight);

    const auto drawPanel = [&g] (juce::Rectangle<int> area, const juce::String& title, juce::Colour accent)
    {
        const auto r = area.toFloat();
        g.setColour (LNF::panel.withAlpha (0.85f));
        g.fillRoundedRectangle (r, 16.0f);
        g.setColour (accent.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), 16.0f, 1.2f);

        g.setColour (accent);
        g.setFont (LNF::font (15.0f));
        g.drawText (title, area.reduced (16, 12).removeFromTop (20), juce::Justification::centredLeft);
    };

    drawPanel (echoArea, "ECHO", LNF::pink);
    drawPanel (reverbArea, "REVERB", LNF::cyan);
    drawPanel (masterArea, "MASTER", LNF::lime);
}

void TchiskibidiSpaceEditor::placeKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (16));
    k.slider.setBounds (area);
}

void TchiskibidiSpaceEditor::resized()
{
    auto area = getLocalBounds().reduced (18);
    headerArea = area.removeFromTop (58);
    area.removeFromTop (12);

    constexpr int gap = 14;
    echoArea = area.removeFromLeft (404);
    area.removeFromLeft (gap);
    reverbArea = area.removeFromLeft (224);
    area.removeFromLeft (gap);
    masterArea = area;

    // --- Echo panel: 4 knobs on top, 2 knobs + controls below ---
    {
        auto inner = echoArea.reduced (12);
        inner.removeFromTop (28);
        auto top = inner.removeFromTop (inner.getHeight() / 2);
        auto bottom = inner;
        const int w = top.getWidth() / 4;

        placeKnob (time,      top.removeFromLeft (w).reduced (2));
        placeKnob (feedback,  top.removeFromLeft (w).reduced (2));
        placeKnob (wobble,    top.removeFromLeft (w).reduced (2));
        placeKnob (echoLevel, top.reduced (2));

        placeKnob (lowCut,  bottom.removeFromLeft (w).reduced (2));
        placeKnob (highCut, bottom.removeFromLeft (w).reduced (2));

        auto controls = bottom.reduced (10, 0);
        const int rowH = 30;
        controls.removeFromTop ((controls.getHeight() - rowH * 3 - 16) / 2);
        syncButton.setBounds (controls.removeFromTop (rowH));
        controls.removeFromTop (8);
        divisionBox.setBounds (controls.removeFromTop (rowH));
        controls.removeFromTop (8);
        pingPongButton.setBounds (controls.removeFromTop (rowH));
    }

    // --- Reverb panel: 2 x 2 knobs ---
    {
        auto inner = reverbArea.reduced (12);
        inner.removeFromTop (28);
        auto top = inner.removeFromTop (inner.getHeight() / 2);
        auto bottom = inner;
        const int w = top.getWidth() / 2;

        placeKnob (size,        top.removeFromLeft (w).reduced (2));
        placeKnob (damping,     top.reduced (2));
        placeKnob (width,       bottom.removeFromLeft (w).reduced (2));
        placeKnob (reverbLevel, bottom.reduced (2));
    }

    // --- Master panel: Mix and Output stacked ---
    {
        auto inner = masterArea.reduced (12);
        inner.removeFromTop (28);
        placeKnob (mix, inner.removeFromTop (inner.getHeight() / 2).reduced (2));
        placeKnob (output, inner.reduced (2));
    }
}
