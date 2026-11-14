#include "PluginEditor.h"

using tchk::LNF;

namespace
{
    constexpr int kW = 640, kH = 970;
    const juce::Colour cSpectral { 0xffff4fd8 }, cComp { 0xff8f7bff }, cEq { 0xffc6ff3d }, cStereo { 0xff3df5ff },
                       cAnalog { 0xffffa23d }, cClip { 0xffffe14d }, cLimiter { 0xffff4f6d };
}

MasterEditor::MasterEditor (MasterProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.75f),
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
      spectralDisplay (p),
      grLow ([&p] { return p.bandGrDb[0].load(); }, -12.0f, 0.0f, cComp, true),
      grMid ([&p] { return p.bandGrDb[1].load(); }, -12.0f, 0.0f, cComp, true),
      grHigh ([&p] { return p.bandGrDb[2].load(); }, -12.0f, 0.0f, cComp, true),
      clipMeter ([&p] { return p.clipGrDb.load(); }, -12.0f, 0.0f, cClip, true),
      limMeter ([&p] { return p.limiterGrDb.load(); }, -12.0f, 0.0f, cLimiter, true),
      analogMode (p.apvts, "analog_mode")
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.layoutFn = [this] { layout(); };

    // Top bar
    presetBox.setTextWhenNothingSelected ("PRESETS");
    presetBox.addItemList (MasterProcessor::getPresetNames(), 1);
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() > 0)
            audioProcessor.applyPreset (presetBox.getSelectedId() - 1);
    };
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
             &presetBox, &undoButton, &redoButton, &aButton, &bButton, &copyButton, &bypassButton, &matchButton,
             &inputKnob, &inputMeter, &autoButton, &targetBox,
             &specOn, &compOn, &eqOn, &stereoOn, &analogOn, &clipOn, &limOn, &bassMono,
             &specAmt, &glue, &detail, &bass, &air, &width, &color, &clipGain, &limGain, &ceiling, &release, &output,
             &spectralDisplay, &grLow, &grMid, &grHigh, &clipMeter, &limMeter, &analogMode, &resetLufsButton })
        content.addAndMakeVisible (c);

    layout();
    startTimerHz (30);
}

MasterEditor::~MasterEditor()
{
    stopTimer();
}

//==============================================================================
void MasterEditor::layout()
{
    presetBox.setBounds (440, 20, 182, 26);

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
    inputKnob.setBounds (22, 112, 90, 120);
    inputMeter.setBounds (116, 120, 10, 108);
    autoButton.setBounds (478, 104, 110, 110);
    targetBox.setBounds (463, 224, 140, 26);

    constexpr int rowH = 112, gap = 8, top = 270;
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
    width.setBounds (knobAt (stereoArea, 118));
    bassMono.setBounds (stereoArea.getX() + 208, stereoArea.getY() + 26, 80, 24);

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

    // Hero: mascot looking at the mouse
    auto mouse = content.getMouseXYRelative().toFloat();
    const juce::Point<float> centre (320.0f, 180.0f);
    const juce::Point<float> look (juce::jlimit (-1.0f, 1.0f, (mouse.x - centre.x) / 250.0f),
                                   juce::jlimit (-1.0f, 1.0f, (mouse.y - centre.y) / 250.0f));
    const bool bypassed = audioProcessor.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const float squint = juce::jlimit (0.0f, 0.35f, -audioProcessor.limiterGrDb.load() / 12.0f);
    tchk::drawMascot (g, { 220.0f, 96.0f, 200.0f, 168.0f }, bypassed ? 0.9f : juce::jmax (blink, squint), look, glowPhase);

    g.setColour (LNF::textDim);
    g.setFont (LNF::font (10.0f));
    g.drawText ("CIBLE", 463, 252, 140, 12, juce::Justification::centred);

    // Modules
    struct M { juce::Rectangle<int> r; const char* name; juce::Colour c; };
    for (const auto& m : { M { specArea, "Spectral", cSpectral }, M { compArea, "Comp", cComp }, M { eqArea, "EQ", cEq },
                           M { stereoArea, "Stereo", cStereo }, M { analogArea, "Analog", cAnalog }, M { clipArea, "Clip", cClip },
                           M { limArea, "Limiter", cLimiter } })
    {
        tchk::drawPanel (g, m.r.toFloat(), m.c);
        tchk::drawTag (g, { (float) m.r.getX() + 12.0f, (float) m.r.getY() + 12.0f, 100.0f, 24.0f }, m.name, m.c);
    }

    g.setColour (LNF::textDim);
    g.setFont (LNF::font (10.0f));
    const char* bandNames[] = { "L", "M", "H" };
    for (int b = 0; b < 3; ++b)
        g.drawText (bandNames[b], compArea.getX() + 280 + b * 20, compArea.getY() + 90, 20, 12, juce::Justification::centred);
    g.drawText ("GR", compArea.getX() + 280, compArea.getY() + 4, 60, 12, juce::Justification::centred);

    // Stereo correlation
    {
        auto bar = juce::Rectangle<float> ((float) stereoArea.getX() + 208.0f, (float) stereoArea.getY() + 70.0f, 80.0f, 8.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (bar, 3.0f);
        const float px = bar.getX() + (corr * 0.5f + 0.5f) * bar.getWidth();
        g.setColour (corr < 0.0f ? LNF::pink : cStereo);
        g.fillEllipse (px - 5.0f, bar.getCentreY() - 5.0f, 10.0f, 10.0f);
        g.setColour (LNF::textDim);
        g.drawText ("-1        PHASE        +1", bar.translated (0, 12).withHeight (12).toNearestInt(), juce::Justification::centred);
    }

    // Limiter info
    g.setColour (LNF::textDim);
    g.drawText ("GR", limArea.getX() + 494, limArea.getY() + 4, 24, 12, juce::Justification::centred);
    g.setColour (cLimiter);
    g.setFont (LNF::font (13.0f));
    g.drawText (juce::String (audioProcessor.limiterGrDb.load(), 1) + " dB", limArea.getX() + 522, limArea.getY() + 30, 70, 18, juce::Justification::centredLeft);
    g.setColour (LNF::textDim);
    g.setFont (LNF::font (10.0f));
    g.drawText ("TRUE PEAK", limArea.getX() + 522, limArea.getY() + 54, 70, 12, juce::Justification::centredLeft);
    g.drawText ("4x OVERSAMPLE", limArea.getX() + 522, limArea.getY() + 68, 80, 12, juce::Justification::centredLeft);

    g.drawText ("GR", clipArea.getX() + 224, clipArea.getY() + 4, 24, 12, juce::Justification::centred);

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
    content.repaint (heroArea.getUnion ({ 200, 90, 240, 180 }));
    content.repaint (meterArea);
    content.repaint (stereoArea);
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
