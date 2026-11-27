#include "PluginEditor.h"

using tchk::LNF;
namespace music = tchk::music;

namespace { constexpr int kW = 880, kH = 652; }

AccordsEditor::AccordsEditor (AccordsProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.9f, "accords"),
      audioProcessor (p),
      keyWheel (music::noteNames(), gold),
      dragMidi ("Glisser le MIDI", [this] { return tchk::exportfiles::writeMidi (audioProcessor.getNotes(), audioProcessor.player.bpm, "Tchiskibidi Supernova"); }),
      dragAudio (TK_FR ("Glisser l'audio"), [this] { return tchk::exportfiles::writeWav (audioProcessor.getNotes(), audioProcessor.totalSteps(), audioProcessor.player.bpm, audioProcessor.exportPatch(), "Tchiskibidi Supernova"); }),
      soundBox (p.apvts, "sound"),
      volume (p.apvts, "volume", "Volume", gold, true)
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.overFn = [this] (juce::Graphics& g) { overContent (g); };
    content.layoutFn = [this] { layout(); };
    levelDb = [&p] { return p.peakDb.load(); };

    keyWheel.onPick = [this] (int i) { pushHistory(); audioProcessor.setInt ("root", i); regenerate (nullptr, false); };
    for (int i = 0; i < music::scaleNames().size(); ++i)
    {
        auto* c = scaleChips.add (new tchk::Chip (music::scaleNames()[i], gold)); c->fontSize = 11.5f;
        c->onClick = [this, i] { pushHistory(); audioProcessor.setInt ("scale", i); regenerate (nullptr, false); };
    }
    for (int b : { 4, 8 })
    {
        auto* c = barChips.add (new tchk::Chip (juce::String (b) + " MESURES", gold));
        c->onClick = [this, b] { pushHistory(); audioProcessor.setInt ("bars", b); regenerate (nullptr, false); };
    }
    for (int i = 0; i < tchk::gen::moodNames().size(); ++i)
    {
        auto* c = moodChips.add (new tchk::Chip (tchk::gen::moodNames()[i], gold));
        c->onClick = [this, i] { pushHistory(); audioProcessor.setInt ("mood", i); regenerate (nullptr, false); };
    }
    seventh.onClick = [this] { pushHistory(); audioProcessor.setInt ("seventh", audioProcessor.getInt ("seventh", 0) ? 0 : 1); regenerate (nullptr, false); };
    listen.outline = true;
    listen.onClick = [this] { openListen(); };
    star.accent = gold;
    star.supernova = true;
    keyWheel.faceted = true;
    for (auto* c : scaleChips) c->shape = tchk::Chip::hex;
    for (auto* c : moodChips) c->shape = tchk::Chip::hex;
    for (auto* c : barChips) c->shape = tchk::Chip::hex;
    seventh.shape = tchk::Chip::hex; listen.shape = tchk::Chip::hex;
    for (auto* c : { &octDown, &octUp }) c->onLight = true;
    badge.onLight = true; dragMidi.onLight = true; dragAudio.onLight = true;
    roll.shapeFn = [] (juce::Rectangle<float> r) { return tchk::shapes::facetPath (r, 16.0f, 11); };
    star.onClick = [this]
    {
        if (roll.hasSelection())   // the selected notes stay, the rest is regenerated around them
        {
            pushHistory();
            audioProcessor.generateAround (roll.selectedNotes());
            refreshFromState();
        }
        else regenerate();
    };
    undoB.fontSize = 10.5f; undoB.setEnabled (false);
    undoB.onClick = [this] { doUndo(); };
    octDown.onClick = [this] { shiftOctave (-1); };
    octUp.onClick = [this] { shiftOctave (1); };

    roll.accent = gold;
    roll.newLength = 8;
    roll.onEdit = [this]
    {
        if (tchk::notesToString (roll.notes) == audioProcessor.apvts.state.getProperty ("notes").toString()) return;   // just a selection
        pushHistory();
        audioProcessor.setNotes (roll.notes); content.repaint();
    };
    roll.onUndo = [this] { doUndo(); };
    roll.onRedo = [this] { doRedo(); };
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
        pushHistory();
        regenerate (&heardDegrees, false);
        audioProcessor.stopListening();
        panel.setVisible (false);
    };
    closeListen.onClick = [this] { audioProcessor.stopListening(); panel.setVisible (false); };
    for (auto* c : { &srcMidi, &srcAudio, &genMatch, &closeListen }) panel.addAndMakeVisible (c);

    for (juce::Component* c : std::initializer_list<juce::Component*> { &keyWheel, &seventh, &listen, &star, &undoB, &octDown, &octUp, &roll, &badge, &dragMidi, &dragAudio, &soundBox, &volume, &presetBox, &heartButton })
        content.addAndMakeVisible (c);
    for (auto* arr : { &scaleChips, &barChips, &moodChips }) for (auto* c : *arr) content.addAndMakeVisible (c);
    content.addChildComponent (panel);

    layout();
    addInfo (240, 76, TK_FR ("Clique la note de ton morceau.||NOTES : Do = C, Do# = C#, Ré = D, Ré# = D#, Mi = E, Fa = F, Fa# = F#, Sol = G, Sol# = G#, La = A, La# = A#, Si = B.|Clique la note de ton morceau sur la roue, puis la gamme. Majeur = joyeux, Mineur = triste ou sombre.|Tu ne la connais pas ? « Écouter mes pistes » la trouve pour toi."), TK_FR ("Tonalité"));
    addInfo (284, 330, TK_FR ("La couleur des accords.||Majeur = lumineux, Mineur = triste ou sombre, Dorien = mélancolique groovy, Phrygien = oriental, Mixolydien = funky. Mineur harm. = mineur avec une tension orientale / classique."), "Gamme");
    addInfo (744, 76, TK_FR ("4 ou 8 mesures.||En 8 mesures, la deuxième moitié répond à la première et finit souvent sur une tension pour relancer la boucle."), "Mesures");
    addInfo (750, 136, TK_FR ("Le style des accords.||L'ambiance change les enchaînements ET le rythme : Trap ajoute des coups courts, R&B coupe les mesures en deux, Rêveur garde des accords longs."), "Ambiance");
    addInfo (804, 277, TK_FR ("Branche ta piste, joue-la, il trouve des accords qui vont avec.||EN MIDI (le plus fiable) : sur la piste Supernova, règle « MIDI From » sur ta piste de piano ou de synthé.|EN AUDIO : dans le menu Sidechain en haut du plugin (Ableton), choisis ta piste. Marche sur un sample, moins précis avec batterie ou grosse basse.|ENSUITE : lance la lecture, joue 4 ou 8 mesures, il trouve la tonalité et les accords. Clique « Générer une progression qui matche » : nouvelle progression dans la même tonalité, sans copier la tienne."), TK_FR ("Écouter mes pistes"));
    addInfo (812, 384, TK_FR ("Corrige les notes à la main.||Clic dans le vide = ajouter une note. Glisser dans le vide = sélection multiple. Clic sur une note = la sélectionner, re-clic = l'effacer. Bord droit = allonger. Suppr = effacer la sélection. Cmd + A = tout sélectionner. Cmd + C / Cmd + V = copier / coller (là où est la souris, ou là où tu as cliqué en dernier ; ça marche aussi de Supernova vers Fusion). Cmd + X = couper. Cmd + D = dupliquer la sélection juste après. Cmd + Z = annuler, Cmd + Maj + Z = refaire (clique d'abord dans la grille).|OCTAVE − / + : toute la grille descend ou monte d'une octave.|Les lignes allumées sont les notes de ta gamme (la plus claire = la tonique).|LECTURE : pas de bouton Écouter. Supernova joue avec Ableton (barre d'espace) et recommence pile au début de ta boucle. Clique l'étoile pendant la lecture : la nouvelle progression prend le relais.|GARDER DES ACCORDS : sélectionne des notes (clic, Maj + clic ou glisse dans le vide), puis clique l'étoile. Elles restent à leur place, et le début ou la suite est regénéré pour bien s'enchaîner avec elles. Reclique l'étoile pour d'autres idées.|↶ REVENIR (sous l'étoile) : remet la progression d'avant.|GLISSER : fais glisser le MIDI sur une piste MIDI, ou l'audio sur une piste audio."), "Grille MIDI");
    addInfo (428, 316, TK_FR ("Le son des accords.||Choisis le son joué par Supernova. « Muet (MIDI seul) » : Supernova ne fait pas de son.|JOUER TES ACCORDS AVEC MILKY WAY OU FUSION : rien à brancher. Dans Milky Way, active « Jouer mes accords ». Dans Fusion, active « Suivre mes accords ». Ils prennent automatiquement les accords de cette grille, et suivent chaque changement."), "Son");
    finishSetup (31, { { 262, 52, 26 }, { 640, 50, 30 } });
    refreshFromState();
    startTimerHz (30);
}

AccordsEditor::~AccordsEditor() { stopTimer(); audioProcessor.stopListening(); }

void AccordsEditor::layout()
{
    // left: blue crystal panel (key + scale)
    keyWheel.setBounds (70, 98, 206, 206);
    for (int i = 0; i < scaleChips.size(); ++i) scaleChips[i]->setBounds (60 + (i % 3) * 76, 314 + (i / 3) * 34, 74, 28);
    // centre: the star (the button), sound, volume, undo
    star.setBounds (285, -22, 310, 310);
    undoB.setBounds (400, 282, 80, 22);
    soundBox.setBounds (330, 336, 160, 26);
    volume.setBounds (498, 312, 66, 70);
    // right: golden panel (bars, mood, 7th, listen, presets)
    barChips[0]->setBounds (598, 98, 106, 28); barChips[1]->setBounds (710, 98, 106, 28);
    for (int i = 0; i < moodChips.size(); ++i) moodChips[i]->setBounds (594 + (i % 3) * 76, 158 + (i / 3) * 34, 74, 28);
    seventh.setBounds (598, 234, 218, 28);
    listen.setBounds (598, 270, 200, 32);
    heartButton.setBounds (598, 314, 26, 26);
    presetBox.setBounds (630, 314, 186, 26);
    // the chords
    roll.setBounds (48, 404, 784, 144);
    // pearl MIDI band
    badge.setBounds (132, 561, 176, 38);
    octDown.setBounds (380, 567, 28, 26); octUp.setBounds (446, 567, 28, 26);
    dragMidi.setBounds (494, 563, 150, 34);
    dragAudio.setBounds (652, 563, 150, 34);
    panel.setBounds (300, 60, 340, 330);
    srcMidi.setBounds (18, 50, 140, 30); srcAudio.setBounds (166, 50, 156, 30);
    genMatch.setBounds (18, 236, 304, 34); closeListen.setBounds (18, 280, 304, 32);
}

// texts that change (drawn above the cached background)
void AccordsEditor::overContent (juce::Graphics& g)
{
    if (roll.hasSelection())
    { g.setColour (gold); g.setFont (LNF::font (11.5f)); g.drawText (TK_FR ("★ L'étoile garde tes notes sélectionnées et refait le reste"), 48, 386, 784, 16, juce::Justification::centred); }
    { const int o = audioProcessor.getInt ("oct", 0); g.setColour (juce::Colour (0xff2a1450)); g.setFont (LNF::font (14.0f)); g.drawText (o > 0 ? "+" + juce::String (o) : juce::String (o), 408, 568, 38, 24, juce::Justification::centred); }
}

void AccordsEditor::paintContent (juce::Graphics& g)
{
    namespace sh = tchk::shapes;
    sky.drawStaticStars (g);
    const juce::Colour blue (0xff5aa8ff), amber (0xffffb03d);
    sh::drawFacetPanel (g, { 44, 70, 258, 324 }, blue, 22.0f, 3);
    sh::drawFacetPanel (g, { 578, 70, 258, 324 }, amber, 22.0f, 8);
    sh::drawFacetPanel (g, { 318, 310, 252, 74 }, juce::Colour (0xffd86bff), 14.0f, 5, 0.6f);
    sh::drawPearlBand (g, { 64, 556, 752, 48 }, gold);
    sh::drawCrystalFrame (g, { 0, 0, 880, 652 }, 20251127, 26.0f);
    // titles of the panels
    auto tag = [&] (const juce::String& txt, juce::Rectangle<int> r, juce::Colour c)
    { g.setColour (c); g.setFont (LNF::font (12.0f)); g.drawText (txt, r, juce::Justification::centred); };
    tag (TK_FR ("TONALITÉ · GAMME"), { 44, 76, 258, 18 }, blue.brighter (0.4f));
    tag ("MESURES", { 578, 76, 258, 18 }, amber.brighter (0.3f));
    tag ("AMBIANCE", { 578, 136, 258, 18 }, amber.brighter (0.3f));
    tag ("SON", { 330, 318, 160, 16 }, LNF::textDim);
    // the name, engraved under the crystal, top left
    g.setColour (juce::Colour (0xffffe9a0)); g.setFont (LNF::font (10.0f));
    g.drawText ("T C H I S K I B I D I", 50, 36, 200, 12, juce::Justification::centredLeft);
    g.setColour (gold); g.setFont (LNF::font (26.0f));
    g.drawText ("SUPERNOVA", 48, 44, 260, 26, juce::Justification::centredLeft);
    // pearl band labels
    g.setColour (juce::Colour (0xff2a1450)); g.setFont (LNF::font (20.0f));
    g.drawText ("MIDI", 82, 565, 50, 30, juce::Justification::centredLeft);
    g.setFont (LNF::font (10.5f)); g.drawText ("OCTAVE", 320, 571, 60, 18, juce::Justification::centredLeft);
}

void AccordsEditor::pushHistory()
{
    audioProcessor.pushUndo();
    undoB.setEnabled (true);
}

namespace { const juce::StringArray undoIds { "notes", "root", "scale", "bars", "mood", "seventh", "oct" }; }

void AccordsEditor::doUndo()
{
    if (audioProcessor.undoStep (undoIds)) { star.pop(); refreshFromState(); }
    undoB.setEnabled (audioProcessor.canUndo());
}

void AccordsEditor::doRedo()
{
    if (audioProcessor.redoStep (undoIds)) { star.pop(); refreshFromState(); }
    undoB.setEnabled (audioProcessor.canUndo());
}

// the whole grid one octave lower / higher
void AccordsEditor::shiftOctave (int d)
{
    auto notes = audioProcessor.getNotes();
    if (! tchk::shiftOctave (notes, d, 24, 100)) return;
    pushHistory();
    audioProcessor.setInt ("oct", audioProcessor.getInt ("oct", 0) + d);
    audioProcessor.setNotes (notes);
    for (auto& n : notes) { audioProcessor.preview (n.pitch, 0.7f, 250); if (&n - notes.data() >= 2) break; }
    refreshFromState();
}

void AccordsEditor::regenerate (const std::vector<int>* avoid, bool keepHistory)
{
    if (keepHistory) pushHistory();
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
    roll.setNotesKeepSelection (audioProcessor.getNotes());
    roll.totalSteps = audioProcessor.totalSteps();
    roll.scaleRoot = audioProcessor.getInt ("root", 9); roll.scaleIdx = sc;
    roll.fit();
    undoB.setEnabled (audioProcessor.canUndo());
    roll.repaint();
    content.repaint();
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
    audioProcessor.flushDiagnostics();
    if (roll.hasSelection() != lastHadSel) { lastHadSel = roll.hasSelection(); content.repaint(); }
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

