#include "PluginEditor.h"

using tchk::LNF;

namespace
{
    constexpr int kW = 780, kH = 580;
    const juce::Colour cDn { 0xff3df5ff }, cHum { 0xffffa23d }, cClick { 0xffffe14d }, cLow { 0xffc6ff3d },
                       cListen { 0xffff4fd8 }, cIo { 0xfff4ecff };
}

CleanEditor::CleanEditor (CleanProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.85f),
      audioProcessor (p),
      spectrum (p),
      dnOn (p.apvts, "dn_on", "On", cDn), humOn (p.apvts, "hum_on", "On", cHum), clickOn (p.apvts, "click_on", "On", cClick),
      adaptive (p.apvts, "adaptive", "Adaptatif", cDn), listen (p.apvts, "listen", "Ecouter le bruit retire", cListen),
      dnAmount (p.apvts, "dn_amount", "Reduction", cDn), dnSens (p.apvts, "dn_sens", "Sensibilite", cDn),
      humAmt (p.apvts, "hum_amt", "Amount", cHum), clickSens (p.apvts, "click_sens", "Sensibilite", cClick),
      lowcut (p.apvts, "lowcut", "Low cut", cLow), output (p.apvts, "output", "Output", cIo),
      humFreq (p.apvts, "hum_freq"), humHarm (p.apvts, "hum_harm")
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.layoutFn = [this] { layout(); };

    learnButton.setClickingTogglesState (true);
    learnButton.setColour (juce::TextButton::buttonOnColourId, LNF::lime);
    learnAttachment = std::make_unique<tchk::APVTS::ButtonAttachment> (p.apvts, "learn", learnButton);
    clearButton.onClick = [this] { audioProcessor.clearProfile(); tchk::setParam (audioProcessor.apvts, "adaptive", 1.0f); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &spectrum, &dnOn, &humOn, &clickOn, &adaptive, &listen, &dnAmount, &dnSens, &humAmt, &clickSens,
             &lowcut, &output, &humFreq, &humHarm, &learnButton, &clearButton })
        content.addAndMakeVisible (c);

    layout();
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

    tchk::layoutModule (clickArea, &clickOn, { &clickSens }, 40);
    clickSens.setBounds (clickSens.getBounds().withTrimmedBottom (18));

    tchk::layoutModule (lowArea, nullptr, { &lowcut }, 34);
    listen.setBounds (listenArea.getX() + 20, listenArea.getY() + 60, listenArea.getWidth() - 40, 30);
    tchk::layoutModule (outArea, nullptr, { &output }, 34);
}

void CleanEditor::paintContent (juce::Graphics& g)
{
    paintBackgroundAndHeader (g, "Clean", {});
    tchk::drawPanel (g, spectrumArea.toFloat(), cDn);
    tchk::drawModule (g, dnArea, "De-noise", cDn);
    tchk::drawModule (g, humArea, "De-hum", cHum);
    tchk::drawModule (g, clickArea, "De-click", cClick);
    tchk::drawModule (g, lowArea, "Rumble", cLow);
    tchk::drawModule (g, listenArea, "Ecoute", cListen);
    tchk::drawModule (g, outArea, "Output", cIo);

    g.setColour (LNF::textDim);
    g.setFont (LNF::font (10.0f));
    g.drawText ("FREQUENCE", humFreq.getBounds().translated (0, -14).withHeight (12), juce::Justification::centred);
    g.drawText ("HARMONIQUES", humHarm.getBounds().translated (-8, -14).withHeight (12).withWidth (100), juce::Justification::centred);
    g.drawText (juce::String (audioProcessor.clicksRepaired.load()) + " CLICS REPARES",
                clickArea.getX() + 8, clickArea.getBottom() - 24, clickArea.getWidth() - 16, 14, juce::Justification::centred);
    g.setFont (LNF::font (10.0f, false));
    g.drawText (audioProcessor.hasLearnedProfile() ? "PROFIL DE BRUIT : APPRIS" : "PROFIL DE BRUIT : AUTO",
                dnArea.getX() + 14, dnArea.getBottom() - 20, 200, 14, juce::Justification::centredLeft);
    g.drawText ("Entends uniquement ce qui est enleve,", listenArea.getX() + 20, listenArea.getY() + 36, 300, 14, juce::Justification::centredLeft);
    g.drawText ("pour verifier que tu ne retires pas la voix.", listenArea.getX() + 20, listenArea.getY() + 96, 300, 14, juce::Justification::centredLeft);

    tchk::drawMascot (g, { 122.0f, 6.0f, 60.0f, 52.0f }, 0.5f, { 0.0f, 0.4f }, 0.0f);
}

void CleanEditor::timerCallback()
{
    spectrum.update();
    const float prog = audioProcessor.learnProgress.load();
    learnButton.setButtonText (learnButton.getToggleState() ? "ECOUTE... " + juce::String (juce::roundToInt (prog * 100.0f)) + "%" : "LEARN");
    content.repaint (dnArea);
    content.repaint (clickArea);
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
