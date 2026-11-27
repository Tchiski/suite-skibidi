#include "PluginEditor.h"

using tchk::LNF;
using namespace twist;

namespace
{
    constexpr int kW = 880, kH = 650; const juce::Rectangle<int> tlArea { 124, 100, 632, 242 }; constexpr int DY = -42; constexpr int X0 = 20;
    // the whole plugin is a big cassette: body with the two little tabs at the top
    juce::Path cassetteShape()
    {
        juce::Path p;
        const float l = 4, t = 4, r = 876, b = 646, rad = 26;
        p.startNewSubPath (l + rad, t);
        // write-protect notches at the top
        for (float nx : { 70.0f, 780.0f }) { p.lineTo (nx, t); p.lineTo (nx, t + 9); p.lineTo (nx + 30, t + 9); p.lineTo (nx + 30, t); }
        p.lineTo (r - rad, t); p.quadraticTo (r, t, r, t + rad);
        p.lineTo (r, b - rad); p.quadraticTo (r, b, r - rad, b);
        p.lineTo (l + rad, b); p.quadraticTo (l, b, l, b - rad);
        p.lineTo (l, t + rad); p.quadraticTo (l, t, l + rad, t);
        p.closeSubPath();
        return p;
    }
    const juce::Point<float> reelL { 70.0f, 221.0f }, reelR { 810.0f, 221.0f };
}

TwistEditor::TwistEditor (TwistProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.9f, "twist"), audioProcessor (p)
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.overFn = [this] (juce::Graphics& g) { overContent (g); };
    content.layoutFn = [this] { layout(); };
    levelDb = [&p] { return p.outputPeakDb.load(); };

    for (int i = 0; i < NumFx; ++i)
    {
        auto* c = fxChips.add (new twist::Sticker (i));
        c->onClick = [this, i] { chosenFx = i; for (int k = 0; k < fxChips.size(); ++k) fxChips[k]->setToggleState (k == i, juce::dontSendNotification); };
        content.addAndMakeVisible (c);
    }
    fxChips[0]->setToggleState (true, juce::dontSendNotification);
    for (int b : { 8, 16, 32, 64 })
    {
        auto* c = zoomChips.add (new tchk::Chip (juce::String (b), coral)); c->fontSize = 11.0f;
        c->onClick = [this, b] { viewBars = b; for (auto* z : zoomChips) z->setToggleState (z->getButtonText() == juce::String (b), juce::dontSendNotification); scroll.setCurrentRange (viewStart, viewBars * 4.0); timeline.repaint(); };
        content.addAndMakeVisible (c);
    }
    zoomChips[1]->setToggleState (true, juce::dontSendNotification);
    const juce::StringArray dn { TK_FR ("Léger"), "Moyen", "Fou" };
    for (int i = 0; i < 3; ++i)
    {
        auto* c = densChips.add (new tchk::Chip (dn[i], coral)); c->fontSize = 11.5f;
        c->onClick = [this, i] { density = i; for (int k = 0; k < 3; ++k) densChips[k]->setToggleState (k == i, juce::dontSendNotification); };
        content.addAndMakeVisible (c);
    }
    densChips[1]->setToggleState (true, juce::dontSendNotification);

    scroll.setRangeLimits (0.0, 128 * 4.0);
    scroll.setCurrentRange (0.0, viewBars * 4.0);
    scroll.setAutoHide (false);
    scroll.addListener (this);
    scroll.setColour (juce::ScrollBar::thumbColourId, coral.withAlpha (0.6f));

    for (auto* s : { &amount, &curve, &mix })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        s->setColour (juce::Slider::rotarySliderFillColourId, coral);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->getProperties().set ("tk", "orbit");
        s->setRange (0.0, 1.0);
        s->textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100)) + " %"; };
        s->updateText();
        s->onValueChange = [this, s]
        {
            if (selected < 0 || selected >= (int) zones.size()) return;
            auto& z = zones[(size_t) selected];
            (s == &amount ? z.amount : s == &curve ? z.curve : z.mix) = (float) s->getValue();
            commit();
        };
        content.addAndMakeVisible (s);
    }
    delB.onClick = [this] { if (selected >= 0 && selected < (int) zones.size()) { zones.erase (zones.begin() + selected); select (-1); commit(); } };
    sprinkleB.outline = true;
    sprinkleB.onClick = [this] { undoZ.push_back (twist::toString (audioProcessor.getZones())); redoZ.clear(); audioProcessor.sprinkle (density, juce::jmax (8, viewBars)); refreshFromState(); };
    magnetB.setToggleState (true, juce::dontSendNotification); magnetB.fontSize = 11.0f;
    magnetB.onClick = [this] { magnet = ! magnet; magnetB.setToggleState (magnet, juce::dontSendNotification); magnetB.setButtonText (magnet ? "Aimant : sur les coups" : "Aimant : sur la grille"); };
    followB.fontSize = 11.0f;
    followB.setToggleState (true, juce::dontSendNotification);
    followB.onClick = [this] { followB.setToggleState (! followB.getToggleState(), juce::dontSendNotification); };
    badge.isPlaying = [this] { return audioProcessor.playing.load(); };
    presets = std::make_unique<tchk::PresetSystem> (p, "Twist", presetBox, heartButton, nullptr, nullptr, TK_FR ("♥ MES TWISTS"), "Mes twists");

    for (juce::Component* c : std::initializer_list<juce::Component*> { &timeline, &scroll, &delB, &sprinkleB, &followB, &magnetB, &badge, &presetBox, &heartButton })
        content.addAndMakeVisible (c);
    layout();
    addInfo (66, 490, TK_FR ("Choisis un effet, puis glisse sur la timeline.||25 effets fous en 4 familles : TEMPS (tape stop, bullet time, disque rayé...), HACHAGE (rafale, hoquet, bug 8-bit...), VOIX FOLLES (hélium, démon, ascenseur...), ESPACE FOU (toupie, trou noir, fantôme...).|AIMANT : les petits triangles sur la timeline sont les coups du morceau (kick, snare...), repérés pendant la lecture. Les zones se collent dessus.|Mets Twist sur ta piste instru (ou le master). Tu poses des effets à des moments précis : un tape stop avant le refrain, une coupure sur le dernier temps, un filtre qui monte...|POSER : clique un effet, puis glisse sur la timeline à l'endroit voulu.|MODIFIER : clique une zone, règle-la en bas, tire son bord droit pour l'allonger, glisse-la pour la déplacer.|EFFACER : sélectionne la zone et clique Effacer (ou Suppr).|Tout est calé sur les mesures d'Ableton : chaque effet se déclenche pile au bon moment, à chaque lecture et dans l'export (bounce)."), "Twist");
    addInfo (332, 64, TK_FR ("Twist place des effets tout seul.||Il pose de petits effets aux bons endroits : fins de phrases de 4 et 8 mesures, avant les changements de partie (tape stop, coupure, filtre, stutter...). Reclique pour d'autres idées.|L'image du morceau (la forme d'onde) se dessine pendant que tu le joues."), "Saupoudrer");
    windowShape = [] { return cassetteShape(); };
    gearBounds = { 818, 14, 26, 26 };
    finishSetup (71, {});
    refreshFromState();
    select (-1);
    startTimerHz (30);
}

TwistEditor::~TwistEditor() { stopTimer(); scroll.removeListener (this); }

void TwistEditor::layout()
{
    // bottom: the sheet of stickers, one block per family (5 columns for TEMPS, 3 for the others)
    int fx = 22;
    for (size_t f = 0; f < families().size(); ++f)
    {
        const int first = families()[f].first, last = f + 1 < families().size() ? families()[f + 1].first : NumFx;
        const int cols = f == 0 ? 5 : 3;
        for (int i = first; i < last; ++i)
        {
            const int k = i - first;
            fxChips[i]->setBounds (fx + (k % cols) * 58, 508 + (k / cols) * 70, 58, 68);
        }
        fx += cols * 58 + 8;
    }
    sprinkleB.setBounds (20, 58, 130, 32);
    for (int i = 0; i < 3; ++i) densChips[i]->setBounds (156 + i * 58, 60, 54, 28);
    for (int i = 0; i < zoomChips.size(); ++i) zoomChips[i]->setBounds (420 + i * 40, 62, 36, 24);
    followB.setBounds (584, 62, 120, 24);
    magnetB.setBounds (712, 60, 148, 28);
    timeline.setBounds (tlArea);
    scroll.setBounds (tlArea.getX() + 8, tlArea.getBottom() + 3, tlArea.getWidth() - 16, 9);
    amount.setBounds (X0 + 320, 424 + DY, 76, 80); curve.setBounds (X0 + 396, 424 + DY, 76, 80); mix.setBounds (X0 + 472, 424 + DY, 76, 80);
    delB.setBounds (X0 + 12, 502 + DY, 80, 24);
    badge.setBounds (600, 410 + DY, 260, 40);
    heartButton.setBounds (600, 462 + DY, 26, 26);
    presetBox.setBounds (632, 462 + DY, 228, 26);
}

void TwistEditor::paintContent (juce::Graphics& g)
{
    namespace sh = tchk::shapes;
    // the cassette: dark plastic body, a striped label, the tape window, screws
    {
        const auto body = cassetteShape();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c1e48), 0, 0, juce::Colour (0xff120a20), 0, 650, false)); g.fillPath (body);
        // the label (everything at the top lives on it)
        const juce::Rectangle<float> label (14, 14, 852, 346);
        g.setColour (juce::Colour (0xff3a2460)); g.fillRoundedRectangle (label, 14);
        const juce::Colour stripes[] = { coral, juce::Colour (0xffffb84d), juce::Colour (0xffff4fd8) };
        for (int i = 0; i < 3; ++i) { g.setColour (stripes[i]); g.fillRect (label.getX() + 140.0f, label.getY() + 12.0f + 7.0f * (float) i, label.getWidth() - 260.0f, 4.0f); }
        // the clear window of the tape, with the two reels
        const juce::Rectangle<float> win (22, 92, 836, 258);
        g.setColour (juce::Colour (0xff0c0716)); g.fillRoundedRectangle (win, 40);
        g.setColour (juce::Colours::white.withAlpha (0.18f)); g.drawRoundedRectangle (win, 40, 2.0f);
        // the lower part of a cassette (where the tape runs out), behind the stickers
        juce::Path trap; trap.startNewSubPath (150, 486); trap.lineTo (730, 486); trap.lineTo (770, 646); trap.lineTo (110, 646); trap.closeSubPath();
        g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillPath (trap);
        // screws
        for (auto pt : { juce::Point<float> (22, 628), juce::Point<float> (858, 628), juce::Point<float> (440, 628) })
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8dcff), pt.x - 4, pt.y - 4, juce::Colour (0xff4a3a68), pt.x + 5, pt.y + 5, false));
            g.fillEllipse (pt.x - 7, pt.y - 7, 14, 14);
            g.setColour (juce::Colour (0xff2a1d40)); g.drawLine (pt.x - 4, pt.y - 4, pt.x + 4, pt.y + 4, 2.0f);
        }
        g.setColour (juce::Colours::white.withAlpha (0.12f)); g.strokePath (body, juce::PathStrokeType (2.0f));
    }
    // title: a sticker of its own
    {
        g.saveState(); g.addTransform (juce::AffineTransform::rotation (-0.05f, 70, 34));
        juce::Rectangle<float> tr (18, 14, 110, 38);
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (tr.translated (4, 4), 7);
        g.setColour (juce::Colours::white); g.fillRoundedRectangle (tr.expanded (3), 9);
        g.setColour (coral); g.fillRoundedRectangle (tr, 7);
        g.setColour (juce::Colour (0xff1a0d2e)); g.setFont (LNF::font (28.0f)); g.drawText ("TWIST", tr, juce::Justification::centred);
        g.restoreState();
    }
    // the sheet of stickers (a torn piece of paper)
    {
        const juce::Rectangle<float> sheet (12, 486, 856, 160);
        const auto p = sh::tornPath (sheet, 9, 5.0f, false, true);
        g.setColour (juce::Colour (0xff2a1748).withAlpha (0.92f)); g.fillPath (p);
        g.setColour (juce::Colours::white.withAlpha (0.10f)); g.strokePath (p, juce::PathStrokeType (1.2f));
        // little dotted lines between the families
        int fx2 = 22;
        for (size_t f = 0; f < families().size(); ++f)
        {
            const int cols = f == 0 ? 5 : 3;
            g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (LNF::font (10.0f));
            g.drawText (families()[f].second, fx2 + 4, 492, 160, 14, juce::Justification::centredLeft);
            fx2 += cols * 58 + 8;
            if (f + 1 < families().size())
            {
                juce::Path d; d.startNewSubPath ((float) fx2 - 6.0f, 496); d.lineTo ((float) fx2 - 6.0f, 640);
                float dashes[] = { 3, 4 }; juce::Path dd; juce::PathStrokeType (1.0f).createDashedStroke (dd, d, dashes, 2);
                g.setColour (juce::Colours::white.withAlpha (0.18f)); g.fillPath (dd);
            }
        }
    }
    g.setColour (LNF::textDim); g.setFont (LNF::font (10.0f));
    g.drawText ("MESURES", 340, 66, 76, 16, juce::Justification::centredRight);
    // the edit card: a piece of tape-sealed paper
    {
        const juce::Rectangle<float> card ((float) X0, 404.0f + DY, 568.0f, 128.0f);
        const auto p = sh::tornPath (card, 21, 4.0f, true, false);
        g.setColour (juce::Colour (0xff20123a).withAlpha (0.92f)); g.fillPath (p);
        g.setColour (coral.withAlpha (0.45f)); g.strokePath (p, juce::PathStrokeType (1.2f));
        for (int k = 0; k < 2; ++k)   // two strips of tape holding it
        {
            g.saveState();
            const float x = k == 0 ? card.getX() + 30.0f : card.getRight() - 70.0f;
            g.addTransform (juce::AffineTransform::rotation (k == 0 ? -0.35f : 0.3f, x + 20, card.getY()));
            g.setColour (juce::Colour (0xfffff2c8).withAlpha (0.55f));
            g.fillPath (sh::tornPath ({ x, card.getY() - 8.0f, 46.0f, 16.0f }, 40 + k, 2.5f, true, false));
            g.restoreState();
        }
    }
    g.setColour (LNF::textDim); g.setFont (LNF::font (10.0f));
    g.drawText (TK_FR ("INTENSITÉ"), X0 + 320, 412 + DY, 76, 14, juce::Justification::centred);
    g.drawText ("COURBE", X0 + 396, 412 + DY, 76, 14, juce::Justification::centred);
    g.drawText ("MIX", X0 + 472, 412 + DY, 76, 14, juce::Justification::centred);
}

void TwistEditor::overContent (juce::Graphics& g)
{
    // the two reels turn while the song plays (the left one full of tape, the right one almost empty)
    {
        const double pb = audioProcessor.playBeat.load();
        const float a = pb >= 0 ? (float) pb * 1.3f : 0.0f;
        auto reel = [&] (juce::Point<float> c, float tape, float rot)
        {
            g.setColour (juce::Colour (0xff3a2418)); g.fillEllipse (c.x - tape, c.y - tape, tape * 2, tape * 2);
            g.setColour (juce::Colour (0xff5a3a24)); g.drawEllipse (c.x - tape + 3, c.y - tape + 3, tape * 2 - 6, tape * 2 - 6, 1.0f);
            const float hub = 22.0f;
            g.setColour (juce::Colour (0xfff0e8ff)); g.fillEllipse (c.x - hub, c.y - hub, hub * 2, hub * 2);
            g.setColour (juce::Colour (0xff2a1d40)); g.fillEllipse (c.x - 9, c.y - 9, 18, 18);
            for (int k = 0; k < 6; ++k)
            {
                const float ang = rot + (float) k * juce::MathConstants<float>::pi / 3.0f;
                juce::Path tooth; tooth.addRectangle (-2.5f, -14.0f, 5.0f, 6.0f);
                g.setColour (juce::Colour (0xfff0e8ff)); g.fillPath (tooth, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
                juce::Path hole; hole.addEllipse (-4.0f, -19.0f, 8.0f, 8.0f);
                g.setColour (juce::Colour (0xff3a2460)); g.fillPath (hole, juce::AffineTransform::rotation (ang + 0.5f).translated (c.x, c.y));
            }
        };
        reel (reelL, 46.0f, a);
        reel (reelR, 30.0f, a * 1.4f);
        // the tape going from one reel to the other along the bottom of the window
        g.setColour (juce::Colour (0xff5a3a24));
        g.drawLine (reelL.x + 10, reelL.y + 46, reelR.x - 10, reelR.y + 30, 2.0f);
    }
    const juce::Rectangle<float> badgeR ((float) X0 + 14, 418 + DY, 56, 56);
    if (selected < 0 || selected >= (int) zones.size())
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillEllipse (badgeR);
        g.setColour (LNF::textDim); g.setFont (LNF::font (24.0f)); g.drawText ("?", badgeR, juce::Justification::centred);
        g.setFont (LNF::font (15.0f));
        g.drawText (TK_FR ("Aucune zone choisie"), X0 + 84, 416 + DY, 230, 22, juce::Justification::centredLeft);
        g.setFont (LNF::font (11.5f, false));
        g.drawFittedText (TK_FR ("Clique une zone sur la timeline pour la régler, ou glisse pour en poser une nouvelle."), X0 + 84, 440 + DY, 230, 50, juce::Justification::topLeft, 3);
        return;
    }
    const auto& z = zones[(size_t) selected];
    const auto c = fxColour (z.fx);
    {
        const auto shp = twist::stickerShape (z.fx, badgeR.reduced (4.0f));
        g.setColour (juce::Colours::white); g.strokePath (shp, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c); g.fillPath (shp);
        twist::drawFxGlyph (g, z.fx, badgeR.reduced (16.0f), juce::Colour (0xff1a0d2e));
    }
    g.setColour (c); g.setFont (LNF::font (16.0f));
    g.drawText (fxNames()[z.fx], X0 + 84, 414 + DY, 230, 22, juce::Justification::centredLeft);
    const int bar = (int) (z.start / 4.0) + 1; const double bt = std::fmod (z.start, 4.0) + 1.0;
    g.setColour (LNF::textDim); g.setFont (LNF::font (10.5f));
    g.drawText (TK_FR ("mesure ") + juce::String (bar) + ", temps " + juce::String (bt, bt == std::floor (bt) ? 0 : 2) + " · " + juce::String (z.end - z.start, 2) + " temps", X0 + 84, 436 + DY, 230, 16, juce::Justification::centredLeft);
    g.setFont (LNF::font (11.5f, false));
    g.drawFittedText (fxHelp()[z.fx], X0 + 84, 454 + DY, 230, 48, juce::Justification::topLeft, 3);
}

void TwistEditor::refreshFromState()
{
    zones = audioProcessor.getZones();
    if (selected >= (int) zones.size()) selected = -1;
    lastVersion = audioProcessor.stateVersion.load();
    select (selected);
}

void TwistEditor::select (int i)
{
    selected = i;
    const bool on = i >= 0 && i < (int) zones.size();
    for (auto* s : { &amount, &curve, &mix }) s->setEnabled (on);
    delB.setEnabled (on);
    if (on)
    {
        amount.setValue (zones[(size_t) i].amount, juce::dontSendNotification);
        curve.setValue (zones[(size_t) i].curve, juce::dontSendNotification);
        mix.setValue (zones[(size_t) i].mix, juce::dontSendNotification);
    }
    timeline.repaint();
    content.repaint();
}

void TwistEditor::undoRedo (bool redo)
{
    auto& from = redo ? redoZ : undoZ;
    auto& to = redo ? undoZ : redoZ;
    if (from.empty()) return;
    to.push_back (twist::toString (audioProcessor.getZones()));
    zones = twist::fromString (from.back()); from.pop_back();
    select (-1);
    audioProcessor.setZones (zones);
    timeline.repaint();
    content.repaint();
}

void TwistEditor::commit()
{
    const auto before = twist::toString (audioProcessor.getZones()), now = twist::toString (zones);
    if (before != now) { undoZ.push_back (before); if (undoZ.size() > 60) undoZ.erase (undoZ.begin()); redoZ.clear(); }
    audioProcessor.setZones (zones);
    timeline.repaint();
    content.repaint();
}

void TwistEditor::scrollBarMoved (juce::ScrollBar*, double start) { viewStart = start; timeline.repaint(); }

void TwistEditor::timerCallback()
{
    if (audioProcessor.stateVersion.load() != lastVersion) refreshFromState();
    const double pb = audioProcessor.playBeat.load();
    if (pb >= 0 && followB.getToggleState() && (pb < viewStart || pb > viewStart + viewBars * 4.0))
    {
        viewStart = std::floor (pb / (viewBars * 4.0)) * viewBars * 4.0;
        scroll.setCurrentRange (viewStart, viewBars * 4.0, juce::dontSendNotification);
    }
    timeline.repaint();
    badge.repaint();
}

//==============================================================================
double TwistEditor::Timeline::beatAt (float x) const
{
    const double b = ed.viewStart + (double) (x - 8.0f) / (double) (getWidth() - 16) * ed.viewBars * 4.0;
    const double grid = juce::jmax (0.0, std::round (b * 4.0) / 4.0);   // a 16th
    if (! ed.magnet) return grid;
    const int c = (int) std::round (b * 4.0);
    for (int d = 0; d <= 2; ++d)   // nearest hit within half a beat
        for (int sgn : { -1, 1 })
        {
            const int k = c + sgn * d;
            if (k >= 0 && k < TwistProcessor::thumbLen && ed.audioProcessor.hits[(size_t) k].load()) return k / 4.0;
        }
    return grid;
}
float TwistEditor::Timeline::xOf (double beat) const { return 8.0f + (float) ((beat - ed.viewStart) / (ed.viewBars * 4.0)) * (float) (getWidth() - 16); }
juce::Rectangle<float> TwistEditor::Timeline::zoneRect (const Zone& z) const
{
    const float x0 = xOf (z.start), x1 = xOf (z.end);
    return { x0, (float) getHeight() - 52.0f, juce::jmax (6.0f, x1 - x0), 40.0f };
}

void TwistEditor::Timeline::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // the song is a long strip of tape, torn at both ends
    const auto tape = tchk::shapes::tornPath (r.reduced (1.0f), 5, 7.0f, true, false);
    g.setColour (juce::Colour (0xff1a0e30).withAlpha (0.94f)); g.fillPath (tape);
    g.setColour (juce::Colours::white.withAlpha (0.12f)); g.strokePath (tape, juce::PathStrokeType (1.2f));
    g.saveState(); g.reduceClipRegion (tape);
    const int beats = ed.viewBars * 4;
    const float h = (float) getHeight();
    // grid + bar numbers
    for (int b = 0; b <= beats; ++b)
    {
        const double beat = ed.viewStart + b;
        const bool bar = std::fmod (beat, 4.0) < 0.001;
        if (! bar && ed.viewBars > 32) continue;
        g.setColour (juce::Colours::white.withAlpha (bar ? 0.22f : 0.06f));
        g.drawVerticalLine ((int) xOf (beat), 22.0f, h);
        if (bar && (ed.viewBars <= 32 || std::fmod (beat / 4.0, 4.0) < 0.001))
        {
            g.setColour (ed.coral); g.setFont (LNF::font (10.0f));
            g.drawText ("M" + juce::String ((int) (beat / 4.0) + 1), (int) xOf (beat) + 4, 6, 40, 14, juce::Justification::centredLeft);
        }
    }
    // the song's picture, built while it plays
    const float mid = 26.0f + (h - 90.0f) * 0.5f, amp = (h - 96.0f) * 0.5f;
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    for (int q = 0; q < beats * 4; ++q)
    {
        const int idx = (int) (ed.viewStart * 4.0) + q;
        if (idx < 0 || idx >= TwistProcessor::thumbLen) break;
        const float v = juce::jmin (1.0f, ed.audioProcessor.thumb[(size_t) idx].load() * 1.4f);
        if (v <= 0.001f) continue;
        const float x = xOf (ed.viewStart + q * 0.25);
        g.fillRect (x, mid - v * amp, juce::jmax (1.0f, (float) (getWidth() - 16) / (float) (beats * 4) - 1.0f), v * amp * 2.0f);
    }
    // the hits (transients) found while playing: little triangles
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    for (int q = 0; q < beats * 4; ++q)
    {
        const int idx = (int) (ed.viewStart * 4.0) + q;
        if (idx < 0 || idx >= TwistProcessor::thumbLen) break;
        if (! ed.audioProcessor.hits[(size_t) idx].load()) continue;
        const float x = xOf (ed.viewStart + q * 0.25);
        juce::Path tri; tri.addTriangle (x - 4.0f, 24.0f, x + 4.0f, 24.0f, x, 30.0f); g.fillPath (tri);
    }
    if (ed.audioProcessor.thumb[(size_t) juce::jlimit (0, TwistProcessor::thumbLen - 1, (int) (ed.viewStart * 4.0) + 2)].load() <= 0.0f)
    {
        g.setColour (LNF::textDim.withAlpha (0.6f)); g.setFont (LNF::font (11.0f, false));
        g.drawText (TK_FR ("Lance la lecture : l'image du morceau se dessine ici"), getLocalBounds().withHeight ((int) (h - 60)), juce::Justification::centred);
    }
    // zones
    for (int i = 0; i < (int) ed.zones.size(); ++i)
    {
        const auto& z = ed.zones[(size_t) i];
        auto zr = zoneRect (z);
        if (zr.getRight() < 0 || zr.getX() > (float) getWidth()) continue;
        const auto c = fxColour (z.fx);
        g.setColour (c.withAlpha (0.10f)); g.fillRect (zr.getX(), 24.0f, zr.getWidth(), zr.getY() - 26.0f);
        // each zone: a piece of coloured tape, torn at the top and the bottom, with its sticker
        const auto piece = tchk::shapes::tornPath (zr, i * 7 + z.fx, 3.0f, false, true);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillPath (piece, juce::AffineTransform::translation (2, 3));
        g.setColour (c.withAlpha (i == ed.selected ? 0.98f : 0.8f)); g.fillPath (piece);
        g.setColour (juce::Colours::white.withAlpha (0.18f)); g.fillRect (zr.withHeight (zr.getHeight() * 0.3f).reduced (0, 2).translated (0, 3));
        if (i == ed.selected) { g.setColour (juce::Colours::white); g.strokePath (piece, juce::PathStrokeType (2.0f)); }
        if (i == ed.audioProcessor.activeZone.load()) { g.setColour (juce::Colours::white.withAlpha (0.35f)); g.fillPath (piece); }
        const bool room = zr.getWidth() > 30.0f;
        if (room) twist::drawFxGlyph (g, z.fx, juce::Rectangle<float> (zr.getX() + 4, zr.getY() + 9, 22, 22), juce::Colour (0xff1a0d2e));
        g.setColour (juce::Colour (0xff14081f)); g.setFont (LNF::font (10.5f));
        g.drawFittedText (fxNames()[z.fx], zr.reduced (5, 2).withTrimmedLeft (room ? 24 : 0).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    }
    g.setColour (LNF::textDim.withAlpha (0.5f)); g.setFont (LNF::font (9.5f, false));
    g.drawText (TK_FR ("glisse ici pour poser l'effet choisi"), getWidth() - 230, getHeight() - 10, 222, 10, juce::Justification::centredRight);
    const double pb = ed.audioProcessor.playBeat.load();
    if (pb >= ed.viewStart && pb <= ed.viewStart + beats) { g.setColour (juce::Colours::white); g.fillRect (xOf (pb) - 1.0f, 22.0f, 2.0f, h - 22.0f); }
    g.restoreState();
}

void TwistEditor::Timeline::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    for (int i = (int) ed.zones.size(); --i >= 0;)
    {
        auto zr = zoneRect (ed.zones[(size_t) i]);
        if (zr.expanded (2, 0).contains (e.position))
        {
            ed.select (i);
            if (e.mods.isPopupMenu()) { ed.zones.erase (ed.zones.begin() + i); ed.select (-1); ed.commit(); dragMode = 0; return; }
            dragMode = e.position.x > zr.getRight() - 8.0f ? 1 : 2;
            dragOff = beatAt (e.position.x) - ed.zones[(size_t) i].start; dragLen = ed.zones[(size_t) i].end - ed.zones[(size_t) i].start;
            return;
        }
    }
    // new zone with the chosen effect
    Zone z; z.fx = ed.chosenFx; z.start = beatAt (e.position.x); z.end = z.start + 0.5;
    ed.zones.push_back (z);
    ed.select ((int) ed.zones.size() - 1);
    dragMode = 1;
}
void TwistEditor::Timeline::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == 0 || ed.selected < 0 || ed.selected >= (int) ed.zones.size()) return;
    auto& z = ed.zones[(size_t) ed.selected];
    const double b = beatAt (e.position.x);
    if (dragMode == 1) z.end = juce::jmax (z.start + 0.25, b);
    else { z.start = juce::jmax (0.0, b - dragOff); z.end = z.start + dragLen; }
    repaint(); ed.content.repaint();
}
void TwistEditor::Timeline::mouseUp (const juce::MouseEvent&)
{
    if (dragMode != 0)
    {
        // keep the zones in time order, the selection follows its zone
        const auto sel = ed.selected >= 0 ? ed.zones[(size_t) ed.selected] : Zone();
        std::stable_sort (ed.zones.begin(), ed.zones.end(), [] (const Zone& a, const Zone& b) { return a.start < b.start; });
        for (int i = 0; i < (int) ed.zones.size(); ++i) if (ed.zones[(size_t) i].start == sel.start && ed.zones[(size_t) i].fx == sel.fx) ed.selected = i;
        ed.commit();
    }
    dragMode = 0;
}
bool TwistEditor::Timeline::keyPressed (const juce::KeyPress& k)
{
    if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && ed.selected >= 0)
    {
        ed.zones.erase (ed.zones.begin() + ed.selected); ed.select (-1); ed.commit(); return true;
    }
    const auto mods = k.getModifiers();
    const int code = k.getKeyCode();
    if (mods.isCommandDown() && (code == 'Z' || code == 'z')) { ed.undoRedo (mods.isShiftDown()); return true; }
    if (mods.isCommandDown() && (code == 'Y' || code == 'y')) { ed.undoRedo (true); return true; }
    if (! mods.isCommandDown()) giveAwayKeyboardFocus();   // other keys go back to the DAW
    return false;
}
