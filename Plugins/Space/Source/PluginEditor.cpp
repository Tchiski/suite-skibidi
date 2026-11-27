#include "PluginEditor.h"

using tchk::LNF;

namespace
{
    constexpr int kW = 860, kH = 640;
    juce::RangedAudioParameter& param (juce::AudioProcessorValueTreeState& s, const char* id) { return *s.getParameter (id); }
    // the window is the rocket: hull, the two fins, and the cockpit dome with the name
    juce::Path finTop() { juce::Path f; f.startNewSubPath (250, 92); f.lineTo (300, 58); f.lineTo (400, 58); f.lineTo (390, 92); f.closeSubPath(); return f; }
    juce::Path finBottom() { juce::Path f; f.startNewSubPath (250, 620); f.lineTo (300, 638); f.lineTo (400, 638); f.lineTo (390, 620); f.closeSubPath(); return f; }
    juce::Path hullPath() { juce::Path h; h.addRoundedRectangle (14, 88, 832, 540, 70); return h; }
    juce::Path domePath() { juce::Path d; d.addEllipse (10, 8, 220, 150); return d; }
    juce::Path rocketShape()
    {
        juce::Path p = hullPath();
        p.addPath (finTop()); p.addPath (finBottom()); p.addPath (domePath());
        p.setUsingNonZeroWinding (true);
        return p;
    }
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
    for (auto* k : { &time, &feedback, &wobble, &echoLevel, &lowCut, &highCut, &size, &damping, &width, &reverbLevel }) k->asPorthole();

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &time, &feedback, &wobble, &echoLevel, &lowCut, &highCut, &size, &damping, &width, &reverbLevel, &mix, &output,
             &syncButton, &pingPongButton, &divisionBox, &xyPad, &echoView, &nebulaView, &inMeter, &outMeter })
        content.addAndMakeVisible (c);

    addInfo (122, 102, TK_FR ("Glisse le point du pad pour régler l'écho.||Répète ton son. Glisse le point du pad XY : vers la droite les échos sont plus espacés, vers le haut il y a plus de répétitions. SYNC cale l'écho sur le tempo du morceau, PING-PONG le fait voyager de gauche à droite."));
    addInfo (540, 102, TK_FR ("Ajoute de l'espace autour du son.||Ajoute de l'espace. SIZE = taille de la pièce (la nébuleuse grandit), DAMPING = queue plus sombre, WIDTH = largeur stéréo, LEVEL = volume de la réverb."));
    addInfo (778, 102, TK_FR ("Dose d'effet et volume final.||MIX dose le son d'origine et l'effet (0 % = sans effet). OUTPUT = volume final."));

    layout();
    windowShape = [] { return rocketShape(); };
    gearBounds = { 810, 98, 26, 26 };
    closeBounds = { 808, 132, 30, 30 };
    finishSetup (11, {});
    startTimerHz (30);
}

void TchiskibidiSpaceEditor::layout()
{
    echoArea = { 18, 88, 404, 534 };
    reverbArea = { 436, 88, 224, 534 };
    masterArea = { 674, 88, 168, 534 };

    xyPad.setBounds (34, 134, 136, 104);
    echoView.setBounds (184, 132, 222, 92);
    nebulaView.setBounds (490, 128, 116, 116);
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

    // the rocket: a capsule hull with fins, riveted seams, the windows set into it
    const juce::Colour steel (0xff1a2b48);
    {
        auto finT = finTop(), finB = finBottom();
        // the cockpit dome (glass) behind the hull
        {
            const auto dome = domePath();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6ad8ff).withAlpha (0.55f), 60, 20, juce::Colour (0xff102040).withAlpha (0.9f), 200, 150, true));
            g.fillPath (dome);
            g.setColour (juce::Colour (0xffb8d8f0).withAlpha (0.8f)); g.strokePath (dome, juce::PathStrokeType (3.0f));
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            juce::Path refl; refl.addCentredArc (120, 83, 90, 60, 0, -2.4f, -1.2f, true); g.strokePath (refl, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        for (auto* f : { &finT, &finB })
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff7ae6), 280, 56, juce::Colour (0xff8a2a9a), 400, 100, false)); g.fillPath (*f);
            g.setColour (juce::Colours::white.withAlpha (0.3f)); g.strokePath (*f, juce::PathStrokeType (1.2f));
        }
        juce::Path hull = hullPath();
        g.setColour (LNF::cyan.withAlpha (0.12f)); g.strokePath (hull, juce::PathStrokeType (10.0f));
        juce::ColourGradient hg (juce::Colour (0xff2a4068).withAlpha (0.92f), 0, 88, juce::Colour (0xff0a1224).withAlpha (0.94f), 0, 628, false);
        hg.addColour (0.12, juce::Colour (0xff3a5a88).withAlpha (0.92f));
        g.setGradientFill (hg); g.fillPath (hull);
        g.saveState(); g.reduceClipRegion (hull);
        // hull plates: thin lines and rivets
        auto seam = [&] (float x)
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRect (x - 1.5f, 88.0f, 3.0f, 540.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRect (x + 1.5f, 88.0f, 1.0f, 540.0f);
            for (float y = 104.0f; y < 620.0f; y += 22.0f)
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8ecff), x - 2, y - 2, juce::Colour (0xff2a4060), x + 2, y + 2, false));
                g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
            }
        };
        seam (429.0f); seam (667.0f);
        for (float y : { 95.0f, 620.0f })
            for (float x = 90.0f; x < 790.0f; x += 26.0f)
            { g.setColour (juce::Colour (0xffb8d8f0).withAlpha (0.55f)); g.fillEllipse (x - 2, y - 2, 4, 4); }
        g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillRect (14.0f, 100.0f, 832.0f, 30.0f);
        g.restoreState();
        g.setColour (juce::Colour (0xffb8d8f0).withAlpha (0.6f)); g.strokePath (hull, juce::PathStrokeType (1.5f));
    }
    // riveted rings around the windows
    auto ring = [&] (juce::Path shape, float thick)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.strokePath (shape, juce::PathStrokeType (thick + 4.0f), juce::AffineTransform::translation (0, 2));
        const auto b = shape.getBounds();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd0e8ff), b.getX(), b.getY(), juce::Colour (0xff23415f), b.getRight(), b.getBottom(), false));
        g.strokePath (shape, juce::PathStrokeType (thick));
        const float len = shape.getLength();
        for (float d = 0; d < len; d += 18.0f)
        {
            const auto p = shape.getPointAlongPath (d);
            g.setColour (juce::Colour (0xffeaf6ff)); g.fillEllipse (p.x - 1.8f, p.y - 1.8f, 3.6f, 3.6f);
        }
    };
    { juce::Path p; p.addEllipse (nebulaView.getBounds().toFloat().expanded (6.0f)); ring (p, 11.0f); }
    { juce::Path p; p.addRoundedRectangle (echoView.getBounds().toFloat().expanded (5.0f), 51.0f); ring (p, 9.0f); }
    { juce::Path p; p.addRoundedRectangle (xyPad.getBounds().toFloat().expanded (4.0f), 12.0f); ring (p, 7.0f); }
    // section plates
    auto plate = [&] (int x, int y, const char* title, juce::Colour accent)
    {
        juce::Rectangle<float> r ((float) x, (float) y, 84.0f, 22.0f);
        g.setColour (juce::Colour (0xff0a1426)); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (accent.withAlpha (0.7f)); g.drawRoundedRectangle (r, 5.0f, 1.2f);
        g.setColour (accent); g.setFont (LNF::font (13.0f)); g.drawText (title, r, juce::Justification::centred);
    };
    plate (34, 100, "ECHO", LNF::pink);
    plate (452, 100, "REVERB", LNF::cyan);
    plate (690, 100, "MASTER", LNF::lime);
    g.setColour (LNF::textDim);
    g.setFont (LNF::font (9.0f));
    g.drawText (TK_FR ("TIME →    ↑ FEEDBACK"), 30, 244, 140, 12, juce::Justification::centred);
    g.drawText ("IN", 732, 238, 22, 12, juce::Justification::centred);
    g.drawText ("OUT", 748, 238, 22, 12, juce::Justification::centred);
    // the name, in the cockpit
    g.setColour (juce::Colour (0xffd8f4ff));
    g.setFont (LNF::font (10.0f));
    g.drawText ("T C H I S K I B I D I", 40, 30, 160, 14, juce::Justification::centred);
    g.setColour (LNF::text);
    g.setFont (LNF::font (32.0f));
    g.drawText ("SPACE", 30, 42, 180, 38, juce::Justification::centred);
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
    // a long capsule window
    juce::Path win; win.addRoundedRectangle (r, r.getHeight() * 0.5f);
    g.reduceClipRegion (win);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff12284a), r.getCentreX(), r.getCentreY(), juce::Colour (0xff030812), r.getRight(), r.getBottom(), true));
    g.fillPath (win);
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
    // the big round porthole: the reverb seen through the window
    juce::Path win; win.addEllipse (r);
    g.reduceClipRegion (win);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff14305a), r.getCentreX(), r.getCentreY() + 10, juce::Colour (0xff050a18), r.getRight(), r.getBottom(), true));
    g.fillPath (win);
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
    g.drawText ("TAILLE " + juce::String (juce::roundToInt (sz * 100.0f)) + " %", 0, (int) r.getHeight() - 26, (int) r.getWidth(), 12, juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    juce::Path refl; refl.addCentredArc (c.x, c.y, r.getWidth() * 0.4f, r.getHeight() * 0.4f, 0.0f, -2.3f, -0.8f, true);
    g.strokePath (refl, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
