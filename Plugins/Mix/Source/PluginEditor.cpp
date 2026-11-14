#include "PluginEditor.h"

using tchk::LNF;

namespace
{
    constexpr int kW = 780, kH = 600;
    const juce::Colour cEq { 0xff3df5ff }, cComp { 0xff8f7bff }, cTrans { 0xffff4fd8 }, cExc { 0xffffa23d },
                       cClip { 0xffffe14d }, cWidth { 0xffc6ff3d }, cIo { 0xfff4ecff };
}

MixEditor::MixEditor (MixProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.85f),
      audioProcessor (p),
      eqOn (p.apvts, "eq_on", "On", cEq), compOn (p.apvts, "comp_on", "On", cComp), transOn (p.apvts, "trans_on", "On", cTrans),
      excOn (p.apvts, "exc_on", "On", cExc), clipOn (p.apvts, "clip_on", "On", cClip),
      lowcut (p.apvts, "lowcut", "Low cut", cEq, true), lowGain (p.apvts, "low_gain", "Low", cEq, true), lowFreq (p.apvts, "low_freq", "Low Hz", cEq, true),
      midGain (p.apvts, "mid_gain", "Mid", cEq, true), midFreq (p.apvts, "mid_freq", "Mid Hz", cEq, true), midQ (p.apvts, "mid_q", "Mid Q", cEq, true),
      hmidGain (p.apvts, "hmid_gain", "Hi-mid", cEq, true), hmidFreq (p.apvts, "hmid_freq", "Hi-mid Hz", cEq, true),
      highGain (p.apvts, "high_gain", "High", cEq, true), highFreq (p.apvts, "high_freq", "High Hz", cEq, true),
      threshold (p.apvts, "threshold", "Thresh", cComp, true), ratio (p.apvts, "ratio", "Ratio", cComp, true),
      attack (p.apvts, "attack", "Attack", cComp, true), release (p.apvts, "release", "Release", cComp, true), compMix (p.apvts, "comp_mix", "Mix", cComp, true),
      punch (p.apvts, "punch", "Punch", cTrans), sustain (p.apvts, "sustain", "Sustain", cTrans),
      excDrive (p.apvts, "exc_drive", "Drive", cExc, true), excFreq (p.apvts, "exc_freq", "Tone", cExc, true), excAmount (p.apvts, "exc_amount", "Amount", cExc, true),
      clip (p.apvts, "clip", "Threshold", cClip), width (p.apvts, "width", "Width", cWidth),
      input (p.apvts, "input", "Input", cIo), output (p.apvts, "output", "Output", cIo),
      grMeter ([&p] { return p.compGrDb.load(); }, -18.0f, 0.0f, cComp, true),
      clipMeter ([&p] { return p.clipGrDb.load(); }, -12.0f, 0.0f, cClip, true),
      inMeter ([&p] { return p.inputPeakDb.load(); }, -48.0f, 0.0f, LNF::lime),
      outMeter ([&p] { return p.outputPeakDb.load(); }, -48.0f, 0.0f, LNF::lime)
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.layoutFn = [this] { layout(); };

    presetBox.setTextWhenNothingSelected ("PRESETS");
    presetBox.addItemList (MixProcessor::getPresetNames(), 1);
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) audioProcessor.applyPreset (presetBox.getSelectedId() - 1); };

    curve.colour = cEq;
    curve.getCoeffs = [this]
    {
        const auto c = MixProcessor::makeEqCoeffs (audioProcessor.apvts, 48000.0);
        std::vector<tchk::BiquadCoeffs> v;
        if (audioProcessor.apvts.getRawParameterValue ("lowcut")->load() > 21.0f) { v.push_back (c[0]); v.push_back (c[0]); }
        if (audioProcessor.apvts.getRawParameterValue ("eq_on")->load() > 0.5f)
            for (size_t k = 1; k < c.size(); ++k) v.push_back (c[k]);
        return v;
    };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &presetBox, &eqOn, &compOn, &transOn, &excOn, &clipOn,
             &lowcut, &lowGain, &lowFreq, &midGain, &midFreq, &midQ, &hmidGain, &hmidFreq, &highGain, &highFreq,
             &threshold, &ratio, &attack, &release, &compMix, &punch, &sustain, &excDrive, &excFreq, &excAmount,
             &clip, &width, &input, &output, &curve, &grMeter, &clipMeter, &inMeter, &outMeter })
        content.addAndMakeVisible (c);

    layout();
    startTimerHz (30);
}

void MixEditor::layout()
{
    presetBox.setBounds (kW - 200, 20, 182, 26);

    eqArea    = { 18, 64, 744, 240 };
    compArea  = { 18, 312, 404, 136 };
    transArea = { 430, 312, 160, 136 };
    excArea   = { 598, 312, 164, 136 };
    clipArea  = { 18, 456, 236, 128 };
    widthArea = { 262, 456, 160, 128 };
    ioArea    = { 430, 456, 332, 128 };

    eqOn.setBounds (eqArea.getRight() - 60, eqArea.getY() + 10, 48, 22);
    curve.setBounds (eqArea.getX() + 14, eqArea.getY() + 40, eqArea.getWidth() - 28, 92);
    tchk::layoutModule (eqArea, nullptr, { &lowcut, &lowGain, &lowFreq, &midGain, &midFreq, &midQ, &hmidGain, &hmidFreq, &highGain, &highFreq }, 138);

    tchk::layoutModule (compArea.withTrimmedRight (30), &compOn, { &threshold, &ratio, &attack, &release, &compMix });
    compOn.setBounds (compArea.getRight() - 60, compArea.getY() + 10, 48, 22);
    grMeter.setBounds (compArea.getRight() - 26, compArea.getY() + 44, 10, 80);

    tchk::layoutModule (transArea, &transOn, { &punch, &sustain });
    tchk::layoutModule (excArea, &excOn, { &excDrive, &excFreq, &excAmount });

    tchk::layoutModule (clipArea.withTrimmedRight (40), nullptr, { &clip });
    clipOn.setBounds (clipArea.getRight() - 60, clipArea.getY() + 10, 48, 22);
    clipMeter.setBounds (clipArea.getRight() - 30, clipArea.getY() + 44, 10, 72);

    tchk::layoutModule (widthArea, nullptr, { &width });

    tchk::layoutModule (ioArea.withTrimmedRight (60), nullptr, { &input, &output });
    inMeter.setBounds (ioArea.getRight() - 48, ioArea.getY() + 40, 10, 76);
    outMeter.setBounds (ioArea.getRight() - 30, ioArea.getY() + 40, 10, 76);
}

void MixEditor::paintContent (juce::Graphics& g)
{
    paintBackgroundAndHeader (g, "Mix", {});
    tchk::drawModule (g, eqArea, "EQ", cEq);
    tchk::drawModule (g, compArea, "Comp", cComp);
    tchk::drawModule (g, transArea, "Transient", cTrans);
    tchk::drawModule (g, excArea, "Exciter", cExc);
    tchk::drawModule (g, clipArea, "Clip", cClip);
    tchk::drawModule (g, widthArea, "Stereo", cWidth);
    tchk::drawModule (g, ioArea, "In / Out", cIo);

    g.setColour (LNF::textDim);
    g.setFont (LNF::font (9.0f));
    g.drawText ("GR", grMeter.getBounds().translated (-6, -14).withSize (22, 12), juce::Justification::centred);
    g.drawText ("GR", clipMeter.getBounds().translated (-6, -14).withSize (22, 12), juce::Justification::centred);
    g.drawText ("IN", inMeter.getBounds().translated (-6, 80).withSize (22, 12), juce::Justification::centred);
    g.drawText ("OUT", outMeter.getBounds().translated (-6, 80).withSize (22, 12), juce::Justification::centred);

    // mini mascot in the header
    tchk::drawMascot (g, { 112.0f, 6.0f, 60.0f, 52.0f }, 0.0f, { 0.3f, 0.2f }, 0.0f);
}

void MixEditor::timerCallback()
{
    for (auto* m : { &grMeter, &clipMeter, &inMeter, &outMeter })
        m->update();
    curve.repaint();
}
