#include "PluginEditor.h"

using tchk::LNF;

namespace
{
    constexpr int kW = 780, kH = 580;
    const juce::Colour cDn { 0xff3df5ff }, cHum { 0xffffa23d }, cClick { 0xffffe14d }, cLow { 0xffc6ff3d },
                       cListen { 0xffff4fd8 }, cIo { 0xfff4ecff };
}

CleanEditor::CleanEditor (CleanProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.85f, "clean"),
      audioProcessor (p),
      spectrum (p),
      dnOn (p.apvts, "dn_on", "On", cDn), humOn (p.apvts, "hum_on", "On", cHum), clickOn (p.apvts, "click_on", "On", cClick),
      adaptive (p.apvts, "adaptive", "Adaptatif", cDn), listen (p.apvts, "listen", TK_FR ("Écouter le bruit retiré"), cListen),
      dnAmount (p.apvts, "dn_amount", TK_FR ("Réduction"), cDn), dnSens (p.apvts, "dn_sens", TK_FR ("Sensibilité"), cDn),
      humAmt (p.apvts, "hum_amt", "Amount", cHum), clickSens (p.apvts, "click_sens", TK_FR ("Sensibilité"), cClick),
      lowcut (p.apvts, "lowcut", "Low cut", cLow), output (p.apvts, "output", "Output", cIo),
      humFreq (p.apvts, "hum_freq"), humHarm (p.apvts, "hum_harm")
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.layoutFn = [this] { layout(); };
    content.overFn = [this] (juce::Graphics& g) { paintDynamic (g); };
    levelDb = [&p] { return p.outputPeakDb.load(); };
    output.asRocket();
    p.displayWanted = true;

    learnButton.setClickingTogglesState (true);
    learnButton.setColour (juce::TextButton::buttonOnColourId, LNF::lime);
    learnAttachment = std::make_unique<tchk::APVTS::ButtonAttachment> (p.apvts, "learn", learnButton);
    clearButton.onClick = [this] { audioProcessor.clearProfile(); tchk::setParam (audioProcessor.apvts, "adaptive", 1.0f); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &spectrum, &dnOn, &humOn, &clickOn, &adaptive, &listen, &dnAmount, &dnSens, &humAmt, &clickSens,
             &lowcut, &output, &humFreq, &humHarm, &learnButton, &clearButton })
        content.addAndMakeVisible (c);

    layout();
    addInfo (spectrumArea.getX() + 130, spectrumArea.getY() + 15, TK_FR ("Rose = ton son, cyan = le bruit repéré.||Le spectre de ton son : en rose le signal, en cyan le bruit que Clean a repéré. Plus la ligne cyan est basse, plus ton enregistrement est propre."));
    addModuleInfo (dnArea, "De-noise", TK_FR ("Appuie LEARN sur un passage de bruit seul.||Enlève le souffle. Appuie sur LEARN pendant un passage où il n'y a que du bruit (3 secondes) : Clean l'apprend. ADAPTATIF suit le bruit tout seul, EFFACER oublie ce qui a été appris. SENSIBILITÉ : plus haut = enlève plus."));
    addModuleInfo (humArea, "De-hum", TK_FR ("Enlève le bourdonnement. 50 Hz en Europe.||Enlève le bourdonnement électrique. Mets 50 Hz en Europe. HARMONIQUES = nombre d'échos du bourdonnement retirés."));
    addModuleInfo (clickArea, "De-click", TK_FR ("Répare clics et craquements.||Répare les petits clics et craquements. Le compteur montre combien il en a réparé."), true);
    addModuleInfo (lowArea, "Rumble", TK_FR ("Coupe les grondements graves (vent, pas, clim)."));
    addModuleInfo (listenArea, TK_FR ("Écoute"), TK_FR ("Écoute seulement ce qui est retiré.||Fais entendre seulement ce qui est retiré : si tu entends la voix, tu en enlèves trop."));
    addModuleInfo (outArea, "Output", TK_FR ("Volume de sortie."));
    finishSetup (44, { { 230, 14, 50 }, { 320, 36, 32 }, { 420, 14, 40 }, { 520, 30, 34 }, { 640, 16, 44 } });
    startTimerHz (30);
}

void CleanEditor::layout()
{
    spectrumArea = { 18, 64, 744, 186 };
    dnArea       = { 18, 258, 340, 176 };
    humArea      = { 366, 258, 200, 176 };
    clickArea    = { 574, 258, 188, 176 };
    lowArea      = { 18, 442, 200, 122 };
    listenArea   = { 226, 442, 330, 122 };
    outArea      = { 564, 442, 198, 122 };

    spectrum.setBounds (spectrumArea.reduced (10));

    tchk::layoutModule (dnArea.withTrimmedRight (124), nullptr, { &dnAmount, &dnSens }, 40);
    dnOn.setBounds (dnArea.getRight() - 60, dnArea.getY() + 10, 48, 22);
    learnButton.setBounds (dnArea.getRight() - 124, dnArea.getY() + 44, 110, 44);
    adaptive.setBounds (dnArea.getRight() - 124, dnArea.getY() + 98, 110, 26);
    clearButton.setBounds (dnArea.getRight() - 124, dnArea.getY() + 132, 110, 24);

    humOn.setBounds (humArea.getRight() - 60, humArea.getY() + 10, 48, 22);
    humFreq.setBounds (humArea.getX() + 12, humArea.getY() + 48, 84, 26);
    humHarm.setBounds (humArea.getX() + 12, humArea.getY() + 112, 84, 26);
    humAmt.setBounds (humArea.getX() + 104, humArea.getY() + 44, 86, 112);

    tchk::layoutModule (clickArea, nullptr, { &clickSens }, 40);
    clickOn.setBounds (onBoundsCompact (clickArea));
    clickSens.setBounds (clickSens.getBounds().withTrimmedBottom (18));

    tchk::layoutModule (lowArea, nullptr, { &lowcut }, 34);
    listen.setBounds (listenArea.getX() + 20, listenArea.getY() + 60, listenArea.getWidth() - 40, 30);
    tchk::layoutModule (outArea, nullptr, { &output }, 34);
}

void CleanEditor::paintContent (juce::Graphics& g)
{
    paintBackgroundAndHeader (g, "Clean", {});
    drawPanelT (g, spectrumArea, cDn);
    drawModuleT (g, dnArea, "De-noise", cDn);
    drawModuleT (g, humArea, "De-hum", cHum);
    drawModuleT (g, clickArea, "De-click", cClick, true);
    drawModuleT (g, lowArea, "Rumble", cLow);
    drawModuleT (g, listenArea, TK_FR ("Écoute"), cListen);
    drawModuleT (g, outArea, "Output", cIo);

    g.setColour (LNF::textDim);
    g.setFont (LNF::font (10.0f));
    g.drawText (TK_FR ("FRÉQUENCE"), humFreq.getBounds().translated (0, -14).withHeight (12), juce::Justification::centred);
    g.drawText ("HARMONIQUES", humHarm.getBounds().translated (-8, -14).withHeight (12).withWidth (100), juce::Justification::centred);
    g.setFont (LNF::font (10.0f, false));
    g.drawText (TK_FR ("Entends uniquement ce qui est enlevé,"), listenArea.getX() + 20, listenArea.getY() + 36, 300, 14, juce::Justification::centredLeft);
    g.drawText (TK_FR ("pour vérifier que tu ne retires pas la voix."), listenArea.getX() + 20, listenArea.getY() + 96, 300, 14, juce::Justification::centredLeft);
}

void CleanEditor::paintDynamic (juce::Graphics& g)
{
    // the mascot: while LEARN listens, it bobs its head and notes float around it
    const bool learning = learnButton.getToggleState();
    const juce::Rectangle<float> m (122.0f, 6.0f, 60.0f, 52.0f);
    if (learning)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::rotation (std::sin (phase * 3.0f) * 0.1f, m.getCentreX(), m.getBottom())
                          .translated (0.0f, std::sin (phase * 8.0f) * 2.0f));
        tchk::drawMascot (g, m, 0.0f, { std::sin (phase * 1.5f), 0.0f }, phase * 4.0f);
        g.setColour (LNF::lime.withAlpha (0.5f + 0.5f * std::sin (phase * 6.0f)));
        g.setFont (juce::Font (juce::FontOptions ("Georgia", 15.0f, juce::Font::bold)));
        const float rise = std::fmod (phase * 20.0f, 16.0f);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("♪")), juce::Rectangle<float> (m.getRight() - 4.0f + std::sin (phase * 4.0f) * 5.0f, m.getY() + 10.0f - rise, 16.0f, 16.0f), juce::Justification::centred);
    }
    else
        tchk::drawMascot (g, m, 0.5f, { 0.0f, 0.4f }, 0.0f);

    // progress ring around LEARN
    const float prog = audioProcessor.learnProgress.load();
    if (learning)
    {
        const auto rb = learnButton.getBounds().toFloat().expanded (4.0f);
        juce::Path full;
        full.addRoundedRectangle (rb, 9.0f);
        g.setColour (LNF::lime.withAlpha (0.15f));
        g.strokePath (full, juce::PathStrokeType (3.0f));
        const float len = full.getLength() * prog;
        juce::Path progress;
        bool first = true;
        for (float d = 0.0f; d <= len; d += 2.0f)
        {
            const auto pt = full.getPointAlongPath (d);
            if (first) { progress.startNewSubPath (pt); first = false; } else progress.lineTo (pt);
        }
        g.setColour (LNF::lime);
        g.strokePath (progress, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (LNF::textDim);
    g.setFont (LNF::font (10.0f));
    g.drawText (juce::String (audioProcessor.clicksRepaired.load()) + TK_FR (" CLICS RÉPARÉS"),
                clickArea.getX() + 8, clickArea.getBottom() - 24, clickArea.getWidth() - 16, 14, juce::Justification::centred);
    g.setFont (LNF::font (9.0f, false));
    g.drawText (audioProcessor.hasLearnedProfile() ? TK_FR ("PROFIL : APPRIS ✓") : juce::String ("PROFIL : AUTO"),
                learnButton.getX() - 6, dnArea.getY() + 158, 122, 12, juce::Justification::centred);
}

void CleanEditor::timerCallback()
{
    spectrum.update();
    const float prog = audioProcessor.learnProgress.load();
    learnButton.setButtonText (learnButton.getToggleState() ? "ECOUTE... " + juce::String (juce::roundToInt (prog * 100.0f)) + "%" : "LEARN");
    phase += 1.0f / 30.0f;
    content.repaint (dnArea);
    content.repaint (clickArea);
    if (learnButton.getToggleState()) content.repaint (100, 0, 120, 64);
}

//==============================================================================
void CleanEditor::SpectrumView::update()
{
    for (int b = 0; b < CleanProcessor::displayBins; ++b)
    {
        const float in = proc.displayInput[(size_t) b].load(), nz = proc.displayNoise[(size_t) b].load();
        inSm[(size_t) b] = in > inSm[(size_t) b] ? in : inSm[(size_t) b] + (in - inSm[(size_t) b]) * 0.15f;
        nzSm[(size_t) b] += (nz - nzSm[(size_t) b]) * 0.2f;
    }
    repaint();
}

void CleanEditor::SpectrumView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 8.0f);

    auto yFor = [&] (float db) { return juce::jlimit (r.getY(), r.getBottom(), r.getBottom() - (db + 100.0f) / 100.0f * r.getHeight()); };
    auto xFor = [&] (int b) { return r.getX() + r.getWidth() * (float) b / (float) (CleanProcessor::displayBins - 1); };

    g.setColour (LNF::panelLight);
    for (float db : { -80.0f, -60.0f, -40.0f, -20.0f })
        g.drawHorizontalLine ((int) yFor (db), r.getX() + 4.0f, r.getRight() - 4.0f);
    g.setColour (LNF::textDim);
    g.setFont (LNF::font (9.0f, false));
    for (auto [f, t] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
    {
        const float x = r.getX() + r.getWidth() * (float) (std::log (f / 30.0) / std::log (20000.0 / 30.0));
        g.drawVerticalLine ((int) x, r.getY() + 4.0f, r.getBottom() - 4.0f);
        g.drawText (t, (int) x + 3, (int) r.getBottom() - 13, 30, 11, juce::Justification::left);
    }

    juce::Path in, inFill, nz;
    for (int b = 0; b < CleanProcessor::displayBins; ++b)
    {
        const float x = xFor (b);
        if (b == 0) { in.startNewSubPath (x, yFor (inSm[0])); inFill.startNewSubPath (x, r.getBottom()); nz.startNewSubPath (x, yFor (nzSm[0])); }
        else { in.lineTo (x, yFor (inSm[(size_t) b])); nz.lineTo (x, yFor (nzSm[(size_t) b])); }
        inFill.lineTo (x, yFor (inSm[(size_t) b]));
    }
    inFill.lineTo (r.getRight(), r.getBottom());
    inFill.closeSubPath();

    g.setColour (LNF::pink.withAlpha (0.18f));
    g.fillPath (inFill);
    g.setColour (LNF::pink);
    g.strokePath (in, juce::PathStrokeType (1.5f));
    g.setColour (LNF::cyan.withAlpha (0.3f));
    g.strokePath (nz, juce::PathStrokeType (5.0f));
    g.setColour (LNF::cyan);
    g.strokePath (nz, juce::PathStrokeType (2.0f));

    g.setFont (LNF::font (10.0f));
    g.setColour (LNF::pink);
    g.drawText ("SIGNAL", (int) r.getX() + 10, (int) r.getY() + 8, 80, 12, juce::Justification::left);
    g.setColour (LNF::cyan);
    g.drawText ("BRUIT", (int) r.getX() + 70, (int) r.getY() + 8, 80, 12, juce::Justification::left);
}
