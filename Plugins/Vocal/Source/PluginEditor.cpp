#include "PluginEditor.h"

using tchk::LNF;

namespace
{
    constexpr int kW = 780, kH = 600;
    const juce::Colour cLevel { 0xffc6ff3d }, cDeess { 0xff3df5ff }, cEq { 0xffff4fd8 }, cComp { 0xff8f7bff },
                       cWarm { 0xffffa23d }, cDbl { 0xffffe14d }, cDelay { 0xff3df5ff }, cVerb { 0xffd08bff }, cIo { 0xfff4ecff };
}

VocalEditor::VocalEditor (VocalProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.85f),
      audioProcessor (p),
      levelOn (p.apvts, "level_on", "On", cLevel), deessOn (p.apvts, "deess_on", "On", cDeess), eqOn (p.apvts, "eq_on", "On", cEq),
      compOn (p.apvts, "comp_on", "On", cComp), warmOn (p.apvts, "warm_on", "On", cWarm), dblOn (p.apvts, "dbl_on", "On", cDbl),
      delayOn (p.apvts, "delay_on", "On", cDelay), verbOn (p.apvts, "verb_on", "On", cVerb),
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
      delayDiv (p.apvts, "delay_div"),
      levelMeter ([&p] { return std::abs (p.levelerDb.load()) - 12.0f; }, -12.0f, 0.0f, cLevel),
      deessMeter ([&p] { return p.deesserGrDb.load(); }, -18.0f, 0.0f, cDeess, true),
      compMeter ([&p] { return p.compGrDb.load(); }, -18.0f, 0.0f, cComp, true),
      inMeter ([&p] { return p.inputPeakDb.load(); }, -48.0f, 0.0f, LNF::lime),
      outMeter ([&p] { return p.outputPeakDb.load(); }, -48.0f, 0.0f, LNF::lime)
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.layoutFn = [this] { layout(); };

    presetBox.setTextWhenNothingSelected ("PRESETS");
    presetBox.addItemList (VocalProcessor::getPresetNames(), 1);
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) audioProcessor.applyPreset (presetBox.getSelectedId() - 1); };

    curve.colour = cEq;
    curve.rangeDb = 12.0f;
    curve.getCoeffs = [this]
    {
        if (audioProcessor.apvts.getRawParameterValue ("eq_on")->load() < 0.5f) return std::vector<tchk::BiquadCoeffs>();
        return VocalProcessor::makeEqCoeffs (audioProcessor.apvts, 48000.0);
    };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &presetBox, &levelOn, &deessOn, &eqOn, &compOn, &warmOn, &dblOn, &delayOn, &verbOn,
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
    delayArea = { 18, 412, 334, 170 };
    verbArea  = { 360, 412, 264, 170 };
    outArea   = { 632, 412, 130, 170 };

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
    tchk::drawModule (g, delayArea, "Delay", cDelay);
    tchk::drawModule (g, verbArea, "Reverb", cVerb);
    tchk::drawModule (g, outArea, "Output", cIo);

    g.setColour (LNF::textDim);
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
}
