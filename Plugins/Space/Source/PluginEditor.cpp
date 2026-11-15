#include "PluginEditor.h"

using tchk::LNF;

namespace
{
    constexpr int kW = 860, kH = 640;
    juce::RangedAudioParameter& param (juce::AudioProcessorValueTreeState& s, const char* id) { return *s.getParameter (id); }
    float value (juce::AudioProcessorValueTreeState& s, const char* id) { return s.getRawParameterValue (id)->load(); }
}

TchiskibidiSpaceEditor::TchiskibidiSpaceEditor (TchiskibidiSpaceProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.85f, "space"),
      audioProcessor (p),
      time (p.apvts, "time", "Time", LNF::pink), feedback (p.apvts, "feedback", "Feedback", LNF::pink),
      wobble (p.apvts, "wobble", "Wobble", LNF::pink), echoLevel (p.apvts, "echolevel", "Level", LNF::pink),
      lowCut (p.apvts, "lowcut", "Low cut", LNF::pink), highCut (p.apvts, "highcut", "High cut", LNF::pink),
      size (p.apvts, "size", "Size", LNF::cyan), damping (p.apvts, "damping", "Damping", LNF::cyan),
      width (p.apvts, "width", "Width", LNF::cyan), reverbLevel (p.apvts, "reverblevel", "Level", LNF::cyan),
      mix (p.apvts, "mix", "Mix", LNF::lime), output (p.apvts, "output", "Output", LNF::lime),
      syncButton (p.apvts, "sync", "Sync", LNF::pink), pingPongButton (p.apvts, "pingpong", "Ping-pong", LNF::pink),
      divisionBox (p.apvts, "division"),
      xyPad (param (p.apvts, "time"), param (p.apvts, "feedback")),
      echoView (p), nebulaView (p),
      inMeter ([&p] { return p.inputPeakDb.load(); }, -48.0f, 0.0f, LNF::lime),
      outMeter ([&p] { return p.outputPeakDb.load(); }, -48.0f, 0.0f, LNF::lime)
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.layoutFn = [this] { layout(); };
    levelDb = [&p] { return p.outputPeakDb.load(); };
    mix.asOrbit(); output.asRocket();

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &time, &feedback, &wobble, &echoLevel, &lowCut, &highCut, &size, &damping, &width, &reverbLevel, &mix, &output,
             &syncButton, &pingPongButton, &divisionBox, &xyPad, &echoView, &nebulaView, &inMeter, &outMeter })
        content.addAndMakeVisible (c);

    addInfo (18 + 16 + 56, 88 + 13, TK_FR ("Glisse le point du pad pour régler l'écho.||Répète ton son. Glisse le point du pad XY : vers la droite les échos sont plus espacés, vers le haut il y a plus de répétitions. SYNC cale l'écho sur le tempo du morceau, PING-PONG le fait voyager de gauche à droite."));
    addInfo (436 + 16 + 78, 88 + 13, TK_FR ("Ajoute de l'espace autour du son.||Ajoute de l'espace. SIZE = taille de la pièce (la nébuleuse grandit), DAMPING = queue plus sombre, WIDTH = largeur stéréo, LEVEL = volume de la réverb."));
    addInfo (674 + 16 + 80, 88 + 13, TK_FR ("Dose d'effet et volume final.||MIX dose le son d'origine et l'effet (0 % = sans effet). OUTPUT = volume final."));

    layout();
    finishSetup (11, { { 250, 22, 54 }, { 330, 44, 34 }, { 420, 18, 40 }, { 520, 12, 44 }, { 610, 36, 30 }, { 700, 16, 38 } });
    startTimerHz (30);
}

void TchiskibidiSpaceEditor::layout()
{
    echoArea = { 18, 88, 404, 534 };
    reverbArea = { 436, 88, 224, 534 };
    masterArea = { 674, 88, 168, 534 };

    xyPad.setBounds (30, 122, 140, 120);
    echoView.setBounds (180, 122, 230, 120);
    nebulaView.setBounds (448, 122, 200, 120);
    inMeter.setBounds (738, 124, 10, 110);
    outMeter.setBounds (754, 124, 10, 110);

    constexpr int row1 = 264, row2 = 420, kh = 152;
    time.setBounds (32, row1, 91, kh);
    feedback.setBounds (127, row1, 91, kh);
    wobble.setBounds (222, row1, 91, kh);
    echoLevel.setBounds (317, row1, 91, kh);
    lowCut.setBounds (32, row2, 91, kh);
    highCut.setBounds (127, row2, 91, kh);
    syncButton.setBounds (230, row2 + 23, 170, 30);
    divisionBox.setBounds (230, row2 + 61, 170, 30);
    pingPongButton.setBounds (230, row2 + 99, 170, 30);

    size.setBounds (450, row1, 96, kh);
    damping.setBounds (550, row1, 96, kh);
    width.setBounds (450, row2, 96, kh);
    reverbLevel.setBounds (550, row2, 96, kh);

    mix.setBounds (688, row1, 140, kh);
    output.setBounds (688, row2, 140, kh);
}

void TchiskibidiSpaceEditor::paintContent (juce::Graphics& g)
{
    sky.drawStaticStars (g);
    g.setColour (LNF::textDim);
    g.setFont (LNF::font (13.0f));
    g.drawText ("T C H I S K I B I D I", 18, 18, 400, 18, juce::Justification::centredLeft);
    g.setColour (LNF::text);
    g.setFont (LNF::font (34.0f));
    g.drawText ("SPACE", 18, 36, 400, 40, juce::Justification::centredLeft);
    g.setColour (LNF::textDim);
    g.setFont (LNF::font (12.0f, false));
    g.drawText ("echo + reverb  |  v0.3", 432, 60, 400, 16, juce::Justification::centredRight);

    auto spPanel = [this, &g] (juce::Rectangle<int> r, const char* title, juce::Colour accent)
    {
        const auto rf = r.toFloat();
        g.setColour (theme.panel.withAlpha (0.72f));
        g.fillRoundedRectangle (rf, 16.0f);
        g.setColour (accent.withAlpha (0.35f));
        g.drawRoundedRectangle (rf.reduced (0.5f), 16.0f, 1.2f);
        g.setColour (accent);
        g.setFont (LNF::font (15.0f));
        g.drawText (title, r.reduced (16, 12).removeFromTop (20), juce::Justification::centredLeft);
    };
    spPanel (echoArea, "ECHO", LNF::pink);
    spPanel (reverbArea, "REVERB", LNF::cyan);
    spPanel (masterArea, "MASTER", LNF::lime);

    g.setColour (LNF::textDim);
    g.setFont (LNF::font (9.0f));
    g.drawText (TK_FR ("TIME →    ↑ FEEDBACK"), 30, 244, 140, 12, juce::Justification::centred);
    g.drawText ("IN", 732, 238, 22, 12, juce::Justification::centred);
    g.drawText ("OUT", 748, 238, 22, 12, juce::Justification::centred);
}

void TchiskibidiSpaceEditor::timerCallback()
{
    const bool sync = value (audioProcessor.apvts, "sync") > 0.5f;
    time.setEnabled (! sync);
    time.setAlpha (sync ? 0.4f : 1.0f);
    divisionBox.setEnabled (sync);

    inMeter.update();
    outMeter.update();

    // onset detection on the input: each hit sends a new wave of echoes
    const float in = audioProcessor.inputPeakDb.load();
    echoView.sinceHit += 1.0 / 30.0;
    if (in > -50.0f && in > slowIn + 6.0f) echoView.sinceHit = 0.0;
    slowIn += (in - slowIn) * 0.08f;

    const float lv = juce::jlimit (0.0f, 1.0f, (audioProcessor.outputPeakDb.load() + 48.0f) / 48.0f);
    nebulaView.pulse += (lv - nebulaView.pulse) * 0.3f;
    nebulaView.phase += 1.0f / 30.0f;
    echoView.phase += 1.0f / 30.0f;
    xyPad.pulse = (float) std::exp (-echoView.sinceHit * 8.0);

    xyPad.repaint();
    echoView.repaint();
    nebulaView.repaint();
}

//==============================================================================
TchiskibidiSpaceEditor::XYPad::XYPad (juce::RangedAudioParameter& t, juce::RangedAudioParameter& f)
    : timeParam (t), fbParam (f),
      timeAtt (t, [this] (float) { repaint(); }), fbAtt (f, [this] (float) { repaint(); })
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void TchiskibidiSpaceEditor::XYPad::setFrom (juce::Point<float> p)
{
    const float x = juce::jlimit (0.0f, 1.0f, p.x / (float) getWidth());
    const float y = juce::jlimit (0.0f, 1.0f, 1.0f - p.y / (float) getHeight());
    timeAtt.setValueAsPartOfGesture (timeParam.convertFrom0to1 (x));
    fbAtt.setValueAsPartOfGesture (fbParam.convertFrom0to1 (y));
}

void TchiskibidiSpaceEditor::XYPad::mouseDown (const juce::MouseEvent& e)
{
    timeAtt.beginGesture(); fbAtt.beginGesture();
    setFrom (e.position);
}
void TchiskibidiSpaceEditor::XYPad::mouseDrag (const juce::MouseEvent& e) { setFrom (e.position); }
void TchiskibidiSpaceEditor::XYPad::mouseUp (const juce::MouseEvent&) { timeAtt.endGesture(); fbAtt.endGesture(); }

void TchiskibidiSpaceEditor::XYPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (LNF::pink.withAlpha (0.3f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (LNF::pink.withAlpha (0.12f));
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine ((int) (r.getWidth() * (float) i / 4.0f), 0.0f, r.getHeight());
        g.drawHorizontalLine ((int) (r.getHeight() * (float) i / 4.0f), 0.0f, r.getWidth());
    }
    const float x = timeParam.getValue() * r.getWidth(), y = (1.0f - fbParam.getValue()) * r.getHeight();
    trail[(size_t) trailPos] = { x, y };
    trailPos = (trailPos + 1) % (int) trail.size();
    for (size_t i = 0; i < trail.size(); ++i)
    {
        const auto age = (float) ((trailPos + (int) trail.size() - 1 - (int) i) % (int) trail.size()) / (float) trail.size();
        g.setColour (LNF::pink.withAlpha (0.25f * (1.0f - age)));
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (trail[i]));
    }
    const float dash[] = { 3.0f, 4.0f };
    g.setColour (LNF::pink.withAlpha (0.5f));
    g.drawDashedLine ({ x, 0.0f, x, r.getHeight() }, dash, 2);
    g.drawDashedLine ({ 0.0f, y, r.getWidth(), y }, dash, 2);
    const float rad = 5.0f + 2.0f * pulse;
    g.setColour (LNF::pink.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (rad * 4.0f, rad * 4.0f).withCentre ({ x, y }));
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre ({ x, y }));
}

//==============================================================================
void TchiskibidiSpaceEditor::EchoView::paint (juce::Graphics& g)
{
    auto& s = proc.apvts;
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r, 8.0f);
    const float w = r.getWidth(), h = r.getHeight();

    const bool pp = value (s, "pingpong") > 0.5f, sync = value (s, "sync") > 0.5f;
    float tms = value (s, "time");
    if (sync)
    {
        static constexpr float beats[] = { 4.0f, 2.0f, 1.0f, 1.5f, 2.0f / 3.0f, 0.5f, 0.75f, 1.0f / 3.0f, 0.25f };
        tms = 500.0f * beats[juce::jlimit (0, 8, (int) value (s, "division"))]; // shown at 120 BPM
    }
    const float fb = value (s, "feedback"), lvl = value (s, "echolevel"), wob = value (s, "wobble");
    const float spacing = juce::jlimit (6.0f, 200.0f, tms / 1000.0f * 110.0f);

    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine ((int) (h * 0.5f), 0.0f, w);
    if (pp)
    {
        g.setColour (LNF::textDim.withAlpha (0.6f));
        g.setFont (LNF::font (9.0f));
        g.drawText ("G", 4, 4, 10, 10, juce::Justification::centred);
        g.drawText ("D", 4, (int) h - 14, 10, 10, juce::Justification::centred);
    }
    const float x0 = 18.0f;
    const float dry = 4.0f + 5.0f * (float) std::exp (-sinceHit * 8.0);
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.fillEllipse (juce::Rectangle<float> (dry * 2.0f, dry * 2.0f).withCentre ({ x0, h * 0.5f }));

    float amp = lvl;
    for (int k = 1; k < 40 && amp > 0.04f; ++k)
    {
        const float x = x0 + (float) k * spacing;
        if (x > w - 6.0f) break;
        const float lane = pp ? (k % 2 ? h * 0.27f : h * 0.73f) : h * 0.5f;
        const float y = lane + std::sin (phase * 5.0f + (float) k) * wob * 10.0f;
        const double age = sinceHit - (double) k * tms / 1000.0;
        const float flash = age >= 0.0 ? (float) std::exp (-age * 7.0) : 0.0f;
        const float rad = 3.0f + 9.0f * amp;
        g.setColour (LNF::pink.withAlpha (0.15f + 0.35f * amp));
        g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre ({ x, y }));
        if (flash > 0.02f)
        {
            g.setColour (juce::Colour (0xffffbef0).withAlpha (flash));
            const float fr = rad * (1.0f + flash * 0.6f);
            g.fillEllipse (juce::Rectangle<float> (fr * 2.0f, fr * 2.0f).withCentre ({ x, y }));
        }
        amp *= fb;
    }
}

//==============================================================================
void TchiskibidiSpaceEditor::NebulaView::paint (juce::Graphics& g)
{
    auto& s = proc.apvts;
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r, 8.0f);
    const float sz = value (s, "size"), damp = value (s, "damping"), wid = value (s, "width"), lvl = value (s, "reverblevel");
    const float R = 22.0f + sz * 58.0f, sx = 0.55f + wid * 0.75f, bright = 1.0f - damp * 0.7f;
    const auto c = r.getCentre();
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (getLocalBounds());
    for (int i = 0; i < 7; ++i)
    {
        const float a = phase * (0.15f + (float) i * 0.03f) + (float) i * 0.9f;
        const float rr = R * (0.55f + 0.45f * std::sin ((float) i * 1.7f + phase * 0.4f)) * (1.0f + 0.25f * pulse * lvl * 2.0f);
        const juce::Point<float> p (c.x + std::cos (a) * R * 0.45f * sx, c.y + std::sin (a * 1.3f) * R * 0.3f);
        const auto col = (i % 2 ? LNF::cyan : juce::Colour (150, 120, 255)).withMultipliedBrightness (bright);
        juce::ColourGradient cg (col.withAlpha (0.45f + lvl * 0.5f), p.x, p.y, col.withAlpha (0.0f), p.x + rr * sx, p.y, true);
        g.setGradientFill (cg);
        g.fillEllipse (p.x - rr * sx, p.y - rr, rr * sx * 2.0f, rr * 2.0f);
    }
    g.setColour (LNF::textDim);
    g.setFont (LNF::font (9.0f));
    g.drawText ("TAILLE " + juce::String (juce::roundToInt (sz * 100.0f)) + " %", 8, (int) r.getHeight() - 16, 100, 12, juce::Justification::centredLeft);
}
