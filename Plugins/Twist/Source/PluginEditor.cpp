#include "PluginEditor.h"

using tchk::LNF;
using namespace twist;

namespace { constexpr int kW = 880, kH = 600; const juce::Rectangle<int> tlArea { 20, 152, 840, 220 }; }

TwistEditor::TwistEditor (TwistProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.9f, "twist"), audioProcessor (p)
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.overFn = [this] (juce::Graphics& g) { overContent (g); };
    content.layoutFn = [this] { layout(); };
    levelDb = [&p] { return p.outputPeakDb.load(); };

    for (int i = 0; i < NumFx; ++i)
    {
        auto* c = fxChips.add (new tchk::Chip (fxNames()[i], fxColour (i))); c->fontSize = 12.0f;
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
    sprinkleB.onClick = [this] { audioProcessor.sprinkle (density, juce::jmax (8, viewBars)); refreshFromState(); };
    followB.setToggleState (true, juce::dontSendNotification);
    followB.onClick = [this] { followB.setToggleState (! followB.getToggleState(), juce::dontSendNotification); };
    badge.isPlaying = [this] { return audioProcessor.playing.load(); };
    presets = std::make_unique<tchk::PresetSystem> (p, "Twist", presetBox, heartButton, nullptr, nullptr, TK_FR ("♥ MES TWISTS"), "Mes twists");

    for (juce::Component* c : std::initializer_list<juce::Component*> { &timeline, &scroll, &delB, &sprinkleB, &followB, &badge, &presetBox, &heartButton })
        content.addAndMakeVisible (c);
    layout();
    addInfo (96, 62, TK_FR ("Choisis un effet, puis glisse sur la timeline.||Mets Twist sur ta piste instru (ou le master). Tu poses des effets à des moments précis : un tape stop avant le refrain, une coupure sur le dernier temps, un filtre qui monte...|POSER : clique un effet, puis glisse sur la timeline à l'endroit voulu.|MODIFIER : clique une zone, règle-la en bas, tire son bord droit pour l'allonger, glisse-la pour la déplacer.|EFFACER : sélectionne la zone et clique Effacer (ou Suppr).|Tout est calé sur les mesures d'Ableton : chaque effet se déclenche pile au bon moment, à chaque lecture et dans l'export (bounce)."), "Twist");
    addInfo (230, 532, TK_FR ("Twist place des effets tout seul.||Il pose de petits effets aux bons endroits : fins de phrases de 4 et 8 mesures, avant les changements de partie (tape stop, coupure, filtre, stutter...). Reclique pour d'autres idées.|L'image du morceau (la forme d'onde) se dessine pendant que tu le joues."), "Saupoudrer");
    finishSetup (71, { { 260, 14, 40 }, { 340, 30, 28 }, { 560, 12, 34 } });
    refreshFromState();
    select (-1);
    startTimerHz (30);
}

TwistEditor::~TwistEditor() { stopTimer(); scroll.removeListener (this); }

void TwistEditor::layout()
{
    int x = 20, y = 84;
    for (int i = 0; i < fxChips.size(); ++i)
    {
        const int w = 30 + (int) (7.2f * (float) fxChips[i]->getButtonText().length());
        if (x + w > 860) { x = 20; y += 34; }
        fxChips[i]->setBounds (x, y, w, 30); x += w + 6;
    }
    timeline.setBounds (tlArea);
    scroll.setBounds (tlArea.getX() + 8, tlArea.getBottom() + 4, tlArea.getWidth() - 16, 10);
    for (int i = 0; i < zoomChips.size(); ++i) zoomChips[i]->setBounds (610 + i * 44, 58, 40, 24);
    followB.setBounds (440, 58, 150, 24);
    amount.setBounds (430, 410, 90, 90); curve.setBounds (530, 410, 90, 90); mix.setBounds (630, 410, 90, 90);
    delB.setBounds (740, 440, 100, 30);
    badge.setBounds (20, 524, 190, 38);
    sprinkleB.setBounds (256, 526, 130, 34);
    for (int i = 0; i < 3; ++i) densChips[i]->setBounds (392 + i * 66, 528, 62, 30);
    heartButton.setBounds (630, 530, 26, 26);
    presetBox.setBounds (662, 530, 198, 26);
}

void TwistEditor::paintContent (juce::Graphics& g)
{
    paintBackgroundAndHeader (g, "Twist", {});
    g.setColour (LNF::textDim); g.setFont (LNF::font (12.0f));
    g.drawText ("EFFETS", 20, 62, 80, 18, juce::Justification::centredLeft);
    g.setFont (LNF::font (10.5f));
    g.drawText ("MESURES", 610 - 64, 61, 60, 18, juce::Justification::centredRight);
    drawPanelT (g, { 20, 392, 840, 122 }, coral);
    g.setColour (LNF::textDim); g.setFont (LNF::font (10.5f));
    g.drawText (TK_FR ("INTENSITÉ"), 430, 398, 90, 14, juce::Justification::centred);
    g.drawText ("COURBE", 530, 398, 90, 14, juce::Justification::centred);
    g.drawText ("MIX", 630, 398, 90, 14, juce::Justification::centred);
}

void TwistEditor::overContent (juce::Graphics& g)
{
    if (selected < 0 || selected >= (int) zones.size())
    {
        g.setColour (LNF::textDim); g.setFont (LNF::font (15.0f));
        g.drawText (TK_FR ("Aucune zone choisie"), 38, 404, 360, 22, juce::Justification::centredLeft);
        g.setFont (LNF::font (11.5f, false));
        g.drawFittedText (TK_FR ("Clique une zone sur la timeline pour la régler, ou glisse pour en poser une nouvelle."), 38, 430, 360, 40, juce::Justification::topLeft, 3);
        return;
    }
    const auto& z = zones[(size_t) selected];
    g.setColour (fxColour (z.fx)); g.setFont (LNF::font (15.0f));
    const int bar = (int) (z.start / 4.0) + 1; const double bt = std::fmod (z.start, 4.0) + 1.0;
    g.drawText (fxNames()[z.fx] + TK_FR (" · mesure ") + juce::String (bar) + ", temps " + juce::String (bt, bt == std::floor (bt) ? 0 : 1), 38, 404, 380, 22, juce::Justification::centredLeft);
    g.setColour (LNF::textDim); g.setFont (LNF::font (11.5f, false));
    g.drawFittedText (fxHelp()[z.fx] + "\n" + TK_FR ("COURBE : effet progressif (à gauche) ou qui arrive d'un coup à la fin (à droite)."), 38, 430, 370, 60, juce::Justification::topLeft, 4);
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

void TwistEditor::commit()
{
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
    return juce::jmax (0.0, std::round (b * 4.0) / 4.0);   // snap to a 16th
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
    g.setColour (juce::Colours::black.withAlpha (0.42f)); g.fillRoundedRectangle (r, 16.0f);
    g.setColour (ed.coral.withAlpha (0.35f)); g.drawRoundedRectangle (r.reduced (0.75f), 16.0f, 1.5f);
    g.saveState(); g.reduceClipRegion (getLocalBounds().reduced (2));
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
        g.setColour (c.withAlpha (i == ed.selected ? 0.95f : 0.72f)); g.fillRoundedRectangle (zr, 8.0f);
        if (i == ed.selected) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (zr, 8.0f, 2.0f); }
        if (i == ed.audioProcessor.activeZone.load()) { g.setColour (juce::Colours::white.withAlpha (0.35f)); g.fillRoundedRectangle (zr, 8.0f); }
        g.setColour (juce::Colour (0xff14081f)); g.setFont (LNF::font (10.5f));
        g.drawFittedText (fxNames()[z.fx], zr.reduced (5, 2).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
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
        ed.zones.erase (ed.zones.begin() + ed.selected); ed.select (-1); ed.commit(); giveAwayKeyboardFocus(); return true;
    }
    return false;
}
