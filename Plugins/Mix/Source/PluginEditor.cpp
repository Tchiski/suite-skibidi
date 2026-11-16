#include "PluginEditor.h"

using tchk::LNF;

namespace
{
    constexpr int kW = 860, kH = 676;
    const juce::Colour cEq { 0xff3df5ff }, cComp { 0xff8f7bff }, cTrans { 0xffff4fd8 }, cExc { 0xffffa23d },
                       cClip { 0xffffe14d }, cWidth { 0xffc6ff3d }, cIo { 0xfff4ecff };
    float raw (const juce::AudioProcessorValueTreeState& s, const juce::String& id) { return s.getRawParameterValue (id)->load(); }
}

MixEditor::MixEditor (MixProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.85f, "mix"),
      audioProcessor (p),
      eqOn (p.apvts, "eq_on", "On", cEq), compOn (p.apvts, "comp_on", "On", cComp), transOn (p.apvts, "trans_on", "On", cTrans),
      excOn (p.apvts, "exc_on", "On", cExc), clipOn (p.apvts, "clip_on", "On", cClip),
      lowcut (p.apvts, "lowcut", "Low cut", cEq, true), lowGain (p.apvts, "low_gain", "Low", cEq, true), lowFreq (p.apvts, "low_freq", "Low Hz", cEq, true),
      midGain (p.apvts, "mid_gain", "Mid", cEq, true), midFreq (p.apvts, "mid_freq", "Mid Hz", cEq, true), midQ (p.apvts, "mid_q", "Mid Q", cEq, true),
      hmidGain (p.apvts, "hmid_gain", "Hi-mid", cEq, true), hmidFreq (p.apvts, "hmid_freq", "Hi-mid Hz", cEq, true),
      highGain (p.apvts, "high_gain", "High", cEq, true), highFreq (p.apvts, "high_freq", "High Hz", cEq, true),
      threshold (p.apvts, "threshold", "Thresh", cComp, true), ratio (p.apvts, "ratio", "Ratio", cComp, true),
      attack (p.apvts, "attack", "Attack", cComp, true), release (p.apvts, "release", "Release", cComp, true), compMix (p.apvts, "comp_mix", "Mix", cComp, true),
      punch (p.apvts, "punch", "Punch", cTrans, true), sustain (p.apvts, "sustain", "Sustain", cTrans, true),
      excDrive (p.apvts, "exc_drive", "Drive", cExc, true), excFreq (p.apvts, "exc_freq", "Tone", cExc, true), excAmount (p.apvts, "exc_amount", "Amount", cExc, true),
      clip (p.apvts, "clip", "Threshold", cClip), width (p.apvts, "width", "Width", cWidth),
      input (p.apvts, "input", "Input", cIo), output (p.apvts, "output", "Output", cIo),
      eqView (p), compView (p), transView (p),
      grMeter ([&p] { return p.compGrDb.load(); }, -18.0f, 0.0f, cComp, true),
      clipMeter ([&p] { return p.clipGrDb.load(); }, -12.0f, 0.0f, cClip, true),
      inMeter ([&p] { return p.inputPeakDb.load(); }, -48.0f, 0.0f, LNF::lime),
      outMeter ([&p] { return p.outputPeakDb.load(); }, -48.0f, 0.0f, LNF::lime)
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.layoutFn = [this] { layout(); };
    levelDb = [&p] { return p.outputPeakDb.load(); };

    presets = std::make_unique<tchk::PresetSystem> (p, "Mix", presetBox, heartButton,
        [] (juce::ComboBox& b) { b.addItemList (MixProcessor::getPresetNames(), 1); },
        [this] (int i) { audioProcessor.applyPreset (i); });
    compMix.asOrbit(); input.asRocket(); output.asRocket();

    for (auto* b : { &paramButton, &multiButton })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, cEq);
    }
    auto setMode = [this] (float m)
    {
        if (auto* prm = audioProcessor.apvts.getParameter ("eq_mode"))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (m));
            prm->endChangeGesture();
        }
    };
    paramButton.onClick = [setMode] { setMode (0.0f); };
    multiButton.onClick = [setMode] { setMode (1.0f); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &presetBox, &heartButton, &paramButton, &multiButton, &eqOn, &compOn, &transOn, &excOn, &clipOn,
             &lowcut, &lowGain, &lowFreq, &midGain, &midFreq, &midQ, &hmidGain, &hmidFreq, &highGain, &highFreq,
             &threshold, &ratio, &attack, &release, &compMix, &punch, &sustain, &excDrive, &excFreq, &excAmount,
             &clip, &width, &input, &output, &eqView, &compView, &transView, &grMeter, &clipMeter, &inMeter, &outMeter })
        content.addAndMakeVisible (c);

    layout();
    addModuleInfo (eqArea, "EQ", TK_FR ("Attrape les points de la courbe pour sculpter le son.||Sculpte le son. Attrape les points sur la courbe : gauche/droite = fréquence, haut/bas = volume, molette sur le point 2 = largeur, double-clic = remise à zéro. PARAM = 4 bandes libres, MULTI = 10 curseurs fixes comme une table de mixage."));
    addModuleInfo (compArea, "Comp", TK_FR ("Rend le volume plus régulier.||Resserre le volume. Le point montre ton son en direct : quand il passe au-dessus du coude de la courbe, il est compressé. THRESH = seuil, RATIO = force, ATTACK/RELEASE = vitesse, MIX = compression parallèle."));
    addModuleInfo (transArea, "Transient", TK_FR ("Plus ou moins d'attaque et de queue.||PUNCH renforce ou adoucit l'attaque (le claquement), SUSTAIN allonge ou raccourcit la queue. Gris = avant, couleur = après."), true);
    addModuleInfo (excArea, "Exciter", TK_FR ("Ajoute de la brillance. Monte AMOUNT.||Ajoute de la brillance au-dessus de TONE. AMOUNT est à 0 par défaut : monte-le pour l'entendre."), true);
    addModuleInfo (clipArea, "Clip", TK_FR ("Arrondit les pics pour gagner du volume.||Arrondit les pics trop forts au-dessus de THRESHOLD, pour gagner du volume sans craquer."));
    addModuleInfo (widthArea, "Stereo", TK_FR ("Largeur stéréo.||0 % = mono, 100 % = normal, 200 % = très large."));
    addModuleInfo (ioArea, "In / Out", TK_FR ("Volumes d'entrée et de sortie.||Volume d'entrée et de sortie, avec les vu-mètres."));

    gearBounds = { kW - 42, 20, 26, 26 };
    finishSetup (22, { { 220, 14, 50 }, { 300, 34, 32 }, { 390, 12, 40 }, { 470, 32, 30 }, { 545, 10, 34 } });
    syncModeButtons();
    startTimerHz (30);
}

void MixEditor::layout()
{
    presetBox.setBounds (kW - 236, 20, 182, 26);
    heartButton.setBounds (kW - 268, 20, 26, 26);

    eqArea    = { 18, 64, 824, 280 };
    compArea  = { 18, 352, 452, 170 };
    transArea = { 478, 352, 180, 170 };
    excArea   = { 666, 352, 176, 170 };
    clipArea  = { 18, 530, 256, 128 };
    widthArea = { 282, 530, 180, 128 };
    ioArea    = { 470, 530, 372, 128 };

    paramButton.setBounds (eqArea.getX() + 136, eqArea.getY() + 10, 64, 22);
    multiButton.setBounds (eqArea.getX() + 202, eqArea.getY() + 10, 64, 22);
    eqOn.setBounds (onBounds (eqArea));
    const bool multi = raw (audioProcessor.apvts, "eq_mode") > 0.5f;
    eqView.setBounds (eqArea.getX() + 14, eqArea.getY() + 40, eqArea.getWidth() - 28, multi ? 226 : 130);
    tchk::layoutModule (eqArea, nullptr, { &lowcut, &lowGain, &lowFreq, &midGain, &midFreq, &midQ, &hmidGain, &hmidFreq, &highGain, &highFreq }, 176);
    for (auto* k : { &lowcut, &lowGain, &lowFreq, &midGain, &midFreq, &midQ, &hmidGain, &hmidFreq, &highGain, &highFreq })
        k->setVisible (! multi);

    compView.setBounds (compArea.getX() + 14, compArea.getY() + 40, 110, 116);
    tchk::layoutModule (compArea.withTrimmedLeft (128).withTrimmedRight (30), nullptr, { &threshold, &ratio, &attack, &release, &compMix }, 50);
    compOn.setBounds (onBounds (compArea));
    grMeter.setBounds (compArea.getRight() - 26, compArea.getY() + 50, 10, 104);

    transView.setBounds (transArea.getX() + 10, transArea.getY() + 40, transArea.getWidth() - 20, 46);
    tchk::layoutModule (transArea.withTrimmedTop (50), nullptr, { &punch, &sustain }, 38);
    transOn.setBounds (onBoundsCompact (transArea));

    tchk::layoutModule (excArea, nullptr, { &excDrive, &excFreq, &excAmount }, 50);
    excOn.setBounds (onBoundsCompact (excArea));

    tchk::layoutModule (clipArea.withTrimmedRight (40), nullptr, { &clip });
    clipOn.setBounds (onBounds (clipArea));
    clipMeter.setBounds (clipArea.getRight() - 30, clipArea.getY() + 44, 10, 72);

    tchk::layoutModule (widthArea, nullptr, { &width });

    tchk::layoutModule (ioArea.withTrimmedRight (60), nullptr, { &input, &output });
    inMeter.setBounds (ioArea.getRight() - 48, ioArea.getY() + 40, 10, 76);
    outMeter.setBounds (ioArea.getRight() - 30, ioArea.getY() + 40, 10, 76);
}

void MixEditor::syncModeButtons()
{
    const bool multi = raw (audioProcessor.apvts, "eq_mode") > 0.5f;
    paramButton.setToggleState (! multi, juce::dontSendNotification);
    multiButton.setToggleState (multi, juce::dontSendNotification);
    if (multi != lastMulti)
    {
        lastMulti = multi;
        layout();
    }
}

void MixEditor::paintContent (juce::Graphics& g)
{
    paintBackgroundAndHeader (g, "Mix", {});
    drawModuleT (g, eqArea, "EQ", cEq);
    drawModuleT (g, compArea, "Comp", cComp);
    drawModuleT (g, transArea, "Transient", cTrans, true);
    drawModuleT (g, excArea, "Exciter", cExc, true);
    drawModuleT (g, clipArea, "Clip", cClip);
    drawModuleT (g, widthArea, "Stereo", cWidth);
    drawModuleT (g, ioArea, "In / Out", cIo);
    // constellations: the knobs of each module are joined like stars
    tchk::drawConstellation (g, { &lowcut, &lowGain, &lowFreq, &midGain, &midFreq, &midQ, &hmidGain, &hmidFreq, &highGain, &highFreq }, cEq);
    tchk::drawConstellation (g, { &threshold, &ratio, &attack, &release, &compMix }, cComp);
    tchk::drawConstellation (g, { &punch, &sustain }, cTrans);
    tchk::drawConstellation (g, { &excDrive, &excFreq, &excAmount }, cExc);

    g.setColour (LNF::textDim);
    g.setFont (LNF::font (9.0f));
    g.drawText ("GR", grMeter.getBounds().translated (-6, -14).withSize (22, 12), juce::Justification::centred);
    g.drawText ("GR", clipMeter.getBounds().translated (-6, -14).withSize (22, 12), juce::Justification::centred);
    g.drawText ("IN", inMeter.getBounds().translated (-6, 80).withSize (22, 12), juce::Justification::centred);
    g.drawText ("OUT", outMeter.getBounds().translated (-6, 80).withSize (22, 12), juce::Justification::centred);

    tchk::drawMascot (g, { 112.0f, 6.0f, 60.0f, 52.0f }, 0.0f, { 0.3f, 0.2f }, 0.0f);
}

void MixEditor::timerCallback()
{
    for (auto* m : { &grMeter, &clipMeter, &inMeter, &outMeter })
        m->update();
    syncModeButtons();

    const float in = audioProcessor.inputPeakDb.load();
    transView.sinceHit += 1.0 / 30.0;
    if (in > -50.0f && in > slowIn + 6.0f) transView.sinceHit = 0.0;
    slowIn += (in - slowIn) * 0.08f;

    audioProcessor.spectrum.wanted = true;
    audioProcessor.spectrum.update (audioProcessor.getSampleRateForDisplay(), 180);
    eqView.repaint();
    compView.repaint();
    transView.repaint();
}

//==============================================================================
MixEditor::EqView::EqView (MixProcessor& p)
    : proc (p),
      nodes { { { "low_freq", "low_gain", juce::Colour (0xffc6ff3d), "1" }, { "mid_freq", "mid_gain", juce::Colour (0xffff4fd8), "2" },
                { "hmid_freq", "hmid_gain", juce::Colour (0xffffa23d), "3" }, { "high_freq", "high_gain", juce::Colour (0xff8f7bff), "4" },
                { "lowcut", nullptr, juce::Colour (0xfff4ecff), "LC" } } }
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

bool MixEditor::EqView::multi() const { return raw (proc.apvts, "eq_mode") > 0.5f; }
float MixEditor::EqView::xForFreq (float f) const { return std::log (f / 20.0f) / std::log (1000.0f) * (float) getWidth(); }
float MixEditor::EqView::freqForX (float x) const { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, x / (float) getWidth())); }
float MixEditor::EqView::yForDb (float db) const { return (float) getHeight() * 0.5f - db / 15.0f * (float) getHeight() * 0.42f; }
float MixEditor::EqView::dbForY (float y) const { return ((float) getHeight() * 0.5f - y) / ((float) getHeight() * 0.42f) * 15.0f; }

void MixEditor::EqView::setParam (const juce::String& id, float v, bool gestureStart, bool gestureEnd)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        if (gestureStart) prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (v)));
        if (gestureEnd) prm->endChangeGesture();
    }
}

void MixEditor::EqView::mouseDown (const juce::MouseEvent& e)
{
    dragNode = dragBand = -1;
    if (multi())
    {
        dragBand = juce::jlimit (0, MixProcessor::numGeqBands - 1, (int) (e.position.x / ((float) getWidth() / (float) MixProcessor::numGeqBands)));
        if (auto* prm = proc.apvts.getParameter (MixProcessor::geqId (dragBand))) prm->beginChangeGesture();
        mouseDrag (e);
        return;
    }
    float best = 22.0f * 22.0f;
    for (int i = 0; i < (int) nodes.size(); ++i)
    {
        const auto& n = nodes[(size_t) i];
        const float nx = juce::jmax (9.0f, xForFreq (raw (proc.apvts, n.freqId)));
        const float ny = n.gainId != nullptr ? yForDb (raw (proc.apvts, n.gainId)) : (float) getHeight() - 14.0f;
        const float d = e.position.getDistanceSquaredFrom ({ nx, ny });
        if (d < best) { best = d; dragNode = i; }
    }
    if (dragNode >= 0)
    {
        const auto& n = nodes[(size_t) dragNode];
        if (auto* prm = proc.apvts.getParameter (n.freqId)) prm->beginChangeGesture();
        if (n.gainId != nullptr) if (auto* prm = proc.apvts.getParameter (n.gainId)) prm->beginChangeGesture();
    }
}

void MixEditor::EqView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand >= 0)
    {
        setParam (MixProcessor::geqId (dragBand), juce::jlimit (-12.0f, 12.0f, dbForY (e.position.y)));
        return;
    }
    if (dragNode < 0) return;
    const auto& n = nodes[(size_t) dragNode];
    setParam (n.freqId, freqForX (e.position.x));
    if (n.gainId != nullptr) setParam (n.gainId, juce::jlimit (-12.0f, 12.0f, dbForY (e.position.y)));
}

void MixEditor::EqView::mouseUp (const juce::MouseEvent&)
{
    if (dragBand >= 0)
        if (auto* prm = proc.apvts.getParameter (MixProcessor::geqId (dragBand))) prm->endChangeGesture();
    if (dragNode >= 0)
    {
        const auto& n = nodes[(size_t) dragNode];
        if (auto* prm = proc.apvts.getParameter (n.freqId)) prm->endChangeGesture();
        if (n.gainId != nullptr) if (auto* prm = proc.apvts.getParameter (n.gainId)) prm->endChangeGesture();
    }
    dragNode = dragBand = -1;
}

void MixEditor::EqView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (multi())
    {
        const int b = juce::jlimit (0, MixProcessor::numGeqBands - 1, (int) (e.position.x / ((float) getWidth() / (float) MixProcessor::numGeqBands)));
        setParam (MixProcessor::geqId (b), 0.0f, true, true);
        return;
    }
    mouseDown (e);
    if (dragNode >= 0 && nodes[(size_t) dragNode].gainId != nullptr)
    {
        if (auto* prm = proc.apvts.getParameter (nodes[(size_t) dragNode].gainId))
            prm->setValueNotifyingHost (prm->getDefaultValue());
    }
    mouseUp (e);
}

void MixEditor::EqView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (multi()) return;
    if (auto* prm = proc.apvts.getParameter ("mid_q"))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->getValue() + w.deltaY * 0.15f));
        prm->endChangeGesture();
    }
}

void MixEditor::EqView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float w = r.getWidth(), h = r.getHeight();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (LNF::panelLight);
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        g.drawVerticalLine ((int) xForFreq (f), 3.0f, h - 3.0f);
    g.drawHorizontalLine ((int) (h * 0.5f), 3.0f, w - 3.0f);
    g.setColour (LNF::textDim);
    g.setFont (LNF::font (9.0f, false));
    g.drawText ("100", (int) xForFreq (100.0f) + 2, (int) h - 12, 30, 10, juce::Justification::left);
    g.drawText ("1k", (int) xForFreq (1000.0f) + 2, (int) h - 12, 30, 10, juce::Justification::left);
    g.drawText ("10k", (int) xForFreq (10000.0f) + 2, (int) h - 12, 30, 10, juce::Justification::left);

    // live spectrum of the track (after the EQ), behind the curve
    if (! proc.spectrum.levels.empty())
    {
        const auto& lv = proc.spectrum.levels;
        juce::Path sp; sp.startNewSubPath (0.0f, h);
        for (size_t i = 0; i < lv.size(); ++i)
        {
            const float x = (float) i / (float) (lv.size() - 1) * w;
            const float y = juce::jlimit (2.0f, h, h - (lv[i] + 90.0f) / 84.0f * (h - 6.0f));
            sp.lineTo (x, y);
        }
        sp.lineTo (w, h); sp.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3df5ff).withAlpha (0.32f), 0, 0, juce::Colour (0xff3df5ff).withAlpha (0.04f), 0, h, false));
        g.fillPath (sp);
        g.setColour (juce::Colour (0xff3df5ff).withAlpha (0.45f));
        g.strokePath (sp, juce::PathStrokeType (1.0f));
    }
    const bool isMulti = multi(), on = raw (proc.apvts, "eq_on") > 0.5f;
    std::vector<tchk::BiquadCoeffs> coeffs;
    const auto c = MixProcessor::makeEqCoeffs (proc.apvts, 48000.0);
    if (raw (proc.apvts, "lowcut") > 21.0f) { coeffs.push_back (c[0]); coeffs.push_back (c[0]); }
    if (on)
    {
        if (isMulti)
            for (int b = 0; b < MixProcessor::numGeqBands; ++b)
                coeffs.push_back (tchk::BiquadCoeffs::make (tchk::FilterType::peak, 48000.0, MixProcessor::geqFreqs[(size_t) b], MixProcessor::geqQ,
                                                            raw (proc.apvts, MixProcessor::geqId (b))));
        else
            for (size_t k = 1; k < c.size(); ++k) coeffs.push_back (c[k]);
    }
    juce::Path curve, fill;
    for (int i = 0; i <= 180; ++i)
    {
        const float x = (float) i / 180.0f * w, f = freqForX (x);
        float db = 0.0f;
        for (auto& cf : coeffs) db += cf.magnitudeDb (f, 48000.0);
        const float y = juce::jlimit (2.0f, h - 2.0f, yForDb (db));
        if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, h * 0.5f); }
        else curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (w, h * 0.5f);
    fill.closeSubPath();
    const auto col = on ? cEq : LNF::textDim;
    g.setColour (col.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (col.withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (isMulti)
    {
        const float bw = w / (float) MixProcessor::numGeqBands;
        for (int b = 0; b < MixProcessor::numGeqBands; ++b)
        {
            const float x = ((float) b + 0.5f) * bw, gdb = raw (proc.apvts, MixProcessor::geqId (b)), y = yForDb (gdb);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillRoundedRectangle (x - 3.0f, 16.0f, 6.0f, h - 40.0f, 3.0f);
            g.setColour (cEq);
            g.fillRect (x - 3.0f, juce::jmin (y, h * 0.5f), 6.0f, std::abs (y - h * 0.5f));
            g.setColour (LNF::text);
            g.fillRoundedRectangle (x - 13.0f, y - 6.0f, 26.0f, 12.0f, 4.0f);
            g.setColour (LNF::background);
            g.drawRoundedRectangle (x - 13.0f, y - 6.0f, 26.0f, 12.0f, 4.0f, 1.5f);
            g.setColour (LNF::textDim);
            g.setFont (LNF::font (9.0f));
            const float f = MixProcessor::geqFreqs[(size_t) b];
            g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f) + "k" : juce::String ((int) f), juce::Rectangle<float> (x - 20.0f, 2.0f, 40.0f, 12.0f), juce::Justification::centred);
            g.drawText ((gdb > 0.05f ? "+" : "") + juce::String (gdb, 1), juce::Rectangle<float> (x - 20.0f, h - 24.0f, 40.0f, 12.0f), juce::Justification::centred);
        }
        return;
    }
    for (auto& n : nodes)
    {
        const float x = juce::jmax (9.0f, xForFreq (raw (proc.apvts, n.freqId)));
        const float y = n.gainId != nullptr ? yForDb (raw (proc.apvts, n.gainId)) : h - 14.0f;
        g.setColour (n.colour);
        g.fillEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f);
        g.setColour (LNF::background);
        g.drawEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f, 2.0f);
        g.setFont (LNF::font (8.0f));
        g.drawText (n.label, juce::Rectangle<float> (x - 8.0f, y - 6.0f, 16.0f, 12.0f), juce::Justification::centred);
    }
}

//==============================================================================
void MixEditor::CompView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float w = r.getWidth(), h = r.getHeight();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 8.0f);
    const float th = raw (proc.apvts, "threshold"), ra = raw (proc.apvts, "ratio"), knee = 6.0f;
    auto out = [=] (float in)
    {
        const float o = in - th;
        if (2.0f * o < -knee) return in;
        if (2.0f * std::abs (o) <= knee) return in + (1.0f / ra - 1.0f) * (o + knee * 0.5f) * (o + knee * 0.5f) / (2.0f * knee);
        return th + o / ra;
    };
    auto X = [w] (float d) { return (d + 60.0f) / 60.0f * w; };
    auto Y = [h] (float d) { return h - (d + 60.0f) / 60.0f * h; };
    g.setColour (LNF::panelLight);
    for (float d = -48.0f; d < 0.0f; d += 12.0f) { g.drawVerticalLine ((int) X (d), 0.0f, h); g.drawHorizontalLine ((int) Y (d), 0.0f, w); }
    const float dash[] = { 3.0f, 3.0f };
    g.setColour (LNF::text.withAlpha (0.15f));
    g.drawDashedLine ({ 0.0f, h, w, 0.0f }, dash, 2);
    juce::Path p;
    for (float d = -60.0f; d <= 0.0f; d += 0.5f)
        if (d <= -60.0f) p.startNewSubPath (X (d), Y (out (d))); else p.lineTo (X (d), Y (out (d)));
    g.setColour (cComp.withAlpha (0.3f));
    g.strokePath (p, juce::PathStrokeType (6.0f));
    g.setColour (cComp);
    g.strokePath (p, juce::PathStrokeType (2.0f));
    g.setColour (cComp.withAlpha (0.5f));
    g.fillRect (X (th) - 0.5f, 0.0f, 1.0f, h);

    const float in = juce::jlimit (-60.0f, 0.0f, proc.compInDb.load());
    shownIn = in > shownIn ? in : shownIn + (in - shownIn) * 0.2f;
    const bool active = shownIn > th && raw (proc.apvts, "comp_on") > 0.5f;
    const juce::Point<float> dot (X (shownIn), Y (out (shownIn)));
    g.setColour ((active ? LNF::pink : LNF::lime).withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (dot));
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (dot));
    g.setColour (active ? LNF::pink : LNF::textDim);
    g.setFont (LNF::font (9.0f));
    g.drawText (active ? TK_FR ("COMPRESSÉ") : juce::String ("LIBRE"), 5, 3, 80, 12, juce::Justification::centredLeft);
}

//==============================================================================
MixEditor::TransView::TransView (MixProcessor& p) : proc (p)
{
    juce::Random rnd (7);
    for (size_t i = 0; i < noise.size(); ++i)
        noise[i] = std::sin ((float) i * 1.8f) * 0.6f + std::sin ((float) i * 4.6f) * 0.3f + (rnd.nextFloat() - 0.5f) * 0.5f;
}

void MixEditor::TransView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float w = r.getWidth(), h = r.getHeight();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 6.0f);
    const bool on = raw (proc.apvts, "trans_on") > 0.5f;
    const float pu = on ? raw (proc.apvts, "punch") : 0.0f, su = on ? raw (proc.apvts, "sustain") : 0.0f;
    auto env = [] (float t, float a, float s)
    {
        const float at = t < 0.06f ? t / 0.06f : 1.0f;
        const float dec = std::exp (-t * (6.0f - s * 3.5f));
        const float peak = t < 0.12f ? 1.0f + a * 0.9f * (1.0f - t / 0.12f) : 1.0f;
        return at * dec * peak;
    };
    auto wave = [&] (float a, float s)
    {
        juce::Path p;
        for (size_t i = 0; i < noise.size(); ++i)
        {
            const float t = (float) i / (float) noise.size();
            const float y = h * 0.5f - env (t, a, s) * noise[i] * h * 0.42f;
            if (i == 0) p.startNewSubPath (0.0f, y); else p.lineTo (t * w, y);
        }
        return p;
    };
    g.setColour (LNF::text.withAlpha (0.25f));
    g.strokePath (wave (0.0f, 0.0f), juce::PathStrokeType (1.0f));
    g.setColour (cTrans);
    g.strokePath (wave (pu, su), juce::PathStrokeType (1.2f));
    if (sinceHit < 0.5)
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillRect ((float) (sinceHit / 0.5) * w, 0.0f, 1.0f, h);
    }
    g.setFont (LNF::font (8.0f));
    g.setColour (LNF::text.withAlpha (0.45f));
    g.drawText ("AVANT", 4, 2, 40, 10, juce::Justification::centredLeft);
    g.setColour (cTrans);
    g.drawText (TK_FR ("APRÈS"), 38, 2, 40, 10, juce::Justification::centredLeft);
}
