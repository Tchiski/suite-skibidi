#include "PluginEditor.h"

using tchk::LNF;
namespace music = tchk::music;

namespace { constexpr int kW = 880, kH = 652; }

AccordsEditor::AccordsEditor (AccordsProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.9f, "accords"),
      audioProcessor (p),
      keyWheel (music::noteNames(), gold),
      dragMidi ("Glisser le MIDI", [this] { return tchk::exportfiles::writeMidi (audioProcessor.getNotes(), audioProcessor.player.bpm, "Tchiskibidi Accords"); }),
      dragAudio (TK_FR ("Glisser l'audio"), [this] { return tchk::exportfiles::writeWav (audioProcessor.getNotes(), audioProcessor.totalSteps(), audioProcessor.player.bpm, audioProcessor.exportPatch(), "Tchiskibidi Accords"); }),
      soundBox (p.apvts, "sound"),
      volume (p.apvts, "volume", "Volume", gold, true)
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.layoutFn = [this] { layout(); };
    levelDb = [&p] { return p.peakDb.load(); };

    keyWheel.onPick = [this] (int i) { audioProcessor.setInt ("root", i); regenerate(); };
    for (int i = 0; i < music::scaleNames().size(); ++i)
    {
        auto* c = scaleChips.add (new tchk::Chip (music::scaleNames()[i], gold)); c->fontSize = 11.5f;
        c->onClick = [this, i] { audioProcessor.setInt ("scale", i); regenerate(); };
    }
    for (int b : { 4, 8 })
    {
        auto* c = barChips.add (new tchk::Chip (juce::String (b) + " MESURES", gold));
        c->onClick = [this, b] { audioProcessor.setInt ("bars", b); regenerate(); };
    }
    for (int i = 0; i < tchk::gen::moodNames().size(); ++i)
    {
        auto* c = moodChips.add (new tchk::Chip (tchk::gen::moodNames()[i], gold));
        c->onClick = [this, i] { audioProcessor.setInt ("mood", i); regenerate(); };
    }
    seventh.onClick = [this] { audioProcessor.setInt ("seventh", audioProcessor.getInt ("seventh", 0) ? 0 : 1); regenerate(); };
    listen.outline = true;
    listen.onClick = [this] { openListen(); };
    star.accent = gold;
    star.onClick = [this] { regenerate(); };

    roll.accent = gold;
    roll.newLength = 8;
    roll.onEdit = [this] { audioProcessor.setNotes (roll.notes); content.repaint(); };
    roll.onAudition = [this] (int pitch) { if (! audioProcessor.player.playing) audioProcessor.preview (pitch, 0.8f, 300); };
    roll.getLabels = [this] { return chordLabels(); };
    roll.getPlayStep = [this] { return audioProcessor.player.playStep.load(); };
    badge.isPlaying = [this] { return audioProcessor.player.playing.load(); };
    volume.asOrbit();

    presets = std::make_unique<tchk::PresetSystem> (p, "Accords", presetBox, heartButton, nullptr, nullptr, TK_FR ("♥ MES PROGRESSIONS"), "Mes progressions");

    // listening panel
    srcMidi.setToggleState (true, juce::dontSendNotification);
    srcMidi.onClick = [this] { srcMidi.setToggleState (true, juce::dontSendNotification); srcAudio.setToggleState (false, juce::dontSendNotification); audioProcessor.startListening (1); };
    srcAudio.onClick = [this] { srcAudio.setToggleState (true, juce::dontSendNotification); srcMidi.setToggleState (false, juce::dontSendNotification); audioProcessor.startListening (2); };
    genMatch.setEnabled (false);
    genMatch.onClick = [this]
    {
        audioProcessor.setInt ("root", heardRoot);
        audioProcessor.setInt ("scale", heardMinor ? 1 : 0);
        regenerate (&heardDegrees);
        audioProcessor.stopListening();
        panel.setVisible (false);
    };
    closeListen.onClick = [this] { audioProcessor.stopListening(); panel.setVisible (false); };
    for (auto* c : { &srcMidi, &srcAudio, &genMatch, &closeListen }) panel.addAndMakeVisible (c);

    for (juce::Component* c : std::initializer_list<juce::Component*> { &keyWheel, &seventh, &listen, &star, &roll, &badge, &dragMidi, &dragAudio, &soundBox, &volume, &presetBox, &heartButton })
        content.addAndMakeVisible (c);
    for (auto* arr : { &scaleChips, &barChips, &moodChips }) for (auto* c : *arr) content.addAndMakeVisible (c);
    content.addChildComponent (panel);

    layout();
    addInfo (112, 64, TK_FR ("Clique la note de ton morceau.||NOTES : Do = C, Do# = C#, Ré = D, Ré# = D#, Mi = E, Fa = F, Fa# = F#, Sol = G, Sol# = G#, La = A, La# = A#, Si = B.|Clique la note de ton morceau sur la roue, puis la gamme. Majeur = joyeux, Mineur = triste ou sombre.|Tu ne la connais pas ? « Écouter mes pistes » la trouve pour toi."), TK_FR ("Tonalité"));
    addInfo (86, 345, TK_FR ("La couleur des accords.||Majeur = lumineux, Mineur = triste ou sombre, Dorien = mélancolique groovy, Phrygien = oriental, Mixolydien = funky. Mineur harm. = mineur avec une tension orientale / classique."), "Gamme");
    addInfo (742, 64, TK_FR ("4 ou 8 mesures.||En 8 mesures, la deuxième moitié répond à la première et finit souvent sur une tension pour relancer la boucle."), "Mesures");
    addInfo (750, 124, TK_FR ("Le style des accords.||L'ambiance change les enchaînements ET le rythme : Trap ajoute des coups courts, R&B coupe les mesures en deux, Rêveur garde des accords longs."), "Ambiance");
    addInfo (844, 296, TK_FR ("Branche ta piste, joue-la, il trouve des accords qui vont avec.||EN MIDI (le plus fiable) : sur la piste Accords, règle « MIDI From » sur ta piste de piano ou de synthé.|EN AUDIO : dans le menu Sidechain en haut du plugin (Ableton), choisis ta piste. Marche sur un sample, moins précis avec batterie ou grosse basse.|ENSUITE : lance la lecture, joue 4 ou 8 mesures, il trouve la tonalité et les accords. Clique « Générer une progression qui matche » : nouvelle progression dans la même tonalité, sans copier la tienne."), TK_FR ("Écouter mes pistes"));
    addInfo (472, 610, TK_FR ("Corrige les notes à la main.||Clic dans le vide = ajouter une note. Glisser dans le vide = sélection multiple. Clic sur une note = la sélectionner, re-clic = l'effacer. Bord droit = allonger. Suppr = effacer la sélection. Cmd + A = tout sélectionner.|LECTURE : pas de bouton Écouter. Accords joue avec Ableton (barre d'espace) et recommence pile au début de ta boucle. Clique l'étoile pendant la lecture : la nouvelle progression prend le relais.|GLISSER : fais glisser le MIDI sur une piste MIDI, ou l'audio sur une piste audio."), "Grille MIDI");
    addInfo (606, 382, TK_FR ("Le son des accords.||Choisis le son joué par Accords. « Muet » : Accords n'envoie que le MIDI, pour jouer tes accords avec un autre instrument (sur l'autre piste, règle « MIDI From » sur la piste Accords)."), "Son");
    finishSetup (31, { { 250, 14, 40 }, { 330, 32, 28 }, { 560, 12, 36 } });
    refreshFromState();
    startTimerHz (30);
}

AccordsEditor::~AccordsEditor() { stopTimer(); audioProcessor.stopListening(); }

void AccordsEditor::layout()
{
    keyWheel.setBounds (34, 84, 250, 250);
    const int sw[] = { 74, 70, 70, 82, 108, 94 };
    int x = 110, y = 340;
    for (int i = 0; i < scaleChips.size(); ++i)
    {
        if (i == 3) { x = 24; y = 372; }
        scaleChips[i]->setBounds (x, y, sw[i], 28); x += sw[i] + 4;
    }
    star.setBounds (310, 48, 320, 320);
    soundBox.setBounds (330, 384, 170, 26);
    volume.setBounds (516, 362, 86, 72);
    barChips[0]->setBounds (650, 84, 104, 30); barChips[1]->setBounds (760, 84, 104, 30);
    for (int i = 0; i < moodChips.size(); ++i) moodChips[i]->setBounds (650 + (i % 3) * 72, 144 + (i / 3) * 36, 68, 30);
    seventh.setBounds (650, 220, 214, 30);
    listen.setBounds (650, 258, 214, 30);
    heartButton.setBounds (650, 332, 26, 26);
    presetBox.setBounds (682, 332, 182, 26);
    roll.setBounds (20, 440, 840, 152);
    badge.setBounds (20, 602, 190, 38);
    dragMidi.setBounds (520, 603, 160, 36);
    dragAudio.setBounds (690, 603, 170, 36);
    panel.setBounds (300, 60, 340, 330);
    srcMidi.setBounds (18, 50, 140, 30); srcAudio.setBounds (166, 50, 156, 30);
    genMatch.setBounds (18, 236, 304, 34); closeListen.setBounds (18, 280, 304, 32);
}

void AccordsEditor::paintContent (juce::Graphics& g)
{
    paintBackgroundAndHeader (g, "Accords", {});
    auto dim = [&] (const juce::String& txt, int x, int y, int w = 200, float size = 12.0f, bool bold = true)
    { g.setColour (LNF::textDim); g.setFont (LNF::font (size, bold)); g.drawText (txt, x, y, w, 18, juce::Justification::centredLeft); };
    dim (TK_FR ("TONALITÉ"), 24, 64);
    dim ("GAMME", 24, 345, 80, 11.0f);
    dim ("MESURES", 650, 64);
    dim ("AMBIANCE", 650, 124);
    dim (TK_FR ("Il écoute ta progression et en"), 650, 290, 200, 10.5f, false);
    dim (TK_FR ("propose une qui va avec."), 650, 304, 200, 10.5f, false);
    dim ("SON", 330, 366, 60, 10.5f);
    dim (TK_FR ("Grille MIDI : modifie les notes à la main"), 222, 610, 250, 11.0f, false);
}

void AccordsEditor::regenerate (const std::vector<int>* avoid)
{
    audioProcessor.generate (avoid);
    refreshFromState();
}

void AccordsEditor::refreshFromState()
{
    keyWheel.setSelected (audioProcessor.getInt ("root", 9));
    const int sc = audioProcessor.getInt ("scale", 1), bars = audioProcessor.getInt ("bars", 4), mood = audioProcessor.getInt ("mood", 0);
    for (int i = 0; i < scaleChips.size(); ++i) scaleChips[i]->setToggleState (i == sc, juce::dontSendNotification);
    barChips[0]->setToggleState (bars == 4, juce::dontSendNotification); barChips[1]->setToggleState (bars == 8, juce::dontSendNotification);
    for (int i = 0; i < moodChips.size(); ++i) moodChips[i]->setToggleState (i == mood, juce::dontSendNotification);
    seventh.setToggleState (audioProcessor.getInt ("seventh", 0) != 0, juce::dontSendNotification);
    roll.notes = audioProcessor.getNotes();
    roll.totalSteps = audioProcessor.totalSteps();
    roll.fit();
    roll.repaint();
    lastVersion = audioProcessor.stateVersion.load();
}

std::vector<std::tuple<int, int, juce::String>> AccordsEditor::chordLabels() const
{
    std::vector<std::tuple<int, int, juce::String>> out;
    std::vector<int> starts;
    for (auto& n : roll.notes) if (std::find (starts.begin(), starts.end(), n.start) == starts.end()) starts.push_back (n.start);
    std::sort (starts.begin(), starts.end());
    for (size_t k = 0; k < starts.size(); ++k)
    {
        const int s0 = starts[k], s1 = k + 1 < starts.size() ? starts[k + 1] : roll.totalSteps;
        std::vector<int> ps;
        for (auto& n : roll.notes) if (n.start <= s0 && n.start + n.length > s0) ps.push_back (n.pitch);
        if (! ps.empty() && s1 > s0) out.push_back ({ s0, s1 - s0, music::chordName (ps) });
    }
    return out;
}

void AccordsEditor::openListen()
{
    panel.chords.clear(); panel.keyText = {}; panel.progress = 0;
    panel.status = TK_FR ("Lance la lecture dans Ableton et joue ta progression...");
    genMatch.setEnabled (false);
    srcMidi.setToggleState (true, juce::dontSendNotification); srcAudio.setToggleState (false, juce::dontSendNotification);
    audioProcessor.startListening (1);
    panel.setVisible (true);
    panel.toFront (false);
}

void AccordsEditor::analyseListen()
{
    const auto halves = audioProcessor.takeListenResult();
    auto heard = tchk::gen::chordsFromChroma (halves);
    std::array<float, 12> hist {};
    for (auto& h : halves) for (int k = 0; k < 12; ++k) hist[(size_t) k] += h[(size_t) k];
    float tot = 0; for (float v : hist) tot += v;
    panel.chords.clear();
    if (tot < 1e-6f || heard.empty())
    {
        panel.status = TK_FR ("Je n'ai rien entendu. Vérifie le routage (voir le i) et réessaie.");
        audioProcessor.startListening (srcAudio.getToggleState() ? 2 : 1);
        return;
    }
    const auto key = music::detectKey (hist);
    heardRoot = key.first; heardMinor = key.second;
    heardDegrees.clear();
    const auto& sc = music::scale (heardMinor ? 1 : 0);
    for (auto& c : heard)
    {
        panel.chords.push_back (music::noteNames()[c.root] + (c.minor ? "m" : ""));
        int deg = 0;
        for (int d = 0; d < 7; ++d) if ((heardRoot + sc[(size_t) d]) % 12 == c.root) deg = d;
        if (heardDegrees.size() < 4) heardDegrees.push_back (deg);
    }
    while (heardDegrees.size() < 4) heardDegrees.push_back (heardDegrees.empty() ? 0 : heardDegrees.back());
    panel.keyText = TK_FR ("Tonalité trouvée : ") + music::noteNames()[heardRoot] + (heardMinor ? " mineur" : " majeur");
    panel.status = TK_FR ("C'est bon, j'ai compris ta progression.");
    genMatch.setEnabled (true);
    audioProcessor.stopListening();
}

void AccordsEditor::ListenPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xf20e0614)); g.fillRoundedRectangle (r, 20.0f);
    g.setColour (juce::Colour (0xffffd34d).withAlpha (0.5f)); g.drawRoundedRectangle (r.reduced (0.75f), 20.0f, 1.5f);
    g.setColour (juce::Colour (0xffffd34d)); g.setFont (LNF::font (16.0f));
    g.drawText (TK_FR ("J'ÉCOUTE TES PISTES"), 18, 14, 300, 24, juce::Justification::centredLeft);
    g.setColour (LNF::textDim); g.setFont (LNF::font (12.0f, false));
    g.drawFittedText (status, 18, 88, 304, 32, juce::Justification::centredLeft, 2);
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (18, 124, 304 * juce::jlimit (0.0f, 1.0f, progress), 4, 2);
    const int n = juce::jmax (1, (int) chords.size());
    for (int i = 0; i < (int) chords.size() && i < 8; ++i)
    {
        const float w = 304.0f / (float) juce::jmin (8, n);
        juce::Rectangle<float> c (18 + w * (float) i, 136, w - 6, 44);
        g.setColour (juce::Colour (0xffffd34d).withAlpha (0.15f)); g.fillRoundedRectangle (c, 10);
        g.setColour (juce::Colour (0xffffd34d)); g.setFont (LNF::font (n > 5 ? 12.0f : 17.0f));
        g.drawText (chords[(size_t) i], c, juce::Justification::centred);
    }
    g.setColour (LNF::text); g.setFont (LNF::font (13.0f));
    g.drawText (keyText, 18, 190, 304, 20, juce::Justification::centredLeft);
}

void AccordsEditor::timerCallback()
{
    if (audioProcessor.stateVersion.load() != lastVersion) refreshFromState();
    if (audioProcessor.player.playing.load()) roll.repaint();
    badge.repaint();
    if (panel.isVisible())
    {
        if (audioProcessor.listenMode.load() != 0)
        {
            const float pr = audioProcessor.listenProgress.load();
            if (pr > 0.0f) panel.status = TK_FR ("J'écoute... ") + juce::String (juce::roundToInt (pr * 100.0f)) + " %";
            panel.progress = pr;
            if (audioProcessor.listenDone.load()) analyseListen();
        }
        panel.repaint();
    }
}

