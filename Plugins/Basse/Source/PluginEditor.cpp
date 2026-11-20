#include "PluginEditor.h"

using tchk::LNF;
namespace music = tchk::music;

namespace
{
    constexpr int kW = 880, kH = 652;
    const int styleSound[] = { 0, 3, 7, 14, 9, 6, 15, 10 }; // a sound that suits each style
}

BasseEditor::BasseEditor (BasseProcessor& p)
    : tchk::ScaledEditor (p, kW, kH, 0.9f, "basse"),
      audioProcessor (p),
      keyWheel (music::noteNames(), pink),
      soundBox (p.apvts, "sound"),
      lengthK (p.apvts, "length", "Longueur", pink, true), glideK (p.apvts, "glide", "Glide", pink, true), subK (p.apvts, "sub", "Sub", pink, true),
      distK (p.apvts, "dist", "Grain", pink, true), slidesK (p.apvts, "slides", "Glissandos", pink, true),
      dragMidi ("Glisser le MIDI", [this] { return tchk::exportfiles::writeMidi (audioProcessor.getNotes(), audioProcessor.player.bpm, "Tchiskibidi Basse"); }),
      dragAudio (TK_FR ("Glisser l'audio"), [this] { return tchk::exportfiles::writeWav (audioProcessor.getNotes(), audioProcessor.totalSteps(), audioProcessor.player.bpm, audioProcessor.exportPatch(), "Tchiskibidi Basse"); })
{
    content.paintFn = [this] (juce::Graphics& g) { paintContent (g); };
    content.overFn = [this] (juce::Graphics& g) { overContent (g); };
    content.layoutFn = [this] { layout(); };
    levelDb = [&p] { return p.peakDb.load(); };

    auto stopFollow = [this] { audioProcessor.setInt ("follow", 0); };
    keyWheel.onPick = [this, stopFollow] (int i) { pushHistory(); stopFollow(); audioProcessor.setInt ("root", i); regenerateNoHistory(); };
    for (int i = 0; i < music::scaleNames().size(); ++i)
    {
        auto* c = scaleChips.add (new tchk::Chip (music::scaleNames()[i], pink)); c->fontSize = 11.5f;
        c->onClick = [this, i, stopFollow] { pushHistory(); stopFollow(); audioProcessor.setInt ("scale", i); regenerateNoHistory(); };
    }
    for (int i = 0; i < tchk::gen::bassStyles().size(); ++i)
    {
        auto* c = styleChips.add (new tchk::Chip (tchk::gen::bassStyles()[i], pink)); c->fontSize = 12.0f;
        c->onClick = [this, i]
        {
            pushHistory();
            audioProcessor.setInt ("style", i);
            if (auto* prm = audioProcessor.apvts.getParameter ("sound")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) styleSound[i]));
            regenerateNoHistory();
        };
    }
    for (int b : { 4, 8 })
    {
        auto* c = barChips.add (new tchk::Chip (juce::String (b) + " MESURES", pink));
        c->onClick = [this, b, stopFollow] { pushHistory(); stopFollow(); audioProcessor.setInt ("bars", b); regenerateNoHistory(); };
    }
    follow.outline = true;
    follow.onClick = [this] { pushHistory(); audioProcessor.setInt ("follow", audioProcessor.getInt ("follow", 0) ? 0 : 1); refreshFromState(); };
    prevB.onClick = [this] { stepSound (-1); };
    nextB.onClick = [this] { stepSound (1); };
    hole.accent = pink;
    hole.onClick = [this] { regenerate(); };
    for (auto* k : { &lengthK, &glideK, &subK, &distK, &slidesK }) k->asOrbit();
    undoB.fontSize = 10.5f; undoB.setEnabled (false);
    undoB.onClick = [this] { doUndo(); };
    octDown.onClick = [this] { shiftOctave (-1); };
    octUp.onClick = [this] { shiftOctave (1); };

    roll.accent = pink;
    roll.glideMode = true;
    roll.newLength = 2;
    roll.onEdit = [this]
    {
        if (tchk::notesToString (roll.notes) == audioProcessor.apvts.state.getProperty ("notes").toString()) return;   // just a selection
        pushHistory();
        audioProcessor.setNotes (roll.notes);
    };
    roll.onUndo = [this] { doUndo(); };
    roll.onRedo = [this] { doRedo(); };
    roll.onAudition = [this] (int pitch) { if (! audioProcessor.player.playing) audioProcessor.preview (pitch, 0.9f, 260); };
    roll.getLabels = [this] { return chordLabels(); };
    roll.getPlayStep = [this] { return audioProcessor.player.playStep.load(); };
    badge.isPlaying = [this] { return audioProcessor.player.playing.load(); };

    presets = std::make_unique<tchk::PresetSystem> (p, "Basse", presetBox, heartButton, nullptr, nullptr, TK_FR ("♥ MES BASSES"), "Mes basses");

    for (juce::Component* c : std::initializer_list<juce::Component*> { &keyWheel, &follow, &prevB, &nextB, &hole, &soundBox, &lengthK, &glideK, &subK, &distK, &slidesK, &undoB, &octDown, &octUp, &roll, &badge, &dragMidi, &dragAudio, &presetBox, &heartButton })
        content.addAndMakeVisible (c);
    for (auto* arr : { &scaleChips, &styleChips, &barChips }) for (auto* c : *arr) content.addAndMakeVisible (c);

    layout();
    addInfo (112, 64, TK_FR ("Clique la note de ton morceau.||NOTES : Do = C, Do# = C#, Ré = D, Ré# = D#, Mi = E, Fa = F, Fa# = F#, Sol = G, Sol# = G#, La = A, La# = A#, Si = B.|La basse est construite sur cette note et cette gamme. Avec « Suivre mes accords », elles se règlent toutes seules."), TK_FR ("Tonalité"));
    addInfo (86, 345, TK_FR ("La couleur de la basse.||Majeur = lumineux, Mineur = triste ou sombre, Dorien = mélancolique groovy, Phrygien = oriental, Mixolydien = funky."), "Gamme");
    addInfo (708, 64, TK_FR ("Le rythme de la basse.||TRAP : longues 808 avec sauts d'octave.|DRILL : 808 qui glissent d'une note à l'autre.|R&B : basse souple qui suit les accords.|AFRO / REGGAETON : rythmes qui dansent.|BOOM BAP : lourd et posé.|HOUSE : notes entre les kicks.|FUNK : notes courtes, octaves qui sautent.|Changer de style choisit aussi un son qui va bien avec."), "Style");
    addInfo (726, 196, TK_FR ("4 ou 8 mesures.||Longueur de la ligne de basse. Avec « Suivre mes accords », elle prend la longueur que tu choisis ici."), "Mesures");
    addInfo (844, 290, TK_FR ("La basse se cale sur tes accords, rien à brancher.||AVEC TCHISKIBIDI ACCORDS : active ce bouton, c'est tout. La basse prend tout de suite la tonalité et les accords de la grille d'Accords, et se refait à chaque fois que tu changes tes accords.|AVEC UN AUTRE INSTRUMENT : envoie ses notes MIDI sur cette piste et lance la lecture. Après un tour de boucle, la basse les suit."), TK_FR ("Suivre mes accords"));
    addInfo (580, 278, TK_FR ("Le son de la basse.||Flèches = son suivant / précédent, ou choisis dans la liste.|808 : la note tient puis s'éteint toute seule, parfaite en trap / drill.|SUB : son continu et rond.|PLUCK / FUNK / HOUSE : notes courtes et rebondissantes.|LONGUEUR = durée des notes. GLIDE = vitesse du glissé. SUB = une couche d'infrabasse une octave en dessous (à sentir plus qu'à entendre). GRAIN = harmoniques qui rendent la basse audible même sur un petit haut-parleur. GLISSANDOS = plus de notes qui glissent (et des sauts d'octave à fond) à la prochaine génération.|↶ REVENIR : remet la ligne de basse d'avant."), "Son");
    addInfo (472, 610, TK_FR ("Corrige la basse à la main.||Clic dans le vide = ajouter une note. Glisser une note = la déplacer. Bord droit = allonger. Re-clic sur une note sélectionnée = l'effacer. Suppr = effacer.|CMD + D = dupliquer la sélection juste après. CMD + Z = annuler, CMD + MAJ + Z = refaire (clique d'abord dans la grille).|OCTAVE − / + : toute la basse descend ou monte d'une octave (gardé pour les basses suivantes).|Les lignes allumées sont les notes de ta gamme (la plus claire = la tonique).|CLIC DROIT SUR UNE NOTE : glissé (la 808 glisse depuis la note d'avant, la courbe en pointillés le montre).|LECTURE : pas de bouton Écouter, la basse joue avec Ableton (barre d'espace) et recommence au début de ta boucle."), "Grille MIDI");
    finishSetup (59, { { 250, 14, 40 }, { 330, 32, 28 }, { 560, 12, 36 } });
    refreshFromState();
    startTimerHz (30);
}

BasseEditor::~BasseEditor() { stopTimer(); }

void BasseEditor::layout()
{
    keyWheel.setBounds (34, 84, 250, 250);
    const int sw[] = { 74, 70, 70, 82, 108, 94 };
    int x = 110, y = 340;
    for (int i = 0; i < scaleChips.size(); ++i)
    {
        if (i == 3) { x = 24; y = 372; }
        scaleChips[i]->setBounds (x, y, sw[i], 28); x += sw[i] + 4;
    }
    hole.setBounds (330, 48, 240, 196);
    prevB.setBounds (330, 272, 34, 30); nextB.setBounds (536, 272, 34, 30);
    soundBox.setBounds (368, 274, 164, 26);
    { int kx = 318; for (auto* k : { &lengthK, &glideK, &subK, &distK, &slidesK }) { k->setBounds (kx, 318, 64, 76); kx += 64; } }
    undoB.setBounds (524, 244, 80, 22);
    const int stw[] = { 62, 64, 58, 62, 92, 104, 72, 66 };
    int sx = 650, sy = 84;
    for (int i = 0; i < styleChips.size(); ++i)
    {
        if (sx + stw[i] > 866) { sx = 650; sy += 34; }
        styleChips[i]->setBounds (sx, sy, stw[i], 30); sx += stw[i] + 4;
    }
    barChips[0]->setBounds (650, 216, 104, 30); barChips[1]->setBounds (760, 216, 104, 30);
    follow.setBounds (650, 256, 214, 30);
    heartButton.setBounds (650, 332, 26, 26);
    presetBox.setBounds (682, 332, 182, 26);
    octDown.setBounds (96, 406, 32, 26); octUp.setBounds (164, 406, 32, 26);
    roll.setBounds (20, 440, 840, 152);
    badge.setBounds (20, 602, 190, 38);
    dragMidi.setBounds (520, 603, 160, 36);
    dragAudio.setBounds (690, 603, 170, 36);
}

void BasseEditor::paintContent (juce::Graphics& g)
{
    paintBackgroundAndHeader (g, "Basse", {});
    auto dim = [&] (const juce::String& txt, int x, int y, int w = 200, float size = 12.0f, bool bold = true, juce::Justification j = juce::Justification::centredLeft)
    { g.setColour (LNF::textDim); g.setFont (LNF::font (size, bold)); g.drawText (txt, x, y, w, 18, j); };
    dim (TK_FR ("TONALITÉ"), 24, 64);
    dim ("GAMME", 24, 345, 80, 11.0f);
    dim ("STYLE", 650, 64);
    dim ("MESURES", 650, 196);
    dim ("OCTAVE", 24, 410, 70, 11.0f);
    { const int o = audioProcessor.getInt ("oct", 0); g.setColour (o != 0 ? pink : LNF::textDim); g.setFont (LNF::font (13.0f)); g.drawText (o > 0 ? "+" + juce::String (o) : juce::String (o), 128, 410, 36, 18, juce::Justification::centred); }
    dim (TK_FR ("Grille MIDI : modifie la basse à la main"), 222, 610, 250, 11.0f, false);
    g.setColour (pink); g.setFont (LNF::font (12.0f));
    g.drawText ("NOUVELLE BASSE", 330, 246, 190, 18, juce::Justification::centred);
}

void BasseEditor::overContent (juce::Graphics& g)
{
    g.setColour (LNF::textDim); g.setFont (LNF::font (10.5f, false));
    const bool on = audioProcessor.getInt ("follow", 0) != 0;
    juce::String msg = ! on ? TK_FR ("La basse joue sur tes accords.")
                          : (audioProcessor.followText.isNotEmpty() ? TK_FR ("Suit : ") + audioProcessor.followText : TK_FR ("J'attends tes accords (ouvre Tchiskibidi Accords, ou lance la lecture)..."));
    g.drawFittedText (msg, 650, 288, 190, 32, juce::Justification::centredLeft, 2);
}

void BasseEditor::stepSound (int d)
{
    if (auto* prm = audioProcessor.apvts.getParameter ("sound"))
    {
        const int n = tchk::sounds::bassSoundNames().size();
        const int cur = (int) audioProcessor.apvts.getRawParameterValue ("sound")->load();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) ((cur + d + n) % n)));
    }
    if (! audioProcessor.player.playing) audioProcessor.preview (tchk::gen::bassPitch (audioProcessor.getInt ("root", 9), audioProcessor.getInt ("scale", 1), 0, 0), 0.9f, 600);
}

void BasseEditor::pushHistory()
{
    audioProcessor.pushUndo();
    undoB.setEnabled (true);
}

namespace { const juce::StringArray undoIds { "notes", "chords", "root", "scale", "bars", "style", "follow", "oct" }; }

void BasseEditor::doUndo()
{
    if (audioProcessor.undoStep (undoIds)) { hole.wake(); refreshFromState(); }
    undoB.setEnabled (audioProcessor.canUndo());
}

void BasseEditor::doRedo()
{
    if (audioProcessor.redoStep (undoIds)) { hole.wake(); refreshFromState(); }
    undoB.setEnabled (audioProcessor.canUndo());
}

// the whole bass line one octave lower / higher
void BasseEditor::shiftOctave (int d)
{
    auto notes = audioProcessor.getNotes();
    if (! tchk::shiftOctave (notes, d, 12, 84)) return;
    pushHistory();
    audioProcessor.setInt ("oct", audioProcessor.getInt ("oct", 0) + d);
    audioProcessor.setNotes (notes);
    if (! audioProcessor.player.playing && ! notes.empty()) audioProcessor.preview (notes.front().pitch, 0.9f, 400);
    refreshFromState();
}

void BasseEditor::regenerate()
{
    pushHistory();
    regenerateNoHistory();
}

void BasseEditor::regenerateNoHistory()
{
    if (audioProcessor.getInt ("follow", 0) != 0)
    {
        auto ch = audioProcessor.getChords();
        audioProcessor.generate (&ch);   // keep following the heard chords, new rhythm
    }
    else audioProcessor.generate();
    hole.wake();
    refreshFromState();
}

void BasseEditor::refreshFromState()
{
    keyWheel.setSelected (audioProcessor.getInt ("root", 9));
    const int sc = audioProcessor.getInt ("scale", 1), bars = audioProcessor.getInt ("bars", 4), style = audioProcessor.getInt ("style", 0);
    for (int i = 0; i < scaleChips.size(); ++i) scaleChips[i]->setToggleState (i == sc, juce::dontSendNotification);
    for (int i = 0; i < styleChips.size(); ++i) styleChips[i]->setToggleState (i == style, juce::dontSendNotification);
    barChips[0]->setToggleState (bars == 4, juce::dontSendNotification); barChips[1]->setToggleState (bars == 8, juce::dontSendNotification);
    follow.setToggleState (audioProcessor.getInt ("follow", 0) != 0, juce::dontSendNotification);
    roll.notes = audioProcessor.getNotes();
    roll.totalSteps = audioProcessor.totalSteps();
    roll.scaleRoot = audioProcessor.getInt ("root", 9); roll.scaleIdx = sc;
    undoB.setEnabled (audioProcessor.canUndo());
    roll.fit();
    roll.repaint();
    content.repaint();
    lastVersion = audioProcessor.stateVersion.load();
}

std::vector<std::tuple<int, int, juce::String>> BasseEditor::chordLabels() const
{
    std::vector<std::tuple<int, int, juce::String>> out;
    const int root = audioProcessor.getInt ("root", 9), sc = audioProcessor.getInt ("scale", 1);
    for (auto& c : audioProcessor.getChords())
    {
        std::vector<int> tones { music::degreePitch (root, sc, c.degree, 48), music::degreePitch (root, sc, c.degree + 2, 48), music::degreePitch (root, sc, c.degree + 4, 48) };
        out.push_back ({ c.start, c.length, music::chordName (tones) });
    }
    return out;
}

void BasseEditor::timerCallback()
{
    audioProcessor.flushDiagnostics();
    if (audioProcessor.stateVersion.load() != lastVersion) refreshFromState();
    const int played = audioProcessor.notesPlayed.load();
    if (played != lastPlayed) { hole.pulse (1.0f); lastPlayed = played; }
    if (audioProcessor.player.playing.load()) roll.repaint();
    badge.repaint();
}
