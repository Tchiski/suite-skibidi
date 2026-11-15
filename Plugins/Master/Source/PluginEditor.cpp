#include "PluginEditor.h"

using tchk::LNF;

namespace
{
    constexpr int kW = 640, kH = 1034;
    const juce::Colour cSpectral { 0xffff4fd8 }, cComp { 0xff8f7bff }, cEq { 0xffc6ff3d }, cStereo { 0xff3df5ff },
                       cAnalog { 0xffffa23d }, cClip { 0xffffe14d }, cLimiter { 0xffff4f6d };
}

MasterEditor::MasterEditor (MasterProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.75f, "master"),
      audioProcessor (p),
      bypassButton (p.apvts, "bypass", "Bypass", LNF::pink),
      matchButton (p.apvts, "match", "Match", LNF::cyan),
      inputKnob (p.apvts, "input", "Input", LNF::text),
      inputMeter ([&p] { return p.inputPeakDb.load(); }, -48.0f, 0.0f, LNF::lime),
      targetBox (p.apvts, "target"),
      specOn (p.apvts, "spec_on", "On", cSpectral), compOn (p.apvts, "comp_on", "On", cComp),
      eqOn (p.apvts, "eq_on", "On", cEq), stereoOn (p.apvts, "stereo_on", "On", cStereo),
      analogOn (p.apvts, "analog_on", "On", cAnalog), clipOn (p.apvts, "clip_on", "On", cClip),
      limOn (p.apvts, "lim_on", "On", cLimiter), bassMono (p.apvts, "bass_mono", "Bass mono", cStereo),
      specAmt (p.apvts, "spec_amt", "Amount", cSpectral),
      glue (p.apvts, "glue", "Glue", cComp), detail (p.apvts, "detail", "Detail", cComp),
      bass (p.apvts, "bass", "Bass", cEq), air (p.apvts, "air", "Air", cEq),
      width (p.apvts, "width", "Width", cStereo),
      color (p.apvts, "color", "Color", cAnalog),
      clipGain (p.apvts, "clip_gain", "Gain", cClip),
      limGain (p.apvts, "lim_gain", "Gain", cLimiter), ceiling (p.apvts, "ceiling", "Ceiling", cLimiter),
      release (p.apvts, "release", "Release", cLimiter), output (p.apvts, "output", "Output", LNF::text),
      spectralDisplay (p), gauge (p), scope (p), spice (p),
      grLow ([&p] { return p.bandGrDb[0].load(); }, -12.0f, 0.0f, cComp, true),
      grMid ([&p] { return p.bandGrDb[1].load(); }, -12.0f, 0.0f, cComp, true),
      grHigh ([&p] { return p.bandGrDb[2].load(); }, -12.0f, 0.0f, cComp, true),
      clipMeter ([&p] { return p.clipGrDb.load(); }, -12.0f, 0.0f, cClip, true),
      limMeter ([&p] { return p.limiterGrDb.load(); }, -12.0f, 0.0f, cLimiter, true),
      analogMode (p.apvts, "analog_mode")
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.layoutFn = [this] { layout(); };
    content.overFn = [this] (juce::Graphics& g) { paintDynamic (g); };
    levelDb = [&p] { return p.truePeakDb.load(); };
    p.scopeWanted = true;

    // Top bar
    presets = std::make_unique<tchk::PresetSystem> (p, "Master", presetBox, heartButton,
        [] (juce::ComboBox& b) { b.addItemList (MasterProcessor::getPresetNames(), 1); },
        [this] (int i) { audioProcessor.applyPreset (i); });
    inputKnob.asRocket(); output.asRocket();
    undoButton.onClick = [this] { audioProcessor.undoManager.undo(); };
    redoButton.onClick = [this] { audioProcessor.undoManager.redo(); };
    aButton.onClick = [this] { if (audioProcessor.getCurrentSlot() != 0) audioProcessor.switchAB(); };
    bButton.onClick = [this] { if (audioProcessor.getCurrentSlot() != 1) audioProcessor.switchAB(); };
    copyButton.onClick = [this] { audioProcessor.copyToOtherSlot(); };
    resetLufsButton.onClick = [this]
    {
        audioProcessor.outputLoudness.requestIntegratedReset();
        audioProcessor.inputLoudness.requestIntegratedReset();
    };

    autoButton.setButtonText ("AUTO");
    autoAttachment = std::make_unique<tchk::APVTS::ButtonAttachment> (p.apvts, "auto", autoButton);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &presetBox, &heartButton, &undoButton, &redoButton, &aButton, &bButton, &copyButton, &bypassButton, &matchButton,
             &inputKnob, &inputMeter, &autoButton, &targetBox,
             &specOn, &compOn, &eqOn, &stereoOn, &analogOn, &clipOn, &limOn, &bassMono,
             &specAmt, &glue, &detail, &bass, &air, &width, &color, &clipGain, &limGain, &ceiling, &release, &output,
             &spectralDisplay, &grLow, &grMid, &grHigh, &clipMeter, &limMeter, &analogMode, &resetLufsButton, &gauge, &scope, &spice })
        content.addAndMakeVisible (c);
    autoButton.toFront (false);

    layout();
    const char* names[] = { "Spectral", "Comp", "EQ", "Stereo", "Analog", "Clip", "Limiter" };
    const juce::String tips[] = {
        TK_FR ("Équilibre graves, médiums, aigus.||Équilibre automatiquement les graves, les médiums et les aigus, à petites doses. La courbe montre ce qu'il corrige."),
        TK_FR ("Soude le mix (GLUE), fait ressortir les détails.||GLUE soude le mix, DETAIL fait remonter les petits détails. Les barres L M H = graves, médiums, aigus."),
        TK_FR ("Plus ou moins de basses et d'air.||BASS agit vers 80 Hz, AIR vers 12 kHz."),
        TK_FR ("Largeur stéréo et vectorscope.||WIDTH élargit l'image. Le vectorscope montre ta stéréo : nuage vertical = mono, large = stéréo, à plat ou rouge = problème de phase. BASS MONO garde les basses au centre."),
        TK_FR ("Chaleur analogique : Tape, Tube ou Crunch.||Saturation chaude : Tape, Tube ou Crunch. COLOR = quantité."),
        TK_FR ("Arrondit les pics avant le limiteur.||Arrondit les pics juste avant le limiteur."),
        TK_FR ("Monte le volume sans dépasser le plafond.||GAIN monte le volume, CEILING = plafond à ne jamais dépasser (-1 dB conseillé), RELEASE = vitesse de relâchement, OUTPUT = volume final. La qualité (2x ou 4x) se règle dans la roue de réglages.") };
    const juce::Rectangle<int> areas[] = { specArea, compArea, eqArea, stereoArea, analogArea, clipArea, limArea };
    for (int i = 0; i < 7; ++i)
        addInfo (areas[i].getX() + 120, areas[i].getY() + 15, tips[i]);
    juce::ignoreUnused (names);
    addInfo (440, 100, TK_FR ("Volume perçu. Vise la zone verte.||La jauge montre le volume perçu (LUFS) en direct. La zone verte est ta CIBLE. AUTO règle le volume pour l'atteindre."));
    addInfo (spiceArea.getX() + 238, spiceArea.getY() + 12, TK_FR ("Un curseur qui dose tout le mastering.||Un seul curseur qui dose tout le mastering d'un coup : à gauche léger et naturel, à droite à fond. Les boutons des modules bougent tout seuls avec lui."));
    const float boxW = (604.0f - 4 * 8.0f) / 5.0f;
    addInfo (meterArea.getX() + (int) boxW - 22, meterArea.getY() + 4, TK_FR ("Les mesures de ton master.||LUFS = volume perçu (S = 3 dernières secondes, I = moyenne du morceau). TRUE PEAK = pic le plus haut. DYNAMICS = écart entre pics et moyenne (OUVERT, PUNCHY, DENSE, ÉCRASÉ). STEREO = compatibilité mono (négatif = problème de phase)."));

    settingsExtras.push_back ({ TK_FR ("Qualité du limiteur"), juce::StringArray { TK_FR ("Éco 2x (moins de CPU)"), TK_FR ("Haute 4x") },
                                [this] { return audioProcessor.isOversamplingHQ() ? 1 : 0; },
                                [this] (int v) { audioProcessor.setOversamplingHQ (v == 1); } });
    extraReset = [this] { audioProcessor.setOversamplingHQ (true); };

    finishSetup (55, { { 200, 14, 46 }, { 290, 36, 30 }, { 360, 12, 40 }, { 60, 240, 34 }, { 140, 196, 36 } });
    startTimerHz (30);
}

MasterEditor::~MasterEditor()
{
    stopTimer();
    audioProcessor.scopeWanted = false;
}

//==============================================================================
void MasterEditor::layout()
{
    presetBox.setBounds (404, 20, 182, 26);
    heartButton.setBounds (372, 20, 26, 26);

    int x = 18;
    for (auto* b : { &undoButton, &redoButton })
    {
        b->setBounds (x, 64, 58, 26);
        x += 64;
    }
    x += 16;
    aButton.setBounds (x, 64, 34, 26);  x += 38;
    bButton.setBounds (x, 64, 34, 26);  x += 38;
    copyButton.setBounds (x, 64, 48, 26);
    bypassButton.setBounds (440, 64, 90, 26);
    matchButton.setBounds (536, 64, 86, 26);

    heroArea = { 18, 100, 604, 160 };
    spiceArea = { 18, 272, 604, 56 };
    gauge.setBounds (176, 96, 288, 168);
    spice.setBounds (spiceArea.getX() + 270, spiceArea.getY() + 14, 316, 28);
    inputKnob.setBounds (22, 112, 90, 120);
    inputMeter.setBounds (116, 120, 10, 108);
    autoButton.setBounds (478, 104, 110, 110);
    targetBox.setBounds (463, 224, 140, 26);

    constexpr int rowH = 112, gap = 8, top = 334;
    specArea   = { 18, top, 604, rowH };
    compArea   = { 18, top + (rowH + gap), 604, rowH };
    eqArea     = { 18, top + 2 * (rowH + gap), 298, rowH };
    stereoArea = { 324, top + 2 * (rowH + gap), 298, rowH };
    analogArea = { 18, top + 3 * (rowH + gap), 298, rowH };
    clipArea   = { 324, top + 3 * (rowH + gap), 298, rowH };
    limArea    = { 18, top + 4 * (rowH + gap), 604, rowH };
    meterArea  = { 18, top + 5 * (rowH + gap), 604, 80 };

    auto onBounds = [] (juce::Rectangle<int> a) { return juce::Rectangle<int> (a.getX() + 14, a.getBottom() - 34, 50, 22); };
    auto knobAt = [] (juce::Rectangle<int> a, int dx) { return juce::Rectangle<int> (a.getX() + dx, a.getY() + 8, 84, 98); };

    specOn.setBounds (onBounds (specArea));
    specAmt.setBounds (knobAt (specArea, 140));
    spectralDisplay.setBounds (specArea.getX() + 236, specArea.getY() + 12, specArea.getWidth() - 250, specArea.getHeight() - 24);

    compOn.setBounds (onBounds (compArea));
    glue.setBounds (knobAt (compArea, 160));
    grLow.setBounds (compArea.getX() + 284, compArea.getY() + 18, 12, 70);
    grMid.setBounds (compArea.getX() + 304, compArea.getY() + 18, 12, 70);
    grHigh.setBounds (compArea.getX() + 324, compArea.getY() + 18, 12, 70);
    detail.setBounds (knobAt (compArea, 380));

    eqOn.setBounds (onBounds (eqArea));
    bass.setBounds (knobAt (eqArea, 118));
    air.setBounds (knobAt (eqArea, 204));

    stereoOn.setBounds (onBounds (stereoArea));
    width.setBounds (knobAt (stereoArea, 110));
    bassMono.setBounds (stereoArea.getX() + 12, stereoArea.getY() + 42, 92, 22);
    scope.setBounds (stereoArea.getX() + 202, stereoArea.getY() + 8, 88, 96);

    analogOn.setBounds (onBounds (analogArea));
    color.setBounds (knobAt (analogArea, 118));
    analogMode.setBounds (analogArea.getX() + 208, analogArea.getY() + 44, 80, 24);

    clipOn.setBounds (onBounds (clipArea));
    clipGain.setBounds (knobAt (clipArea, 118));
    clipMeter.setBounds (clipArea.getX() + 230, clipArea.getY() + 18, 12, 76);

    limOn.setBounds (onBounds (limArea));
    limGain.setBounds (knobAt (limArea, 130));
    ceiling.setBounds (knobAt (limArea, 220));
    release.setBounds (knobAt (limArea, 310));
    output.setBounds (knobAt (limArea, 400));
    limMeter.setBounds (limArea.getX() + 500, limArea.getY() + 18, 12, 76);

    const float boxW = (604.0f - 4 * 8.0f) / 5.0f;
    resetLufsButton.setBounds ((int) (meterArea.getX() + (boxW + 8.0f) + boxW - 44.0f), meterArea.getBottom() - 18, 40, 15);
}

//==============================================================================
void MasterEditor::paintContent (juce::Graphics& g)
{
    paintBackgroundAndHeader (g, "Master", {});

    g.setColour (LNF::textDim);
    g.setFont (LNF::font (10.0f));
    g.drawText ("CIBLE", 463, 252, 140, 12, juce::Justification::centred);

    // SPICE MOI CA BABY panel
    {
        const auto r = spiceArea.toFloat();
        g.setColour (juce::Colour (30, 8, 6).withAlpha (0.8f));
        g.fillRoundedRectangle (r, 14.0f);
        juce::ColourGradient cg (juce::Colour (255, 90, 40).withAlpha (0.28f), r.getX(), 0.0f, juce::Colour (255, 60, 100).withAlpha (0.28f), r.getRight(), 0.0f, false);
        cg.addColour (0.6, juce::Colour (255, 200, 60).withAlpha (0.18f));
        g.setGradientFill (cg);
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (juce::Colour (255, 140, 60).withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.75f), 14.0f, 1.5f);
        // chili
        const float cx = r.getX() + 14.0f, cy = r.getY() + 11.0f;
        juce::Path stem, chili, shine;
        stem.startNewSubPath (cx + 15.0f, cy + 7.0f); stem.cubicTo (cx + 15.0f, cy + 3.0f, cx + 19.0f, cy + 1.0f, cx + 21.0f, cy + 2.0f);
        chili.startNewSubPath (cx + 13.0f, cy + 8.0f);
        chili.cubicTo (cx + 22.0f, cy + 8.0f, cx + 24.0f, cy + 16.0f, cx + 20.0f, cy + 24.0f);
        chili.cubicTo (cx + 17.0f, cy + 30.0f, cx + 9.0f, cy + 33.0f, cx + 4.0f, cy + 32.0f);
        chili.cubicTo (cx + 9.0f, cy + 28.0f, cx + 6.0f, cy + 20.0f, cx + 8.0f, cy + 14.0f);
        chili.cubicTo (cx + 9.0f, cy + 10.0f, cx + 10.0f, cy + 8.0f, cx + 13.0f, cy + 8.0f);
        chili.closeSubPath();
        shine.startNewSubPath (cx + 12.0f, cy + 12.0f); shine.cubicTo (cx + 10.0f, cy + 16.0f, cx + 10.0f, cy + 20.0f, cx + 11.0f, cy + 23.0f);
        g.setColour (juce::Colour (0xff6bd65a)); g.strokePath (stem, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffff3d3d)); g.fillPath (chili);
        g.setColour (juce::Colour (0xff7a0a0a)); g.strokePath (chili, juce::PathStrokeType (1.5f));
        g.setColour (juce::Colour (0xffff9a9a)); g.strokePath (shine, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (0xffffd27a));
        g.setFont (LNF::font (17.0f));
        g.drawText (TK_FR ("SPICE MOI ÇA BABY"), (int) r.getX() + 46, (int) r.getY() + 10, 200, 22, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffffb38a));
        g.setFont (LNF::font (10.0f, false));
        g.drawText ("un seul curseur pour tout le master", (int) r.getX() + 46, (int) r.getY() + 32, 200, 14, juce::Justification::centredLeft);
    }

    // Modules
    struct M { juce::Rectangle<int> r; const char* name; juce::Colour c; };
    for (const auto& m : { M { specArea, "Spectral", cSpectral }, M { compArea, "Comp", cComp }, M { eqArea, "EQ", cEq },
                           M { stereoArea, "Stereo", cStereo }, M { analogArea, "Analog", cAnalog }, M { clipArea, "Clip", cClip },
                           M { limArea, "Limiter", cLimiter } })
    {
        drawPanelT (g, m.r, m.c);
        tchk::drawTag (g, { (float) m.r.getX() + 12.0f, (float) m.r.getY() + 12.0f, 100.0f, 24.0f }, m.name, m.c);
    }

    g.setColour (LNF::textDim);
    g.setFont (LNF::font (10.0f));
    const char* bandNames[] = { "L", "M", "H" };
    for (int b = 0; b < 3; ++b)
        g.drawText (bandNames[b], compArea.getX() + 280 + b * 20, compArea.getY() + 90, 20, 12, juce::Justification::centred);
    g.drawText ("GR", compArea.getX() + 280, compArea.getY() + 4, 60, 12, juce::Justification::centred);
    g.drawText ("GR", limArea.getX() + 494, limArea.getY() + 4, 24, 12, juce::Justification::centred);
    g.drawText ("TRUE PEAK", limArea.getX() + 522, limArea.getY() + 54, 70, 12, juce::Justification::centredLeft);
    g.drawText ("4x OVERSAMPLE", limArea.getX() + 522, limArea.getY() + 68, 80, 12, juce::Justification::centredLeft);
    g.drawText ("GR", clipArea.getX() + 224, clipArea.getY() + 4, 24, 12, juce::Justification::centred);
}

void MasterEditor::paintDynamic (juce::Graphics& g)
{
    g.setColour (cLimiter);
    g.setFont (LNF::font (13.0f));
    g.drawText (juce::String (audioProcessor.limiterGrDb.load(), 1) + " dB", limArea.getX() + 522, limArea.getY() + 30, 70, 18, juce::Justification::centredLeft);
    if (! audioProcessor.isOversamplingHQ())
    {
        // cover "4x" with the real factor
        g.setColour (LNF::textDim);
        g.setFont (LNF::font (10.0f));
        g.drawText ("(2x en Eco)", limArea.getX() + 522, limArea.getY() + 82, 80, 12, juce::Justification::centredLeft);
    }
    // Bottom meter strip
    const float target = MasterProcessor::getTargetLufs ((int) audioProcessor.apvts.getRawParameterValue ("target")->load());
    const float ceilingDb = audioProcessor.apvts.getRawParameterValue ("ceiling")->load();
    const float boxW = (604.0f - 4 * 8.0f) / 5.0f;
    auto box = [&] (int i) { return juce::Rectangle<float> ((float) meterArea.getX() + (float) i * (boxW + 8.0f), (float) meterArea.getY(), boxW, (float) meterArea.getHeight()); };
    auto fmt = [] (float v) { return v <= -99.0f ? juce::String ("--") : juce::String (v, 1); };

    {
        juce::String st = "--"; juce::Colour sc = LNF::textDim;
        if (lufsS > -99.0f)
        {
            if (std::abs (lufsS - target) <= 1.0f) { st = "OK"; sc = LNF::lime; }
            else if (lufsS > target)               { st = "FORT"; sc = LNF::pink; }
            else                                   { st = "BAS"; sc = LNF::cyan; }
        }
        tchk::drawReadout (g, box (0), "LUFS S", fmt (lufsS), st, sc);
    }
    tchk::drawReadout (g, box (1), "LUFS I", fmt (lufsI), lufsI > -99.0f ? "INTEGRE" : "--", LNF::textDim);
    {
        const bool over = peak > ceilingDb + 0.1f;
        tchk::drawReadout (g, box (2), "TRUE PEAK", fmt (peak), peak <= -99.0f ? "--" : (over ? "CLIP !" : "OK"), over ? LNF::pink : LNF::lime);
    }
    {
        juce::String st = "--"; juce::Colour sc = LNF::textDim;
        if (lufsS > -99.0f)
        {
            if (plr > 12.0f)      { st = "OUVERT"; sc = LNF::cyan; }
            else if (plr > 8.0f)  { st = "PUNCHY"; sc = LNF::lime; }
            else if (plr > 6.0f)  { st = "DENSE"; sc = cClip; }
            else                  { st = "ECRASE"; sc = LNF::pink; }
        }
        tchk::drawReadout (g, box (3), "DYNAMICS", lufsS > -99.0f ? juce::String (plr, 1) : juce::String ("--"), st, sc);
    }
    {
        juce::String st = "OK"; juce::Colour sc = LNF::lime;
        if (corr < 0.0f)      { st = "PHASE !"; sc = LNF::pink; }
        else if (corr < 0.3f) { st = "LARGE"; sc = cClip; }
        tchk::drawReadout (g, box (4), "STEREO", juce::String (corr, 2), st, sc);
    }
}


//==============================================================================
void MasterEditor::timerCallback()
{
    for (auto* m : { &inputMeter, &grLow, &grMid, &grHigh, &clipMeter, &limMeter })
        m->update();

    lufsS = audioProcessor.outputLoudness.shortTerm.load();
    lufsI = audioProcessor.outputLoudness.integrated.load();
    peak = audioProcessor.truePeakHoldDb.load();
    plr = juce::jmax (0.0f, peak - lufsS);
    corr = audioProcessor.correlation.load();

    // mascot animation
    glowPhase += 0.12f;
    if (--blinkCountdown <= 0)
    {
        blink += 0.34f;
        if (blink >= 1.0f) { blink = 0.0f; blinkCountdown = 60 + juce::Random::getSystemRandom().nextInt (120); }
    }
    autoButton.pulse += 0.15f;
    if (autoButton.getToggleState()) autoButton.repaint();

    aButton.setToggleState (audioProcessor.getCurrentSlot() == 0, juce::dontSendNotification);
    bButton.setToggleState (audioProcessor.getCurrentSlot() == 1, juce::dontSendNotification);
    undoButton.setEnabled (audioProcessor.undoManager.canUndo());
    redoButton.setEnabled (audioProcessor.undoManager.canRedo());

    if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        audioProcessor.undoManager.beginNewTransaction();

    spectralDisplay.repaint();
    gauge.blink = audioProcessor.apvts.getRawParameterValue ("bypass")->load() > 0.5f ? 0.9f : juce::jmax (blink, juce::jlimit (0.0f, 0.35f, -audioProcessor.limiterGrDb.load() / 12.0f));
    gauge.glow = glowPhase;
    {
        auto mouse = content.getMouseXYRelative().toFloat();
        gauge.look = { juce::jlimit (-1.0f, 1.0f, (mouse.x - 320.0f) / 250.0f), juce::jlimit (-1.0f, 1.0f, (mouse.y - 200.0f) / 250.0f) };
    }
    gauge.repaint();
    scope.pull();
    scope.repaint();
    spice.repaint();
    content.repaint (meterArea);
    content.repaint (limArea.withTrimmedLeft (480));
}

//==============================================================================
void MasterEditor::AutoButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (8.0f);
    const bool on = getToggleState();

    if (on)
    {
        const float glow = 0.5f + 0.5f * std::sin (pulse);
        g.setColour (LNF::lime.withAlpha (0.25f + 0.25f * glow));
        g.fillEllipse (r.expanded (6.0f));
    }
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (r.translated (0.0f, 3.0f));
    g.setColour (juce::Colour (0xffd9d4e8));
    g.fillEllipse (r);
    auto inner = r.reduced (7.0f);
    auto top = on ? juce::Colour (0xffff6fb0) : juce::Colour (0xffff3d6e);
    if (highlighted) top = top.brighter (0.15f);
    if (down) top = top.darker (0.2f);
    g.setGradientFill (juce::ColourGradient (top, inner.getCentreX(), inner.getY(),
                                             juce::Colour (0xffa0103a), inner.getCentreX(), inner.getBottom(), false));
    g.fillEllipse (inner);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.fillEllipse (inner.getX() + inner.getWidth() * 0.2f, inner.getY() + 4.0f, inner.getWidth() * 0.6f, inner.getHeight() * 0.3f);
    g.setColour (juce::Colours::white);
    g.setFont (LNF::font (20.0f));
    g.drawText ("AUTO", inner, juce::Justification::centred);
}

void MasterEditor::SpectralDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 8.0f);

    const double fs = 48000.0;
    auto xForFreq = [&] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
    auto yForDb = [&] (float db) { return r.getCentreY() - db / 8.0f * (r.getHeight() * 0.42f); };

    g.setColour (LNF::panelLight);
    for (double f : { 100.0, 1000.0, 10000.0 })
        g.drawVerticalLine ((int) xForFreq (f), r.getY() + 4.0f, r.getBottom() - 4.0f);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 4.0f, r.getRight() - 4.0f);
    g.setColour (LNF::textDim);
    g.setFont (LNF::font (9.0f, false));
    g.drawText ("100", (int) xForFreq (100.0) + 2, (int) r.getBottom() - 13, 30, 11, juce::Justification::left);
    g.drawText ("1k", (int) xForFreq (1000.0) + 2, (int) r.getBottom() - 13, 30, 11, juce::Justification::left);
    g.drawText ("10k", (int) xForFreq (10000.0) + 2, (int) r.getBottom() - 13, 30, 11, juce::Justification::left);

    std::array<tchk::BiquadCoeffs, MasterProcessor::numSpectralBands> coeffs;
    for (int b = 0; b < MasterProcessor::numSpectralBands; ++b)
    {
        const float gdb = proc.spectralGainDb[(size_t) b].load();
        if (b == 0)      coeffs[(size_t) b] = tchk::BiquadCoeffs::make (tchk::FilterType::lowShelf, fs, 100.0, 0.7, gdb);
        else if (b == 5) coeffs[(size_t) b] = tchk::BiquadCoeffs::make (tchk::FilterType::highShelf, fs, 9000.0, 0.7, gdb);
        else             coeffs[(size_t) b] = tchk::BiquadCoeffs::make (tchk::FilterType::peak, fs, MasterProcessor::spectralFreqs[(size_t) b], 0.9, gdb);
    }

    juce::Path curve;
    const int steps = 120;
    for (int i = 0; i <= steps; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, (double) i / steps);
        float db = 0.0f;
        for (auto& c : coeffs) db += c.magnitudeDb (f, fs);
        const float x = xForFreq (f), y = juce::jlimit (r.getY() + 2.0f, r.getBottom() - 2.0f, yForDb (db));
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    const bool on = proc.apvts.getRawParameterValue ("spec_on")->load() > 0.5f;
    const auto c = on ? LNF::cyan : LNF::textDim;
    g.setColour (c.withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
void MasterEditor::LoudnessGauge::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const float cx = w * 0.5f, cy = h - 16.0f, R = 128.0f;
    constexpr float lo = -24.0f, hi = -4.0f;
    auto ang = [&] (float v) { return juce::degreesToRadians (-200.0f + (juce::jlimit (lo, hi, v) - lo) / (hi - lo) * 220.0f); };
    auto pt = [&] (float a, float rad) { return juce::Point<float> (cx + std::cos (a) * rad, cy + std::sin (a) * rad); };
    auto arc = [&] (float v0, float v1, float rad)
    {
        juce::Path p;
        const int steps = juce::jmax (2, (int) ((v1 - v0) * 3.0f));
        for (int i = 0; i <= steps; ++i)
        {
            const auto q = pt (ang (v0 + (v1 - v0) * (float) i / (float) steps), rad);
            if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
        }
        return p;
    };
    const float target = MasterProcessor::getTargetLufs ((int) proc.apvts.getRawParameterValue ("target")->load());
    const float now = proc.outputLoudness.shortTerm.load();
    if (now > -70.0f) shown += (now - shown) * 0.15f; else shown += (-30.0f - shown) * 0.05f;

    const juce::PathStrokeType thick (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.strokePath (arc (lo, hi, R), thick);
    g.setColour (LNF::lime.withAlpha (0.3f));
    g.strokePath (arc (target - 1.0f, target + 1.0f, R), juce::PathStrokeType (16.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (LNF::lime);
    g.strokePath (arc (target - 1.0f, target + 1.0f, R), thick);

    const auto col = std::abs (shown - target) <= 1.0f ? LNF::lime : (shown > target ? LNF::pink : LNF::cyan);
    if (shown > lo)
    {
        g.setColour (col);
        g.strokePath (arc (lo, juce::jmin (hi, shown), R - 12.0f), juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setFont (LNF::font (9.0f));
    for (float v = lo; v <= hi; v += 2.0f)
    {
        const bool major = ((int) v % 4) == 0;
        g.setColour (LNF::textDim);
        g.drawLine ({ pt (ang (v), R + 7.0f), pt (ang (v), R + (major ? 12.0f : 10.0f)) }, 1.0f);
        if (v == -24.0f || v == -18.0f || v == -14.0f || v == -10.0f || v == -6.0f)
            g.drawText (juce::String ((int) v), juce::Rectangle<float> (24.0f, 12.0f).withCentre (pt (ang (v), R + 22.0f)), juce::Justification::centred);
    }
    const float a = ang (shown);
    g.setColour (juce::Colours::white);
    g.drawLine ({ pt (a, R - 22.0f), pt (a, R + 6.0f) }, 3.0f);
    g.setColour (col.withAlpha (0.4f));
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (pt (a, R + 6.0f)));

    tchk::drawMascot (g, { 86.0f, 54.0f, 116.0f, 98.0f }, blink, look, glow);

    g.setColour (col);
    g.setFont (LNF::font (15.0f));
    g.drawText (now > -70.0f ? juce::String (shown, 1) + " LUFS" : juce::String ("-- LUFS"), juce::Rectangle<float> (cx - 60.0f, cy - 4.0f, 120.0f, 18.0f), juce::Justification::centred);
}

//==============================================================================
void MasterEditor::Vectorscope::pull()
{
    auto& f = proc.scopeFifo;
    int s1, n1, s2, n2;
    f.prepareToRead (f.getNumReady(), s1, n1, s2, n2);
    auto take = [&] (int start, int count)
    {
        for (int k = 0; k < count; ++k)
        {
            l[(size_t) pos] = proc.scopeL[(size_t) (start + k)];
            r[(size_t) pos] = proc.scopeR[(size_t) (start + k)];
            pos = (pos + 1) % N;
        }
    };
    take (s1, n1);
    take (s2, n2);
    f.finishedRead (n1 + n2);
}

void MasterEditor::Vectorscope::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (b, 8.0f);
    const float cx = b.getCentreX(), cy = b.getCentreY(), R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 6.0f;
    g.setColour (cStereo.withAlpha (0.18f));
    juce::Path diamond;
    diamond.startNewSubPath (cx, cy - R); diamond.lineTo (cx + R, cy); diamond.lineTo (cx, cy + R); diamond.lineTo (cx - R, cy); diamond.closeSubPath();
    g.strokePath (diamond, juce::PathStrokeType (1.0f));
    g.drawVerticalLine ((int) cx, cy - R, cy + R);
    g.drawHorizontalLine ((int) cy, cx - R, cx + R);

    const float corr = proc.correlation.load();
    g.setColour ((corr < 0.0f ? LNF::pink : cStereo).withAlpha (0.75f));
    for (int i = 0; i < N; ++i)
    {
        const float L = l[(size_t) i], Rr = r[(size_t) i];
        const float mid = (L + Rr) * 0.7071f, side = (Rr - L) * 0.7071f;
        const float x = cx + juce::jlimit (-1.0f, 1.0f, side) * R, y = cy - juce::jlimit (-1.0f, 1.0f, mid) * R;
        g.fillRect (x, y, 1.4f, 1.4f);
    }
    g.setColour (LNF::textDim);
    g.setFont (LNF::font (8.0f));
    g.drawText ("M", (int) cx + 2, 2, 10, 10, juce::Justification::centredLeft);
    g.drawText ("G", 3, (int) cy - 11, 10, 10, juce::Justification::centredLeft);
    g.drawText ("D", (int) b.getRight() - 10, (int) cy - 11, 10, 10, juce::Justification::centredLeft);
}

//==============================================================================
MasterEditor::SpiceSlider::SpiceSlider (MasterProcessor& p)
    : proc (p), att (*p.apvts.getParameter ("spice"), [this] (float v) { value = v; repaint(); })
{
    att.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void MasterEditor::SpiceSlider::setFromX (float x)
{
    value = juce::jlimit (0.0f, 1.0f, x / (float) getWidth());
    att.setValueAsPartOfGesture (value);
    proc.applySpice (value);
    repaint();
}

void MasterEditor::SpiceSlider::mouseDown (const juce::MouseEvent& e) { att.beginGesture(); setFromX (e.position.x); }
void MasterEditor::SpiceSlider::mouseDrag (const juce::MouseEvent& e) { setFromX (e.position.x); }
void MasterEditor::SpiceSlider::mouseUp (const juce::MouseEvent&) { att.endGesture(); }

void MasterEditor::SpiceSlider::paint (juce::Graphics& g)
{
    const float w = (float) getWidth();
    const auto track = juce::Rectangle<float> (0.0f, 10.0f, w, 8.0f);
    juce::ColourGradient cg (juce::Colour (0xffffe14d), 0.0f, 0.0f, juce::Colour (0xffff2a6d), w, 0.0f, false);
    cg.addColour (0.4, juce::Colour (0xffffa23d));
    cg.addColour (0.75, juce::Colour (0xffff4f3d));
    g.setGradientFill (cg);
    g.fillRoundedRectangle (track, 4.0f);
    const float x = value * w;
    g.setColour (juce::Colour (255, juce::jlimit (0, 255, (int) (150.0f - value * 110.0f)), 40).withAlpha (0.35f + value * 0.4f));
    g.fillEllipse (juce::Rectangle<float> (28.0f + value * 16.0f, 28.0f + value * 16.0f).withCentre ({ x, 14.0f }));
    g.setColour (juce::Colour (0xfffff4e0));
    g.fillEllipse (juce::Rectangle<float> (24.0f, 24.0f).withCentre ({ x, 14.0f }));
    g.setColour (juce::Colour (0xffff6a3d));
    g.drawEllipse (juce::Rectangle<float> (24.0f, 24.0f).withCentre ({ x, 14.0f }), 3.0f);
}
