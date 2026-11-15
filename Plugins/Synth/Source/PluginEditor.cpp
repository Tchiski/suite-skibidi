#include "PluginEditor.h"

using tchk::LNF;
namespace snd = tchk::sounds;

namespace
{
    constexpr int kW = 880, kH = 652;
    const char* macroIds[] = { "bright", "space", "motion", "attack" };
    const char* fineIds[] = { "release", "width", "warmth", "vibrato", "glide", "filter", "reso", "detune" };
    // random ranges that always sound good: fine settings, then macros
    const float lo[] = { 0.05f, 0.3f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.1f, 0.1f, 0.1f, 0.1f };
    const float hi[] = { 0.8f, 1.0f, 0.7f, 0.4f, 0.35f, 1.0f, 0.6f, 0.6f, 0.9f, 0.9f, 0.9f, 0.9f };
}

SynthEditor::SynthEditor (SynthProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.9f, "synth"),
      audioProcessor (p),
      catWheel (snd::categories(), orange),
      volume (p.apvts, "volume", "Volume", orange, true)
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.overFn = [this] (juce::Graphics& g) { overContent (g); };
    content.layoutFn = [this] { layout(); };
    levelDb = [&p] { return p.peakDb.load(); };

    catWheel.onPick = [this] (int c) { audioProcessor.selectPreset (snd::join (c, 0)); showCategory (c); demo(); };
    for (int i = 0; i < 19; ++i)
    {
        auto* c = presetChips.add (new tchk::Chip ({}, orange)); c->fontSize = 12.0f;
        c->onClick = [this, i] { audioProcessor.selectPreset (snd::join (juce::jmax (0, shownCat), i)); refreshPresetUI(); demo(); };
        content.addAndMakeVisible (c);
    }
    auto step = [this] (int d)
    {
        const auto [c, i] = snd::split (audioProcessor.presetIndex());
        const int n = snd::names (c).size();
        audioProcessor.selectPreset (snd::join (c, (i + d + n) % n)); refreshPresetUI(); demo();
    };
    prevB.onClick = [step] { step (-1); };
    nextB.onClick = [step] { step (1); };

    const char* macroNames[] = { "Brillance", "Espace", "Mouvement", "Attaque" };
    for (int i = 0; i < 4; ++i) { auto* k = macros.add (new tchk::Knob (p.apvts, macroIds[i], macroNames[i], orange, true)); k->asOrbit(); content.addAndMakeVisible (k); }
    const juce::StringArray fineNames { TK_FR ("Relâchement"), "Largeur", "Chaleur", "Vibrato", "Glide", "Filtre", TK_FR ("Résonance"), TK_FR ("Détune") };
    for (int i = 0; i < 8; ++i) { auto* k = fine.add (new tchk::Knob (p.apvts, fineIds[i], fineNames[i], orange, true)); k->asOrbit(); content.addAndMakeVisible (k); }
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

    for (juce::Component* c : std::initializer_list<juce::Component*> { &catWheel, &prevB, &nextB, &monoB, &demoB, &backB, &volume, &comet, &presetBox, &heartButton })
        content.addAndMakeVisible (c);
    layout();
    addInfo (124, 64, TK_FR ("Choisis une famille, puis un preset.||152 sons en 8 familles : pads, plucks, leads, cordes, basses, keys, cloches et effets. Les flèches passent au son suivant."), TK_FR ("Catégories"));
    addInfo (150, 430, TK_FR ("Retouche le son du preset.||Ces réglages marchent sur tous les presets. Fais tourner le soleil autour de chaque planète.|HASARD : la comète au centre tire des réglages au hasard (réglages fins + Brillance, Espace, Mouvement, Attaque), dans des limites qui sonnent toujours bien.|↶ REVENIR : remet le son d'avant le dernier tirage (tu peux reculer plusieurs fois).|♥ GARDER : enregistre le son tel quel avec un nom. Tu le retrouves dans « Mes sons »."), TK_FR ("Réglages fins"));
    addInfo (846, 70, TK_FR ("Les 4 grands réglages.||BRILLANCE : plus sombre ou plus brillant. ESPACE : la réverbération. MOUVEMENT : le son ondule. ATTAQUE : le son arrive d'un coup ou doucement. La fusée au centre règle le volume."), "Macros");
    finishSetup (47, { { 230, 14, 40 }, { 320, 30, 28 }, { 560, 12, 34 } });
    refreshPresetUI();
    startTimerHz (30);
}

SynthEditor::~SynthEditor() { stopTimer(); }

void SynthEditor::layout()
{
    catWheel.setBounds (24, 86, 280, 280);
    prevB.setBounds (348, 74, 40, 40); nextB.setBounds (592, 74, 40, 40);
    for (int i = 0; i < presetChips.size(); ++i) presetChips[i]->setBounds (340 + (i % 2) * 152, 128 + (i / 2) * 27, 148, 25);
    const int cx = 762, cy = 222;
    const juce::Point<int> spots[] = { { cx, cy - 112 }, { cx + 72, cy }, { cx, cy + 112 }, { cx - 72, cy } };
    for (int i = 0; i < 4; ++i) macros[i]->setBounds (juce::Rectangle<int> (84, 76).withCentre (spots[i]));
    volume.setBounds (cx - 30, cy - 62, 60, 124);
    const int fx[] = { 40, 132, 224, 316, 492, 584, 676, 768 };
    for (int i = 0; i < 8; ++i) fine[i]->setBounds (fx[i], 456, 84, 82);
    comet.setBounds (398, 444, 96, 92);
    backB.setBounds (404, 540, 84, 22);
    heartButton.setBounds (650, 424, 26, 26);
    presetBox.setBounds (682, 424, 170, 26);
    for (int i = 0; i < octChips.size(); ++i) octChips[i]->setBounds (110 + i * 50, 592, 44, 32);
    monoB.setBounds (372, 592, 70, 32);
    demoB.setBounds (676, 592, 176, 34);
}

void SynthEditor::paintContent (juce::Graphics& g)
{
    paintBackgroundAndHeader (g, TK_FR ("Synthé"), {});
    g.setColour (LNF::textDim); g.setFont (LNF::font (12.0f));
    g.drawText (TK_FR ("CATÉGORIES"), 24, 64, 200, 18, juce::Justification::centredLeft);
    // name box
    juce::Rectangle<float> nb (340, 66, 300, 56);
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (nb, 28.0f);
    g.setColour (orange.withAlpha (0.5f)); g.drawRoundedRectangle (nb.reduced (0.75f), 28.0f, 1.5f);
    // macro orbit ring
    g.setColour (orange.withAlpha (0.25f));
    juce::Path ring; ring.addEllipse (762 - 92, 222 - 92, 184, 184);
    float dashes[] = { 3.0f, 6.0f }; juce::Path d; juce::PathStrokeType (1.0f).createDashedStroke (d, ring, dashes, 2); g.fillPath (d);
    // fine settings panel
    drawPanelT (g, { 20, 414, 840, 228 }, orange);
    g.setColour (LNF::textDim); g.setFont (LNF::font (12.0f));
    g.drawText (TK_FR ("RÉGLAGES FINS"), 38, 430, 200, 18, juce::Justification::centredLeft);
    g.setColour (orange); g.setFont (LNF::font (11.0f));
    g.drawText ("HASARD", 398, 528, 96, 14, juce::Justification::centred);
    g.setColour (LNF::textDim); g.setFont (LNF::font (11.0f));
    g.drawText ("OCTAVE", 38, 599, 70, 18, juce::Justification::centredLeft);
}

void SynthEditor::overContent (juce::Graphics& g)
{
    const auto [c, i] = snd::split (audioProcessor.presetIndex());
    g.setColour (LNF::textDim); g.setFont (LNF::font (10.0f));
    g.drawText (snd::categories()[c].toUpperCase() + "  " + juce::String (i + 1) + "/" + juce::String (snd::names (c).size()), 390, 74, 200, 14, juce::Justification::centred);
    g.setColour (orange); g.setFont (LNF::font (19.0f));
    g.drawFittedText (snd::names (c)[i], 392, 88, 196, 26, juce::Justification::centred, 1);
}

void SynthEditor::showCategory (int cat)
{
    shownCat = cat;
    catWheel.setSelected (cat);
    const auto& n = snd::names (cat);
    for (int i = 0; i < presetChips.size(); ++i)
    {
        presetChips[i]->setVisible (i < n.size());
        if (i < n.size()) presetChips[i]->setButtonText (n[i]);
    }
    refreshPresetUI();
}

void SynthEditor::refreshPresetUI()
{
    const auto [c, i] = snd::split (audioProcessor.presetIndex());
    if (c != shownCat) { showCategory (c); return; }
    for (int k = 0; k < presetChips.size(); ++k) presetChips[k]->setToggleState (k == i, juce::dontSendNotification);
    const int oct = (int) audioProcessor.apvts.getRawParameterValue ("octave")->load();
    for (int k = 0; k < octChips.size(); ++k) octChips[k]->setToggleState (k - 2 == oct, juce::dontSendNotification);
    monoB.setToggleState (audioProcessor.apvts.getRawParameterValue ("mono")->load() > 0.5f, juce::dontSendNotification);
    lastPreset = audioProcessor.presetIndex();
    content.repaint();
}

void SynthEditor::demo()
{
    if (audioProcessor.player.playing.load()) return;
    for (int n : { 60, 64, 67 }) audioProcessor.preview (n, 0.8f, 900);
}

std::vector<float> SynthEditor::snapshot() const
{
    std::vector<float> v;
    for (auto* id : fineIds) v.push_back (audioProcessor.apvts.getRawParameterValue (id)->load());
    for (auto* id : macroIds) v.push_back (audioProcessor.apvts.getRawParameterValue (id)->load());
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
            if (auto* prm = audioProcessor.apvts.getParameter (k < 8 ? fineIds[k] : macroIds[k - 8]))
                prm->setValueNotifyingHost (animFrom[(size_t) k] + (animTo[(size_t) k] - animFrom[(size_t) k]) * e);
        if (u >= 1.0f) animStart = -1;
    }
    if (audioProcessor.presetIndex() != lastPreset) refreshPresetUI();
    comet.repaint();
}
