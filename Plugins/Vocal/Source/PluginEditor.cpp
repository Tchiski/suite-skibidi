#include "PluginEditor.h"

using tchk::LNF;

namespace
{
    constexpr int kW = 780, kH = 780;
    const juce::Colour cLevel { 0xffc6ff3d }, cDeess { 0xff3df5ff }, cEq { 0xffff4fd8 }, cComp { 0xff8f7bff },
                       cWarm { 0xffffa23d }, cDbl { 0xffffe14d }, cDelay { 0xff3df5ff }, cVerb { 0xffd08bff }, cIo { 0xfff4ecff }, cTune { 0xff3df5c8 };
}

VocalEditor::VocalEditor (VocalProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.85f, "vocal"),
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
      pitchRoll (p.autotune),
      levelMeter ([&p] { return std::abs (p.levelerDb.load()) - 12.0f; }, -12.0f, 0.0f, cLevel),
      deessMeter ([&p] { return p.deesserGrDb.load(); }, -18.0f, 0.0f, cDeess, true),
      compMeter ([&p] { return p.compGrDb.load(); }, -18.0f, 0.0f, cComp, true),
      inMeter ([&p] { return p.inputPeakDb.load(); }, -48.0f, 0.0f, LNF::lime),
      outMeter ([&p] { return p.outputPeakDb.load(); }, -48.0f, 0.0f, LNF::lime)
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.layoutFn = [this] { layout(); };
    levelDb = [&p] { return p.outputPeakDb.load(); };
    p.autotune.displayWanted = true;

    presets = std::make_unique<tchk::PresetSystem> (p, "Vocal", presetBox, heartButton,
        [] (juce::ComboBox& b)
        {
            const auto names = VocalProcessor::getPresetNames();
            for (int i = 0; i < names.size(); ++i)
            {
                if (i == VocalProcessor::numGenericPresets)
                    b.addSectionHeading ("ARTISTES");
                b.addItem (names[i], i + 1);
            }
        },
        [this] (int i) { audioProcessor.applyPreset (i); });
    delayMix.asOrbit(); verbMix.asOrbit(); input.asRocket(); output.asRocket();

    curve.colour = cEq;
    curve.rangeDb = 12.0f;
    curve.getCoeffs = [this]
    {
        if (audioProcessor.apvts.getRawParameterValue ("eq_on")->load() < 0.5f) return std::vector<tchk::BiquadCoeffs>();
        return VocalProcessor::makeEqCoeffs (audioProcessor.apvts, 48000.0);
    };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &presetBox, &heartButton, &tuneOn, &tuneSpeed, &tuneAmount, &tuneKey, &tuneScale, &pitchRoll, &sFlash, &levelOn, &deessOn, &eqOn, &compOn, &warmOn, &dblOn, &delayOn, &verbOn,
             &levelAmt, &deessFreq, &deessAmt, &lowcut, &mud, &presence, &air, &compAmt, &compSpeed, &warmth,
             &dblAmt, &dblWidth, &delayMix, &delayFb, &verbMix, &verbSize, &input, &output, &delayDiv, &curve,
             &levelMeter, &deessMeter, &compMeter, &inMeter, &outMeter })
        content.addAndMakeVisible (c);

    layout();
    addModuleInfo (levelArea, "Level", TK_FR ("Volume de voix régulier, automatiquement.||Égalise automatiquement le volume de la voix, comme un ingé son qui bouge le fader."), true);
    addModuleInfo (deessArea, "De-ess", TK_FR ("Calme les « S » qui sifflent. Le S s'allume quand il agit. FREQ = zone des sifflantes."));
    addModuleInfo (eqArea, "EQ voix", TK_FR ("Une zone de la voix par bouton.||LOW CUT enlève les graves inutiles, MUD le son boueux vers 300 Hz, PRESENCE fait ressortir la voix, AIR ajoute de la brillance."));
    addModuleInfo (compArea, "Comp", TK_FR ("Volume plus régulier.||Rend le volume plus régulier. AMOUNT = force, SPEED = vitesse."));
    addInfo (warmArea.getRight() - 26, warmArea.getBottom() - 26, TK_FR ("Chaleur façon lampe.||Saturation chaude façon lampe."));
    addModuleInfo (dblArea, "Double", TK_FR ("Élargit la voix.||Imite une deuxième prise de voix, pour élargir le son."));
    addModuleInfo (inArea, "Input", TK_FR ("Volume d'entrée."));
    addModuleInfo (tuneArea, "Autotune", TK_FR ("Choisis ta tonalité : la voix est recalée dessus.||Corrige la justesse. Choisis la tonalité et la gamme de ton morceau (Chromatique marche partout). SPEED 0 ms = effet robot, 100 ms et + = naturel. Sur le piano roll : ligne blanche = ta voix, ligne turquoise = la voix corrigée. La roue de réglages permet de choisir le type de voix."));
    addModuleInfo (delayArea, "Delay", TK_FR ("Échos calés sur le tempo.||Échos calés sur le tempo du morceau. TEMPO = longueur des échos."));
    addModuleInfo (verbArea, "Reverb", TK_FR ("Espace autour de la voix.||Espace autour de la voix : MIX = quantité, SIZE = taille."));
    addModuleInfo (outArea, "Output", TK_FR ("Volume de sortie."));

    settingsExtras.push_back ({ TK_FR ("Autotune : type de voix"), tchk::AutoTune::rangeNames(),
                                [this] { return audioProcessor.tuneRange.load(); },
                                [this] (int v) { audioProcessor.tuneRange = v; } });
    extraReset = [this] { audioProcessor.tuneRange = 1; };

    finishSetup (33, { { 230, 16, 48 }, { 310, 36, 30 }, { 400, 12, 42 }, { 480, 34, 30 }, { 535, 10, 32 } });
    startTimerHz (30);
}

void VocalEditor::layout()
{
    presetBox.setBounds (kW - 236, 20, 182, 26);
    heartButton.setBounds (kW - 268, 20, 26, 26);

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
    levelOn.setBounds (onBoundsCompact (levelArea));
    levelMeter.setBounds (levelArea.getRight() - 26, levelArea.getY() + 50, 10, 96);

    tchk::layoutModule (deessArea.withTrimmedRight (26), nullptr, { &deessFreq, &deessAmt }, 44);
    deessOn.setBounds (onBoundsCompact (deessArea));
    sFlash.setBounds (onBoundsCompact (deessArea).getX() - 34, deessArea.getY() + 10, 28, 22);
    deessMeter.setBounds (deessArea.getRight() - 26, deessArea.getY() + 50, 10, 96);

    eqOn.setBounds (eqArea.getRight() - 60, eqArea.getY() + 10, 48, 22);
    curve.setBounds (eqArea.getX() + 12, eqArea.getY() + 38, eqArea.getWidth() - 24, 44);
    tchk::layoutModule (eqArea, nullptr, { &lowcut, &mud, &presence, &air }, 86);

    tchk::layoutModule (compArea.withTrimmedRight (26), nullptr, { &compAmt, &compSpeed }, 44);
    compOn.setBounds (compArea.getRight() - 60, compArea.getY() + 10, 48, 22);
    compMeter.setBounds (compArea.getRight() - 26, compArea.getY() + 50, 10, 96);

    tchk::layoutModule (warmArea, nullptr, { &warmth }, 44);
    warmOn.setBounds (onBoundsCompact (warmArea));
    tchk::layoutModule (dblArea, &dblOn, { &dblAmt, &dblWidth }, 44);
    tchk::layoutModule (inArea.withTrimmedRight (22), nullptr, { &input }, 44);
    inMeter.setBounds (inArea.getRight() - 24, inArea.getY() + 50, 10, 96);

    tuneOn.setBounds (onBounds (tuneArea));
    tuneKey.setBounds (tuneArea.getX() + 20, tuneArea.getY() + 60, 120, 28);
    tuneScale.setBounds (tuneArea.getX() + 20, tuneArea.getY() + 114, 120, 28);
    tuneSpeed.setBounds (tuneArea.getX() + 150, tuneArea.getY() + 40, 92, 122);
    tuneAmount.setBounds (tuneArea.getX() + 244, tuneArea.getY() + 40, 92, 122);
    pitchRoll.setBounds (tuneArea.getX() + 346, tuneArea.getY() + 40, 382, 120);

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
    drawModuleT (g, levelArea, "Level", cLevel, true);
    drawModuleT (g, deessArea, "De-ess", cDeess);
    drawModuleT (g, eqArea, "EQ voix", cEq);
    drawModuleT (g, compArea, "Comp", cComp);
    drawModuleT (g, warmArea, "Warm", cWarm, true);
    drawModuleT (g, dblArea, "Double", cDbl);
    drawModuleT (g, inArea, "Input", cIo);
    drawModuleT (g, tuneArea, "Autotune", cTune);
    drawModuleT (g, delayArea, "Delay", cDelay);
    drawModuleT (g, verbArea, "Reverb", cVerb);
    drawModuleT (g, outArea, "Output", cIo);

    g.setColour (LNF::textDim);
    g.setFont (LNF::font (10.0f));
    g.drawText (TK_FR ("TONALITÉ"), tuneKey.getBounds().translated (0, -16).withHeight (14), juce::Justification::centred);
    g.drawText ("GAMME", tuneScale.getBounds().translated (0, -16).withHeight (14), juce::Justification::centred);
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
    pitchRoll.push();
    pitchRoll.repaint();
    const float gr = -audioProcessor.deesserGrDb.load();
    sFlash.amount = juce::jmax (juce::jlimit (0.0f, 1.0f, (gr - 1.0f) / 5.0f), sFlash.amount * 0.8f);
    sFlash.repaint();
}

//==============================================================================
void VocalEditor::PitchRoll::push()
{
    const float s = tune.detectedNote.load();
    sung[(size_t) pos] = s;
    fixed[(size_t) pos] = s >= 0.0f ? s + tune.shiftSemis.load() : -1.0f;
    pos = (pos + 1) % N;
    if (s >= 0.0f) centre += (s - centre) * 0.05f; // the view follows the voice
}

void VocalEditor::PitchRoll::paint (juce::Graphics& g)
{
    static const char* names[] = { "DO", "DO#", "RE", "RE#", "MI", "FA", "FA#", "SOL", "SOL#", "LA", "LA#", "SI" };
    const juce::Colour accent (0xff3df5c8);
    auto r = getLocalBounds().toFloat();
    const float w = r.getWidth(), h = r.getHeight(), left = 30.0f;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r, 8.0f);

    const int lo = (int) std::round (centre) - 6, hi = lo + 12;
    const float rowH = h / (float) (hi - lo);
    auto ny = [&] (float n) { return h - (n - (float) lo) / (float) (hi - lo) * h; };
    const int last = (pos + N - 1) % N;
    const float now = sung[(size_t) last];
    const int target = now >= 0.0f ? (int) std::round (fixed[(size_t) last]) : -100;
    for (int n = lo; n <= hi; ++n)
    {
        const int pc = ((n % 12) + 12) % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        g.setColour (n == target ? accent.withAlpha (0.18f) : (black ? juce::Colours::black.withAlpha (0.25f) : juce::Colours::white.withAlpha (0.025f)));
        g.fillRect (left, ny ((float) n + 0.5f), w - left, rowH);
        if ((pc == 0 || pc == 4 || pc == 7 || pc == 9) && n > lo && n < hi)
        {
            g.setColour (LNF::textDim);
            g.setFont (LNF::font (8.0f, false));
            g.drawText (juce::String (names[pc]) + juce::String (n / 12 - 1), 2, (int) ny ((float) n) - 5, 28, 10, juce::Justification::centredLeft);
        }
    }
    auto line = [&] (const std::array<float, N>& a, juce::Colour c, float thick)
    {
        juce::Path p;
        bool drawing = false;
        for (int i = 0; i < N; ++i)
        {
            const float v = a[(size_t) ((pos + i) % N)];
            const float x = left + (float) i / (float) (N - 1) * (w - left - 4.0f);
            if (v < 0.0f || v < (float) lo - 1.0f || v > (float) hi + 1.0f) { drawing = false; continue; }
            if (! drawing) { p.startNewSubPath (x, ny (v)); drawing = true; } else p.lineTo (x, ny (v));
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    line (sung, LNF::text.withAlpha (0.6f), 1.5f);
    line (fixed, accent.withAlpha (0.35f), 6.0f);
    line (fixed, accent, 2.0f);

    auto noteName = [] (float m) { const int k = (int) std::round (m); return juce::String (names[((k % 12) + 12) % 12]) + juce::String (k / 12 - 1); };
    g.setColour (juce::Colour (0xff0a0616).withAlpha (0.8f));
    g.fillRoundedRectangle (w - 116.0f, 4.0f, 112.0f, 34.0f, 6.0f);
    g.setColour (LNF::text);
    g.setFont (LNF::font (12.0f));
    g.drawText (now >= 0.0f ? noteName (now) + juce::String (juce::CharPointer_UTF8 ("  →  ")) + noteName ((float) target) : juce::String ("--"),
                juce::Rectangle<float> (w - 112.0f, 6.0f, 104.0f, 16.0f), juce::Justification::centredLeft);
    if (now >= 0.0f)
    {
        const int cents = juce::roundToInt ((now - (float) target) * 100.0f);
        g.setColour (std::abs (cents) < 10 ? LNF::lime : accent);
        g.setFont (LNF::font (9.0f));
        g.drawText ((cents > 0 ? "+" : "") + juce::String (cents) + " cents", juce::Rectangle<float> (w - 112.0f, 22.0f, 104.0f, 12.0f), juce::Justification::centredLeft);
    }
}

void VocalEditor::SFlash::paint (juce::Graphics& g)
{
    const juce::Colour c (0xff3df5ff);
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    if (amount > 0.2f)
    {
        g.setColour (c.withAlpha (0.3f * amount));
        g.fillRoundedRectangle (r.expanded (2.0f), 7.0f);
        g.setColour (c.withAlpha (juce::jlimit (0.3f, 1.0f, amount)));
        g.fillRoundedRectangle (r, 6.0f);
    }
    g.setColour (amount > 0.2f ? LNF::background : c.withAlpha (0.5f));
    g.drawRoundedRectangle (r, 6.0f, 1.5f);
    g.setFont (LNF::font (15.0f));
    g.drawText ("S", r, juce::Justification::centred);
}
