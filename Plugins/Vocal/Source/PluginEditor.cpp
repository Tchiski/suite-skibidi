#include "PluginEditor.h"

using tchk::LNF;

namespace
{
    constexpr int kW = 780, kH = 780;
    const juce::Colour cLevel { 0xffc6ff3d }, cDeess { 0xff3df5ff }, cEq { 0xffff4fd8 }, cComp { 0xff8f7bff },
                       cWarm { 0xffffa23d }, cDbl { 0xffffe14d }, cDelay { 0xff3df5ff }, cVerb { 0xffd08bff }, cIo { 0xfff4ecff }, cTune { 0xff3df5c8 };
}

VocalEditor::VocalEditor (VocalProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.85f),
      audioProcessor (p),
      tuneOn (p.apvts, "tune_on", "On", cTune),
      levelOn (p.apvts, "level_on", "On", cLevel), deessOn (p.apvts, "deess_on", "On", cDeess), eqOn (p.apvts, "eq_on", "On", cEq),
      compOn (p.apvts, "comp_on", "On", cComp), warmOn (p.apvts, "warm_on", "On", cWarm), dblOn (p.apvts, "dbl_on", "On", cDbl),
      delayOn (p.apvts, "delay_on", "On", cDelay), verbOn (p.apvts, "verb_on", "On", cVerb),
      tuneSpeed (p.apvts, "tune_speed", "Speed", cTune), tuneAmount (p.apvts, "tune_amount", "Amount", cTune),
      levelAmt (p.apvts, "level_amt", "Amount", cLevel),
      deessFreq (p.apvts, "deess_freq", "Freq", cDeess), deessAmt (p.apvts, "deess_amt", "Amount", cDeess),
      lowcut (p.apvts, "lowcut", "Low cut", cEq, true), mud (p.apvts, "mud", "Mud", cEq, true),
      presence (p.apvts, "presence", "Presence", cEq, true), air (p.apvts, "air", "Air", cEq, true),
      compAmt (p.apvts, "comp_amt", "Amount", cComp), compSpeed (p.apvts, "comp_speed", "Speed", cComp),
      warmth (p.apvts, "warmth", "Warmth", cWarm),
      dblAmt (p.apvts, "dbl_amt", "Amount", cDbl), dblWidth (p.apvts, "dbl_width", "Width", cDbl),
      delayMix (p.apvts, "delay_mix", "Mix", cDelay), delayFb (p.apvts, "delay_fb", "Feedback", cDelay),
      verbMix (p.apvts, "verb_mix", "Mix", cVerb), verbSize (p.apvts, "verb_size", "Size", cVerb),
      input (p.apvts, "input", "Input", cIo), output (p.apvts, "output", "Output", cIo),
      delayDiv (p.apvts, "delay_div"), tuneKey (p.apvts, "tune_key"), tuneScale (p.apvts, "tune_scale"),
      noteDisplay (p.autotune),
      levelMeter ([&p] { return std::abs (p.levelerDb.load()) - 12.0f; }, -12.0f, 0.0f, cLevel),
      deessMeter ([&p] { return p.deesserGrDb.load(); }, -18.0f, 0.0f, cDeess, true),
      compMeter ([&p] { return p.compGrDb.load(); }, -18.0f, 0.0f, cComp, true),
      inMeter ([&p] { return p.inputPeakDb.load(); }, -48.0f, 0.0f, LNF::lime),
      outMeter ([&p] { return p.outputPeakDb.load(); }, -48.0f, 0.0f, LNF::lime)
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.layoutFn = [this] { layout(); };

    presetBox.setTextWhenNothingSelected ("PRESETS");
    {
        const auto names = VocalProcessor::getPresetNames();
        for (int i = 0; i < names.size(); ++i)
        {
            if (i == VocalProcessor::numGenericPresets)
                presetBox.addSectionHeading ("ARTISTES");
            presetBox.addItem (names[i], i + 1);
        }
    }
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) audioProcessor.applyPreset (presetBox.getSelectedId() - 1); };

    curve.colour = cEq;
    curve.rangeDb = 12.0f;
    curve.getCoeffs = [this]
    {
        if (audioProcessor.apvts.getRawParameterValue ("eq_on")->load() < 0.5f) return std::vector<tchk::BiquadCoeffs>();
        return VocalProcessor::makeEqCoeffs (audioProcessor.apvts, 48000.0);
    };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &presetBox, &tuneOn, &tuneSpeed, &tuneAmount, &tuneKey, &tuneScale, &noteDisplay, &levelOn, &deessOn, &eqOn, &compOn, &warmOn, &dblOn, &delayOn, &verbOn,
             &levelAmt, &deessFreq, &deessAmt, &lowcut, &mud, &presence, &air, &compAmt, &compSpeed, &warmth,
             &dblAmt, &dblWidth, &delayMix, &delayFb, &verbMix, &verbSize, &input, &output, &delayDiv, &curve,
             &levelMeter, &deessMeter, &compMeter, &inMeter, &outMeter })
        content.addAndMakeVisible (c);

    layout();
    startTimerHz (30);
}

void VocalEditor::layout()
{
    presetBox.setBounds (kW - 200, 20, 182, 26);

    levelArea = { 18, 64, 160, 166 };
    deessArea = { 186, 64, 210, 166 };
    eqArea    = { 404, 64, 358, 166 };
    compArea  = { 18, 238, 250, 166 };
    warmArea  = { 276, 238, 130, 166 };
    dblArea   = { 414, 238, 210, 166 };
    inArea    = { 632, 238, 130, 166 };
    tuneArea  = { 18, 412, 744, 170 };
    delayArea = { 18, 590, 334, 172 };
    verbArea  = { 360, 590, 264, 172 };
    outArea   = { 632, 590, 130, 172 };

    tchk::layoutModule (levelArea.withTrimmedRight (26), nullptr, { &levelAmt }, 44);
    levelOn.setBounds (levelArea.getRight() - 60, levelArea.getY() + 10, 48, 22);
    levelMeter.setBounds (levelArea.getRight() - 26, levelArea.getY() + 50, 10, 96);

    tchk::layoutModule (deessArea.withTrimmedRight (26), nullptr, { &deessFreq, &deessAmt }, 44);
    deessOn.setBounds (deessArea.getRight() - 60, deessArea.getY() + 10, 48, 22);
    deessMeter.setBounds (deessArea.getRight() - 26, deessArea.getY() + 50, 10, 96);

    eqOn.setBounds (eqArea.getRight() - 60, eqArea.getY() + 10, 48, 22);
    curve.setBounds (eqArea.getX() + 12, eqArea.getY() + 38, eqArea.getWidth() - 24, 44);
    tchk::layoutModule (eqArea, nullptr, { &lowcut, &mud, &presence, &air }, 86);

    tchk::layoutModule (compArea.withTrimmedRight (26), nullptr, { &compAmt, &compSpeed }, 44);
    compOn.setBounds (compArea.getRight() - 60, compArea.getY() + 10, 48, 22);
    compMeter.setBounds (compArea.getRight() - 26, compArea.getY() + 50, 10, 96);

    tchk::layoutModule (warmArea, &warmOn, { &warmth }, 44);
    tchk::layoutModule (dblArea, &dblOn, { &dblAmt, &dblWidth }, 44);
    tchk::layoutModule (inArea.withTrimmedRight (22), nullptr, { &input }, 44);
    inMeter.setBounds (inArea.getRight() - 24, inArea.getY() + 50, 10, 96);

    tuneOn.setBounds (tuneArea.getRight() - 60, tuneArea.getY() + 10, 48, 22);
    tuneKey.setBounds (tuneArea.getX() + 20, tuneArea.getY() + 72, 110, 28);
    tuneScale.setBounds (tuneArea.getX() + 146, tuneArea.getY() + 72, 140, 28);
    tuneSpeed.setBounds (tuneArea.getX() + 304, tuneArea.getY() + 44, 96, 116);
    tuneAmount.setBounds (tuneArea.getX() + 404, tuneArea.getY() + 44, 96, 116);
    noteDisplay.setBounds (tuneArea.getX() + 516, tuneArea.getY() + 44, 212, 112);

    delayOn.setBounds (delayArea.getRight() - 60, delayArea.getY() + 10, 48, 22);
    tchk::layoutModule (delayArea, nullptr, { &delayMix, nullptr, &delayFb }, 44);
    const int third = (delayArea.getWidth() - 16) / 3;
    delayDiv.setBounds (delayArea.getX() + 8 + third + 8, delayArea.getY() + 90, third - 16, 26);

    tchk::layoutModule (verbArea, &verbOn, { &verbMix, &verbSize }, 44);
    tchk::layoutModule (outArea.withTrimmedRight (22), nullptr, { &output }, 44);
    outMeter.setBounds (outArea.getRight() - 24, outArea.getY() + 50, 10, 96);
}

void VocalEditor::paintContent (juce::Graphics& g)
{
    paintBackgroundAndHeader (g, "Vocal", {});
    tchk::drawModule (g, levelArea, "Level", cLevel);
    tchk::drawModule (g, deessArea, "De-ess", cDeess);
    tchk::drawModule (g, eqArea, "EQ voix", cEq);
    tchk::drawModule (g, compArea, "Comp", cComp);
    tchk::drawModule (g, warmArea, "Warm", cWarm);
    tchk::drawModule (g, dblArea, "Double", cDbl);
    tchk::drawModule (g, inArea, "Input", cIo);
    tchk::drawModule (g, tuneArea, "Autotune", cTune);
    tchk::drawModule (g, delayArea, "Delay", cDelay);
    tchk::drawModule (g, verbArea, "Reverb", cVerb);
    tchk::drawModule (g, outArea, "Output", cIo);

    g.setColour (LNF::textDim);
    g.setFont (LNF::font (10.0f));
    g.drawText ("TONALITE", tuneKey.getBounds().translated (0, -16).withHeight (14), juce::Justification::centred);
    g.drawText ("GAMME", tuneScale.getBounds().translated (0, -16).withHeight (14), juce::Justification::centred);
    g.setFont (LNF::font (10.0f, false));
    g.drawText ("Speed 0 ms = effet robot  |  100 ms et + = naturel", tuneArea.getX() + 20, tuneArea.getBottom() - 46, 280, 14, juce::Justification::centredLeft);
    g.drawText ("Chromatique marche sur tous les morceaux.", tuneArea.getX() + 20, tuneArea.getBottom() - 30, 280, 14, juce::Justification::centredLeft);
    g.setFont (LNF::font (10.0f));
    g.drawText ("TEMPO", delayDiv.getBounds().translated (0, -16).withHeight (14), juce::Justification::centred);
    g.setFont (LNF::font (9.0f));
    for (auto* m : { &levelMeter, &deessMeter, &compMeter })
        g.drawText (m == &levelMeter ? "+/-" : "GR", m->getBounds().translated (-6, -14).withSize (22, 12), juce::Justification::centred);

    tchk::drawMascot (g, { 122.0f, 6.0f, 60.0f, 52.0f }, 0.0f, { -0.3f, 0.2f }, 0.0f);
}

void VocalEditor::timerCallback()
{
    for (auto* m : { &levelMeter, &deessMeter, &compMeter, &inMeter, &outMeter })
        m->update();
    curve.repaint();
    noteDisplay.repaint();
}

//==============================================================================
void VocalEditor::NoteDisplay::paint (juce::Graphics& g)
{
    static const char* names[] = { "DO", "DO#", "RE", "RE#", "MI", "FA", "FA#", "SOL", "SOL#", "LA", "LA#", "SI" };
    const juce::Colour accent (0xff3df5c8);
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (LNF::panelLight);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    const float sung = tune.detectedNote.load(), target = tune.targetNote.load();
    auto inner = r.reduced (10.0f, 6.0f);
    g.setColour (LNF::textDim);
    g.setFont (LNF::font (10.0f));
    g.drawText ("NOTE CHANTEE", inner.removeFromTop (12.0f), juce::Justification::centred);

    auto noteName = [] (float m) { const int k = (int) std::round (m); return juce::String (names[((k % 12) + 12) % 12]) + juce::String (k / 12 - 1); };
    g.setColour (sung >= 0.0f ? LNF::text : LNF::textDim);
    g.setFont (LNF::font (28.0f));
    g.drawText (sung >= 0.0f ? noteName (sung) : juce::String ("--"), inner.removeFromTop (34.0f), juce::Justification::centred);

    g.setFont (LNF::font (11.0f));
    g.setColour (accent);
    g.drawText (target >= 0.0f ? "> " + noteName (target) : juce::String (" "), inner.removeFromTop (14.0f), juce::Justification::centred);

    // correction bar: how far the voice was from the note (-50..+50 cents)
    const float cents = sung >= 0.0f && target >= 0.0f ? (sung - target) * 100.0f : 0.0f;
    shownCents += (cents - shownCents) * 0.4f;
    auto bar = inner.removeFromBottom (10.0f).reduced (6.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (LNF::panelLight);
    g.fillRect (bar.getCentreX() - 0.5f, bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);
    const float x = bar.getCentreX() + juce::jlimit (-1.0f, 1.0f, shownCents / 50.0f) * bar.getWidth() * 0.5f;
    g.setColour (std::abs (shownCents) < 10.0f ? LNF::lime : accent);
    g.fillEllipse (x - 4.0f, bar.getCentreY() - 4.0f, 8.0f, 8.0f);
}
