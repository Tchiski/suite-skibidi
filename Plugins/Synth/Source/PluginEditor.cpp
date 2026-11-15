#include "PluginEditor.h"

using tchk::LNF;
namespace snd = tchk::sounds;

namespace
{
    constexpr int kW = 880, kH = 652;
    // the 12 settings, in display order
    const char* ids[] = { "bright", "space", "motion", "attack", "release", "width", "warmth", "vibrato", "glide", "filter", "reso", "detune" };
    // random ranges that always sound good
    const float lo[] = { 0.1f, 0.1f, 0.1f, 0.1f, 0.05f, 0.3f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f };
    const float hi[] = { 0.9f, 0.9f, 0.9f, 0.9f, 0.8f, 1.0f, 0.7f, 0.4f, 0.35f, 1.0f, 0.6f, 0.6f };
}

SynthEditor::SynthEditor (SynthProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.9f, "synth"),
      audioProcessor (p),
      volume (p.apvts, "volume", "Volume", orange, true)
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.overFn = [this] (juce::Graphics& g) { overContent (g); };
    content.layoutFn = [this] { layout(); };
    levelDb = [&p] { return p.peakDb.load(); };

    for (int c = 0; c < snd::categories().size(); ++c)
    {
        auto* b = catChips.add (new tchk::Chip (snd::categories()[c], orange));
        b->onClick = [this, c] { audioProcessor.selectPreset (snd::join (c, 0)); refreshPresetUI(); demo(); };
        content.addAndMakeVisible (b);
    }
    auto step = [this] (int d)
    {
        const auto [c, i] = snd::split (audioProcessor.presetIndex());
        const int n = snd::names (c).size();
        audioProcessor.selectPreset (snd::join (c, (i + d + n) % n)); refreshPresetUI(); demo();
    };
    prevB.onClick = [step] { step (-1); };
    nextB.onClick = [step] { step (1); };
    orb.accent = orange; orb.accent2 = juce::Colour (0xffffe2cc);
    orb.onClick = [this] { openList(); };

    const juce::StringArray names { "Brillance", "Espace", "Mouvement", "Attaque", TK_FR ("Relâchement"), "Largeur", "Chaleur", "Vibrato", "Glide", "Filtre", TK_FR ("Résonance"), TK_FR ("Détune") };
    for (int i = 0; i < 12; ++i) { auto* k = knobs.add (new tchk::Knob (p.apvts, ids[i], names[i], orange, true)); k->asOrbit(); content.addAndMakeVisible (k); }
    volume.asRocket();

    for (int o = -2; o <= 2; ++o)
    {
        auto* c = octChips.add (new tchk::Chip ((o > 0 ? "+" : "") + juce::String (o), orange));
        c->onClick = [this, o] { audioProcessor.setParam ("octave", (float) o); refreshPresetUI(); };
        content.addAndMakeVisible (c);
    }
    monoB.onClick = [this] { audioProcessor.setParam ("mono", monoB.getToggleState() ? 0.0f : 1.0f); refreshPresetUI(); };
    demoB.setToggleState (true, juce::dontSendNotification);
    demoB.onClick = [this] { demo(); };
    backB.fontSize = 10.5f;
    backB.setEnabled (false);
    backB.onClick = [this] { undo(); };
    comet.accent = orange;
    comet.onClick = [this] { roll(); };

    presets = std::make_unique<tchk::PresetSystem> (p, "Synthe", presetBox, heartButton, nullptr, nullptr, TK_FR ("♥ MES SONS"), "Mes sons");

    for (juce::Component* c : std::initializer_list<juce::Component*> { &orb, &prevB, &nextB, &monoB, &demoB, &backB, &volume, &comet, &presetBox, &heartButton })
        content.addAndMakeVisible (c);
    layout();
    addInfo (850, 38, TK_FR ("Choisis une famille, puis un son.||Les familles en haut : pads, plucks, leads, cordes, basses, keys, cloches, effets (152 sons en tout).|Les flèches de chaque côté de la sphère passent au son suivant. Clique la sphère pour voir toute la liste et choisir directement.|Joue avec ton clavier MIDI ou le clavier de l'ordinateur d'Ableton (touche M)."), TK_FR ("Sons"));
    addInfo (130, 314, TK_FR ("Retouche le son.||Ces 12 réglages marchent sur tous les sons. Fais tourner le soleil autour de chaque planète.|BRILLANCE : plus sombre ou plus brillant. ESPACE : la réverbération. MOUVEMENT : le son ondule. ATTAQUE : le son arrive d'un coup ou doucement.|HASARD : la comète au centre tire des réglages au hasard, dans des limites qui sonnent toujours bien. ↶ REVENIR remet le son d'avant (tu peux reculer plusieurs fois).|♥ GARDER : enregistre le son tel quel avec un nom. Tu le retrouves dans « Mes sons »."), TK_FR ("Réglages"));
    finishSetup (47, { { 230, 14, 40 }, { 320, 30, 28 }, { 560, 12, 34 } });
    refreshPresetUI();
    startTimerHz (30);
}

SynthEditor::~SynthEditor() { stopTimer(); }

void SynthEditor::layout()
{
    std::vector<int> cw; int total = 0;
    for (auto* c : catChips) { const int w = 20 + (int) (7.0f * (float) c->getButtonText().length()); cw.push_back (w); total += w + 5; }
    int x = juce::jmax (16, (kW - total) / 2 - 10);
    for (int i = 0; i < catChips.size(); ++i) { catChips[i]->fontSize = 12.0f; catChips[i]->setBounds (x, 64, cw[(size_t) i], 30); x += cw[(size_t) i] + 5; }
    orb.setBounds (440 - 160, 104, 320, 186);
    prevB.setBounds (440 - 150, 176, 42, 42); nextB.setBounds (440 + 108, 176, 42, 42);
    volume.setBounds (752, 112, 70, 170);
    const int kx[] = { 44, 136, 228, 568, 660, 752 };
    for (int i = 0; i < 12; ++i) knobs[i]->setBounds (kx[i % 6], i < 6 ? 340 : 438, 84, 84);
    comet.setBounds (392, 356, 96, 92);
    backB.setBounds (398, 468, 84, 22);
    heartButton.setBounds (650, 312, 26, 26);
    presetBox.setBounds (682, 312, 170, 26);
    for (int i = 0; i < octChips.size(); ++i) octChips[i]->setBounds (110 + i * 50, 592, 44, 32);
    monoB.setBounds (372, 592, 70, 32);
    demoB.setBounds (676, 592, 176, 34);
}

void SynthEditor::paintContent (juce::Graphics& g)
{
    paintBackgroundAndHeader (g, TK_FR ("Synthé"), {});
    drawPanelT (g, { 20, 302, 840, 340 }, orange);
    g.setColour (LNF::textDim); g.setFont (LNF::font (12.0f));
    g.drawText (TK_FR ("RÉGLAGES"), 38, 314, 200, 18, juce::Justification::centredLeft);
    g.setColour (orange); g.setFont (LNF::font (11.0f));
    g.drawText ("HASARD", 392, 448, 96, 16, juce::Justification::centred);
    g.setColour (LNF::textDim); g.setFont (LNF::font (11.0f));
    g.drawText ("OCTAVE", 38, 599, 70, 18, juce::Justification::centredLeft);
}

void SynthEditor::overContent (juce::Graphics&) {}

// one planet per family, lit from the top left, its surface turning slowly
void SynthEditor::Orb::paint (juce::Graphics& g)
{
    using C = juce::Colour;
    struct Look { C base, light, dark; };
    static const Look looks[] = { { C (0xffff9a6b), C (0xffffe2cc), C (0xff5a2a6e) }, { C (0xff7fe0ff), C (0xffe8fbff), C (0xff1a3a6e) }, { C (0xffff5a3d), C (0xffffd27a), C (0xff3a0808) },
                                  { C (0xffb48cff), C (0xfff0e4ff), C (0xff2a1450) }, { C (0xff6b2a8a), C (0xffc99ae0), C (0xff0e0418) }, { C (0xffd8d4e8), C (0xffffffff), C (0xff3a3650) },
                                  { C (0xff9ff6ff), C (0xffffffff), C (0xff2a5a8a) }, { C (0xffff5fd2), C (0xffffd1f5), C (0xff1a0a3a) }, { C (0xff3ddcff), C (0xffe6ffff), C (0xff0a4a8a) },
                                  { C (0xff5dd47a), C (0xffe2ffd0), C (0xff0e3a1a) }, { C (0xffffd34d), C (0xfffff6d0), C (0xff6a4a10) } };
    const auto& L = looks[juce::jlimit (0, 10, cat)];
    const double t = tchk::nowSec();
    const auto c = getLocalBounds().toFloat().getCentre();
    const float R = juce::jmin (getWidth(), getHeight()) * 0.5f - 8.0f;
    const auto ctr = c.translated (0, 3.0f * std::sin ((float) t * 1.5f));
    const float rot = (float) t * 0.25f;
    const auto disc = juce::Rectangle<float> (R * 2, R * 2).withCentre (ctr);
    // glow
    g.setGradientFill (juce::ColourGradient (L.base.withAlpha (0.45f), ctr.x, ctr.y, L.base.withAlpha (0.0f), ctr.x + R * 1.55f, ctr.y, true));
    g.fillEllipse (juce::Rectangle<float> (R * 3.1f, R * 3.1f).withCentre (ctr));
    if (cat == 10) // chorale: rays of light
        for (int k = 0; k < 16; ++k)
        {
            const float a = (float) k * juce::MathConstants<float>::twoPi / 16.0f + rot * 0.4f;
            juce::Path ray; ray.addTriangle (ctr.getPointOnCircumference (R * 0.9f, a - 0.05f), ctr.getPointOnCircumference (R * 0.9f, a + 0.05f), ctr.getPointOnCircumference (R * 1.45f, a));
            g.setColour (L.light.withAlpha (0.25f)); g.fillPath (ray);
        }
    auto ring = [&] (bool back, float rx, float ry, float tilt, float alpha, float w)
    {
        juce::Path pth; pth.addCentredArc (ctr.x, ctr.y, rx, ry, tilt, back ? -juce::MathConstants<float>::halfPi : juce::MathConstants<float>::halfPi,
                                           back ? juce::MathConstants<float>::halfPi : juce::MathConstants<float>::halfPi * 3.0f, true);
        g.setColour (L.light.withAlpha (alpha * (back ? 0.55f : 1.0f))); g.strokePath (pth, juce::PathStrokeType (w));
    };
    const bool ringed = cat == 0 || cat == 3 || cat == 10;
    if (ringed) { ring (true, R * 1.6f, R * 0.33f, -0.18f, 0.5f, 4.0f); if (cat == 3) ring (true, R * 1.85f, R * 0.4f, -0.18f, 0.3f, 2.0f); }
    // body (lit from the top left)
    juce::ColourGradient body (L.light, ctr.x - R * 0.45f, ctr.y - R * 0.5f, L.dark, ctr.x + R * 0.9f, ctr.y + R * 0.9f, true);
    body.addColour (0.42, L.base);
    g.setGradientFill (body); g.fillEllipse (disc);
    // surface details turning with the planet
    g.saveState();
    juce::Path clip; clip.addEllipse (disc); g.reduceClipRegion (clip);
    juce::Random rr (cat * 101 + 7);
    auto feature = [&] (float lon, float lat, std::function<void (juce::Point<float>, float)> draw)
    {
        const float a = lon + rot;
        const float vis = std::cos (a);
        if (vis <= 0.05f) return;
        const juce::Point<float> pt (ctr.x + R * std::cos (lat) * std::sin (a), ctr.y + R * std::sin (lat));
        draw (pt, vis);
    };
    switch (cat)
    {
        case 0: case 3: case 7: // bands (gas giant), swirls for fx
            for (int k = 0; k < 7; ++k)
            {
                const float y = ctr.y - R + (float) (k + 1) * R * 2.0f / 8.0f, wob = 4.0f * std::sin (rot * 2.0f + (float) k);
                juce::Path band; band.startNewSubPath (ctr.x - R, y + wob);
                for (int s2 = 1; s2 <= 12; ++s2) band.lineTo (ctr.x - R + (float) s2 * R / 6.0f, y + wob + 3.0f * std::sin ((float) s2 * 0.9f + rot * 3.0f + (float) k));
                g.setColour ((k % 2 ? L.light : L.dark).withAlpha (0.18f)); g.strokePath (band, juce::PathStrokeType (R * 0.12f));
            }
            break;
        case 1: case 6: // crystal facets / ice with sparkles
            for (int k = 0; k < 14; ++k)
                feature (rr.nextFloat() * 6.28f, (rr.nextFloat() - 0.5f) * 2.6f, [&] (juce::Point<float> pt, float vis)
                {
                    const float sz = R * 0.22f * vis;
                    juce::Path f; f.addTriangle (pt.translated (0, -sz), pt.translated (sz * 0.8f, sz * 0.5f), pt.translated (-sz * 0.8f, sz * 0.5f));
                    g.setColour (L.light.withAlpha (0.25f * vis)); g.fillPath (f);
                    if (cat == 6) { const float tw = 0.5f + 0.5f * std::sin ((float) t * 3.0f + pt.x); g.setColour (juce::Colours::white.withAlpha (tw * vis)); g.fillEllipse (pt.x - 2, pt.y - 2, 4, 4); }
                });
            break;
        case 2: // lava cracks
            for (int k = 0; k < 10; ++k)
                feature (rr.nextFloat() * 6.28f, (rr.nextFloat() - 0.5f) * 2.4f, [&] (juce::Point<float> pt, float vis)
                {
                    juce::Path cr; cr.startNewSubPath (pt); cr.lineTo (pt.translated (R * 0.15f * vis, R * 0.08f)); cr.lineTo (pt.translated (R * 0.22f * vis, -R * 0.05f));
                    g.setColour (C (0xffffe08a).withAlpha (0.8f * vis)); g.strokePath (cr, juce::PathStrokeType (2.5f));
                });
            break;
        case 4: case 5: // craters (dark heavy planet / moon)
            for (int k = 0; k < 12; ++k)
                feature (rr.nextFloat() * 6.28f, (rr.nextFloat() - 0.5f) * 2.6f, [&] (juce::Point<float> pt, float vis)
                {
                    const float sz = R * (0.08f + rr.nextFloat() * 0.12f);
                    g.setColour (L.dark.withAlpha (0.35f)); g.fillEllipse (pt.x - sz * vis, pt.y - sz * 0.8f, sz * 2 * vis, sz * 1.6f);
                    g.setColour (L.light.withAlpha (0.25f)); g.drawEllipse (pt.x - sz * vis, pt.y - sz * 0.8f, sz * 2 * vis, sz * 1.6f, 1.2f);
                });
            break;
        case 8: // frutiger aero: bubbles rising inside the glass
            for (int k = 0; k < 9; ++k)
            {
                const float ph = (float) std::fmod (t * 0.25 + k * 0.137, 1.0);
                const float bx = ctr.x + R * 0.7f * std::sin ((float) k * 2.1f), by = ctr.y + R - ph * R * 2.0f, sz = R * (0.06f + 0.05f * (float) (k % 3));
                g.setColour (juce::Colours::white.withAlpha (0.5f * (1.0f - ph))); g.drawEllipse (bx - sz, by - sz, sz * 2, sz * 2, 1.5f);
                g.fillEllipse (bx - sz * 0.4f, by - sz * 0.5f, sz * 0.5f, sz * 0.4f);
            }
            break;
        case 9: // botanica: leaves
            for (int k = 0; k < 12; ++k)
                feature (rr.nextFloat() * 6.28f, (rr.nextFloat() - 0.5f) * 2.4f, [&] (juce::Point<float> pt, float vis)
                {
                    const float sz = R * 0.16f;
                    juce::Path leaf; leaf.startNewSubPath (pt.translated (-sz * vis, 0)); leaf.quadraticTo (pt.translated (0, -sz * 0.7f), pt.translated (sz * vis, 0)); leaf.quadraticTo (pt.translated (0, sz * 0.7f), pt.translated (-sz * vis, 0));
                    g.setColour (C (0xff1f7a3a).withAlpha (0.55f * vis)); g.fillPath (leaf);
                    g.setColour (L.light.withAlpha (0.3f * vis)); g.strokePath (leaf, juce::PathStrokeType (1.0f));
                });
            break;
        default: break;
    }
    g.restoreState();
    // night side shadow + rim light + glossy highlight
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, ctr.x - R * 0.2f, ctr.y - R * 0.2f, juce::Colours::black.withAlpha (0.55f), ctr.x + R, ctr.y + R, false));
    g.fillEllipse (disc);
    g.setColour (L.light.withAlpha (0.35f)); g.drawEllipse (disc.reduced (1.0f), 1.5f);
    g.setColour (juce::Colours::white.withAlpha (cat == 8 ? 0.45f : 0.2f)); g.fillEllipse (ctr.x - R * 0.6f, ctr.y - R * 0.72f, R * 0.7f, R * 0.36f);
    if (ringed) { ring (false, R * 1.6f, R * 0.33f, -0.18f, 0.7f, 4.0f); if (cat == 3) ring (false, R * 1.85f, R * 0.4f, -0.18f, 0.45f, 2.0f); }
    // texts
    const bool darkText = cat == 0 || cat == 1 || cat == 5 || cat == 6 || cat == 8 || cat == 10;
    const auto ink = darkText ? C (0xff14081f) : C (0xfff4ecff);
    g.setColour (ink.withAlpha (0.7f)); g.setFont (LNF::font (10.5f));
    g.drawText (family, juce::Rectangle<float> (R * 1.8f, 14).withCentre (ctr.translated (0, -R * 0.34f)), juce::Justification::centred);
    g.setColour (ink); g.setFont (LNF::font (21.0f));
    g.drawFittedText (name, juce::Rectangle<float> (R * 1.7f, 50).withCentre (ctr.translated (0, -R * 0.02f)).toNearestInt(), juce::Justification::centred, 2);
    g.setColour (ink.withAlpha (0.6f)); g.setFont (LNF::font (10.0f));
    g.drawText (TK_FR ("▾ tous les sons"), juce::Rectangle<float> (R * 1.6f, 14).withCentre (ctr.translated (0, R * 0.38f)), juce::Justification::centred);
}

void SynthEditor::openList()
{
    const auto [c, cur] = snd::split (audioProcessor.presetIndex());
    juce::PopupMenu m;
    m.addSectionHeader (snd::categories()[c].toUpperCase());
    const auto& n = snd::names (c);
    for (int i = 0; i < n.size(); ++i) m.addItem (i + 1, n[i], true, i == cur);
    juce::Component::SafePointer<SynthEditor> safe (this);
    const int cat = c;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&orb), [safe, cat] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        safe->audioProcessor.selectPreset (snd::join (cat, r - 1));
        safe->refreshPresetUI();
        safe->demo();
    });
}

void SynthEditor::refreshPresetUI()
{
    const auto [c, i] = snd::split (audioProcessor.presetIndex());
    for (int k = 0; k < catChips.size(); ++k) catChips[k]->setToggleState (k == c, juce::dontSendNotification);
    orb.family = snd::categories()[c].toUpperCase() + "  " + juce::String (i + 1) + "/" + juce::String (snd::names (c).size());
    orb.name = snd::names (c)[i];
    orb.cat = c;
    orb.repaint();
    const int oct = (int) audioProcessor.apvts.getRawParameterValue ("octave")->load();
    for (int k = 0; k < octChips.size(); ++k) octChips[k]->setToggleState (k - 2 == oct, juce::dontSendNotification);
    monoB.setToggleState (audioProcessor.apvts.getRawParameterValue ("mono")->load() > 0.5f, juce::dontSendNotification);
    lastPreset = audioProcessor.presetIndex();
}

void SynthEditor::demo()
{
    if (audioProcessor.player.playing.load()) return;
    for (int n : { 60, 64, 67 }) audioProcessor.preview (n, 0.8f, 900);
}

std::vector<float> SynthEditor::snapshot() const
{
    std::vector<float> v;
    for (auto* id : ids) v.push_back (audioProcessor.apvts.getRawParameterValue (id)->load());
    return v;
}

void SynthEditor::animateTo (const std::vector<float>& target)
{
    animFrom = snapshot(); animTo = target; animStart = tchk::nowSec();
}

void SynthEditor::roll()
{
    history.push_back (snapshot());
    if (history.size() > 30) history.erase (history.begin());
    std::vector<float> tgt;
    auto& r = juce::Random::getSystemRandom();
    for (int k = 0; k < 12; ++k) tgt.push_back (lo[k] + r.nextFloat() * (hi[k] - lo[k]));
    animateTo (tgt);
    backB.setEnabled (true);
    juce::Timer::callAfterDelay (420, [safe = juce::Component::SafePointer<SynthEditor> (this)] { if (safe != nullptr) safe->demo(); });
}

void SynthEditor::undo()
{
    if (history.empty()) return;
    animateTo (history.back());
    history.pop_back();
    backB.setEnabled (! history.empty());
    juce::Timer::callAfterDelay (420, [safe = juce::Component::SafePointer<SynthEditor> (this)] { if (safe != nullptr) safe->demo(); });
}

void SynthEditor::timerCallback()
{
    if (animStart > 0)
    {
        const float u = (float) juce::jlimit (0.0, 1.0, (tchk::nowSec() - animStart) / 0.38);
        const float e = 1.0f - std::pow (1.0f - u, 3.0f);
        for (int k = 0; k < 12; ++k)
            if (auto* prm = audioProcessor.apvts.getParameter (ids[k]))
                prm->setValueNotifyingHost (animFrom[(size_t) k] + (animTo[(size_t) k] - animFrom[(size_t) k]) * e);
        if (u >= 1.0f) animStart = -1;
    }
    if (audioProcessor.presetIndex() != lastPreset) refreshPresetUI();
    comet.repaint();
    orb.repaint();
}
