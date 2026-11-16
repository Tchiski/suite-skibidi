#pragma once

// Sound banks of the Tchiskibidi instruments: 152 Synthé presets in 8 families,
// the Accords sounds and the Basse sounds (808, subs, plucked basses).

#include "TchiskibidiInstrument.h"

namespace tchk
{
namespace sounds
{
inline const juce::StringArray& categories()
{
    static const juce::StringArray c { "Pads", "Plucks", "Leads", "Cordes", "Basses", "Keys", "Cloches", "FX", "Frutiger Aero", "Botanica", "Chorale" };
    return c;
}
inline const juce::StringArray& names (int cat)
{
    cat = juce::jlimit (0, 10, cat);
    static const std::vector<juce::StringArray> n {
        { TK_FR ("Nuage d'Opale"), "Aube Lente", "Plaine des Songes", "Velours Cosmique", "Brume Rose", TK_FR ("Océan Calme"), TK_FR ("Cathédrale"), "Lumière Douce", "Vent Solaire", "Glace Chaude",
          TK_FR ("Rêve Lucide"), "Infini", "Soie Lunaire", "Nappe d'Ambre", "Halo", TK_FR ("Marée Lente"), "Pad Choral", "Givre", "Nostalgie" },
        { "Goutte de Pluie", "Kalimba Lunaire", "Pluck Trap", "Bulle", "Harpe de Verre", TK_FR ("Pizz Néon"), "Clochette", "Marimba Douce", "Plume", "Pluck Afro",
          "Koto Rose", "Pop Pop", "Pluck Lo-fi", "Guitare Rêve", "Pluck House", TK_FR ("Étincelle"), "Pluck Drill", "Sitar Cosmique", "Picotis" },
        { "Laser Rose", "Sifflet Alien", "Lead Rap", "Saw Royale", TK_FR ("Flûte Comète"), "Synthwave", "Chiptune", TK_FR ("Cri de Néon"), "Lead Doux", "Hyper Lead",
          "Sinus Star", "Bzzz", "Lead Drill", TK_FR ("Sifflé"), "Lead Afro", "Lead Acide", "Lead Mono", TK_FR ("Voix Robot"), "Lead Rave" },
        { "Violons du Soir", TK_FR ("Cordes Cinéma"), "Pizzicato", "Violoncelle", "Cordes Lo-fi", "Ensemble Lent", TK_FR ("Staccato Épique"), "Cordes Soul", TK_FR ("Trémolo"), TK_FR ("Orchestre Rêvé"),
          "Alto Chaud", "Cordes 80s", "Quatuor", "Cordes Trap", "Cordes Sombres", "Cordes Brillantes", TK_FR ("Arco Lunaire"), "Cordes Drill", "Cordes Vintage" },
        { "808 Profonde", "Sub Rond", "Reese", "Basse Funk", "Wobble", "Basse Pluck", "Moog Chaud", "Basse Acid", "Sub Sinus", "Basse Afro",
          "Grrr", "Slide 808", "Basse House", "Basse Synthwave", "Sub Doux", "Basse Saturée", "Basse Rebond", "Basse Feutre", "Basse Drill" },
        { "Piano Doux", "Rhodes Lune", "Wurli", "Orgue Soul", "Piano Lo-fi", "Clavinet", "Keys 80s", "Piano Feutre", "Orgue Gospel", "Keys Rêve",
          "Piano Cristal", "Rhodes Trap", TK_FR ("Keys Vitrées"), "Orgue Cosmique", "Toy Piano", "Keys Soul", "Keys Brumeuses", "Piano Pop", "Keys Lounge" },
        { "Cloche de Verre", "Carillon", "Bol Tibétain", TK_FR ("Cloche Étoile"), "Gamelan", "Vibraphone", "Glockenspiel", "Cloche FM", "Boîte à Musique", "Cloche Rose",
          "Cloche Lointaine", "Tubular", "Cloche Lo-fi", "Cloche Trap", "Steel Drum", "Cloche Givrée", "Cloche Douce", "Cloche Brillante", TK_FR ("Cloche Rêve") },
        { "Montée Cosmique", TK_FR ("Descente Spatiale"), "Bruit Blanc", "Vent Stellaire", "Sonar", "Laser", "Glitch", TK_FR ("Météore"), "Radio Lointaine", "Ovni",
          "Scintille", "Respiration", TK_FR ("Tempête"), "Trou Noir", "Signal", "Pluie d'Étoiles", "Ressort", TK_FR ("Écho Spatial"), "Impact" },
        { "Bulle d'Eau", "Aqua Glass", "Ciel Vista", "Dauphin Chrome", "Lagune Claire", TK_FR ("Verre Soufflé"), "Aurore Aero", "Gouttelette", "Nuage Lisse", "Cristal Bleu",
          TK_FR ("Écran de Veille"), "Mer de Pixels", "Brise Glossy", TK_FR ("Arc-en-Ciel Doux"), TK_FR ("Poisson Lumière"), "Matin Tranquille" },
        { TK_FR ("Forêt Humide"), TK_FR ("Flûte de Bambou"), "Mousse Douce", "Marimba Bois", TK_FR ("Feuille Tombée"), TK_FR ("Rosée"), "Kalimba Racine", TK_FR ("Serre Tropicale"), TK_FR ("Pluie sur Fougère"), TK_FR ("Sève"),
          "Ocarina Lunaire", "Jardin Secret", "Bois Creux", "Pollen", "Lianes", TK_FR ("Clairière") },
        { TK_FR ("Chœur Céleste"), "Aah Doux", "Ooh Lointain", TK_FR ("Chœur Sombre"), TK_FR ("Voix d'Ange"), TK_FR ("Cathédrale Vocale"), TK_FR ("Chœur Rêve"), "Moines", "Soprano Brume", TK_FR ("Chœur Synthé"),
          TK_FR ("Voix Éthérée"), TK_FR ("Prière"), TK_FR ("Chœur Lunaire"), "Hymne", "Murmure", "Chœur Gospel" } };
    return n[(size_t) cat];
}
inline int numCategories() { return categories().size(); }
inline int count() { int t = 0; for (int c = 0; c < numCategories(); ++c) t += names (c).size(); return t; }
inline std::pair<int, int> split (int index)
{
    const int nc = numCategories();
    for (int c = 0; c < nc; ++c) { if (index < names (c).size()) return { c, index }; index -= names (c).size(); }
    return { nc - 1, names (nc - 1).size() - 1 };
}
inline int join (int cat, int idx) { int t = 0; for (int c = 0; c < cat; ++c) t += names (c).size(); return t + idx; }

// Each preset = its family's base sound, gently reshaped by its own number.
// The ranges are kept soft: no harsh resonance, little drive, warm filters, chorus and space.
inline Patch makeSynthPatch (int index)
{
    const auto [cat, i] = split (index);
    juce::Random r ((juce::int64) (index * 7919 + 13));
    auto v = [&] (float a, float b) { return a + r.nextFloat() * (b - a); };
    Patch p;
    p.drive = 0; p.reso = 0.1f;
    switch (cat)
    {
        case 0: // pads: slow, wide, soft
            p.saw = v (0.15f, 0.55f); p.tri = v (0.3f, 0.8f); p.sine = v (0.2f, 0.6f); p.square = 0;
            p.unison = 3 + r.nextInt (3); p.detune = v (5, 13); p.cutoff = v (900, 2600); p.reso = v (0.05f, 0.18f); p.fenv = v (0.3f, 1.0f);
            p.fA = v (0.6f, 2.5f); p.fD = v (1.5f, 3.5f); p.fS = v (0.4f, 0.8f); p.fR = v (1.5f, 3);
            p.A = v (0.6f, 2.0f); p.D = 1.2f; p.S = v (0.8f, 1); p.R = v (1.8f, 4.0f);
            p.lfoRate = v (0.08f, 0.5f); p.lfoFilter = v (0.1f, 0.45f); p.vibrato = v (0, 0.04f); p.space = v (0.55f, 0.9f); p.width = v (0.8f, 1); p.chorus = v (0.3f, 0.7f); p.level = 0.55f;
            break;
        case 1: // plucks: round, short, glassy
            p.tri = v (0.4f, 1); p.sine = v (0.3f, 0.8f); p.saw = v (0, 0.4f); p.square = r.nextFloat() < 0.3f ? v (0.1f, 0.3f) : 0;
            p.unison = 1 + r.nextInt (2); p.detune = v (3, 9); p.cutoff = v (600, 1600); p.reso = v (0.05f, 0.25f); p.fenv = v (2.0f, 3.5f);
            p.fA = 0.001f; p.fD = v (0.12f, 0.4f); p.fS = 0; p.fR = 0.2f; p.A = 0.002f; p.D = v (0.35f, 1.0f); p.S = v (0, 0.12f); p.R = v (0.3f, 0.8f);
            p.space = v (0.3f, 0.6f); p.width = v (0.4f, 0.8f); p.chorus = v (0.1f, 0.4f); p.level = 0.7f;
            break;
        case 2: // leads: singing, vibrato, not harsh
            p.saw = v (0.3f, 0.7f); p.square = v (0, 0.4f); p.sine = v (0.2f, 0.7f); p.tri = v (0, 0.4f);
            p.unison = r.nextFloat() < 0.5f ? 1 : 2 + r.nextInt (2); p.detune = v (4, 10); p.cutoff = v (1400, 4000); p.reso = v (0.05f, 0.25f); p.fenv = v (0.4f, 1.5f);
            p.fA = 0.004f; p.fD = v (0.3f, 0.9f); p.fS = v (0.4f, 0.8f); p.fR = 0.3f; p.A = v (0.005f, 0.06f); p.D = 0.4f; p.S = v (0.7f, 1); p.R = v (0.2f, 0.6f);
            p.vibrato = v (0.08f, 0.25f); p.lfoRate = v (4.8f, 6.0f); p.mono = r.nextFloat() < 0.6f; p.glide = p.mono ? v (0.03f, 0.1f) : 0; p.drive = v (0, 0.08f);
            p.space = v (0.2f, 0.45f); p.width = v (0.3f, 0.7f); p.chorus = v (0.1f, 0.4f); p.level = 0.5f;
            break;
        case 3: // strings: ensemble, slow bow, gentle vibrato
            p.saw = v (0.5f, 0.8f); p.tri = v (0.2f, 0.5f); p.unison = 4 + r.nextInt (3); p.detune = v (7, 14); p.cutoff = v (1500, 3800); p.reso = 0.08f; p.fenv = v (0.3f, 0.9f);
            p.fA = v (0.2f, 0.8f); p.fD = 1; p.fS = 0.7f; p.fR = 1;
            p.A = (i == 2 || i == 6 || i == 8) ? 0.005f : v (0.25f, 0.9f); p.D = (i == 2 || i == 6) ? 0.3f : 0.6f; p.S = (i == 2 || i == 6) ? 0.0f : 0.9f; p.R = v (0.6f, 1.8f);
            p.vibrato = v (0.04f, 0.12f); p.lfoRate = v (4.8f, 5.6f); p.space = v (0.45f, 0.8f); p.width = 1.0f; p.chorus = v (0.3f, 0.6f); p.level = 0.5f;
            break;
        case 4: // basses: deep and clean
            p.mono = true; p.octave = -2; p.glide = v (0.0f, 0.08f);
            if (i == 0 || i == 11 || i == 18) { p.sine = 1; p.saw = 0; p.pitchEnv = v (4, 9); p.pitchDecay = 0.04f; p.drive = v (0.05f, 0.2f); p.A = 0.003f; p.D = v (0.8f, 1.6f); p.S = 0; p.R = 0.2f; p.cutoff = 5000; p.fenv = 0; p.click = 0.15f; }
            else { p.saw = v (0.2f, 0.6f); p.sine = v (0.4f, 0.9f); p.tri = v (0, 0.4f); p.sub = v (0.3f, 0.6f); p.cutoff = v (300, 900); p.reso = v (0.05f, 0.25f); p.fenv = v (1, 2.5f);
                   p.fA = 0.001f; p.fD = v (0.15f, 0.5f); p.fS = v (0.1f, 0.4f); p.fR = 0.2f; p.A = 0.003f; p.D = 0.4f; p.S = v (0.6f, 1); p.R = 0.15f; p.drive = v (0, 0.12f);
                   if (i == 4) { p.lfoFilter = 0.9f; p.lfoRate = 3.0f; } }
            p.space = 0.03f; p.width = 0.2f; p.level = 0.8f;
            break;
        case 5: // keys: soft piano / electric piano / organ
            if (i == 3 || i == 8 || i == 13) { p.sine = 1; p.tri = 0.5f; p.saw = 0; p.square = 0.15f; p.cutoff = 2600; p.fenv = 0; p.A = 0.008f; p.D = 0.1f; p.S = 1; p.R = 0.12f; p.vibrato = 0.04f; p.lfoRate = 6; p.chorus = 0.5f; }
            else { p.sine = v (0.6f, 1); p.tri = v (0.3f, 0.8f); p.saw = v (0, 0.15f); p.cutoff = v (1000, 2600); p.fenv = v (1, 2.2f); p.fA = 0.001f; p.fD = v (0.4f, 1.2f); p.fS = 0.25f; p.fR = 0.6f;
                   p.A = 0.003f; p.D = v (1.2f, 2.8f); p.S = v (0.08f, 0.3f); p.R = v (0.35f, 0.8f); p.chorus = v (0.1f, 0.5f); }
            p.space = v (0.2f, 0.45f); p.width = v (0.4f, 0.7f); p.level = 0.6f;
            break;
        case 6: // bells: pure, long ringing
            p.sine = 1; p.tri = v (0.1f, 0.4f); p.square = v (0.0f, 0.12f); p.saw = 0; p.cutoff = v (3000, 7000); p.fenv = v (0.3f, 1.2f); p.fA = 0.001f; p.fD = 0.8f; p.fS = 0.3f; p.fR = 1;
            p.A = 0.001f; p.D = v (1.5f, 4); p.S = 0; p.R = v (1.5f, 3.5f); p.unison = 1 + r.nextInt (2); p.detune = v (2, 6); p.space = v (0.5f, 0.85f); p.width = v (0.6f, 1); p.chorus = v (0.1f, 0.4f); p.level = 0.55f;
            break;
        case 7: // fx: sweeps and textures, still musical
            p.saw = v (0, 0.5f); p.tri = v (0.2f, 0.7f); p.noise = v (0.05f, 0.35f); p.sine = v (0.1f, 0.5f); p.pitchEnv = v (-14, 14); p.pitchDecay = v (0.4f, 2.5f); p.cutoff = v (700, 4000); p.reso = v (0.1f, 0.4f);
            p.fenv = v (-1, 2.5f); p.fA = v (0.01f, 1.5f); p.fD = v (0.6f, 3); p.fS = v (0.2f, 0.6f); p.fR = 1.5f; p.A = v (0.01f, 1); p.D = v (0.6f, 2); p.S = v (0.3f, 0.8f); p.R = v (0.8f, 2.5f);
            p.lfoRate = v (0.2f, 6); p.lfoFilter = v (0.2f, 1.5f); p.vibrato = v (0, 0.6f); p.space = v (0.5f, 0.95f); p.width = 1; p.chorus = v (0.2f, 0.6f); p.level = 0.45f;
            break;
        case 8: // frutiger aero: glossy, bubbly, bright and clean
            p.sine = v (0.5f, 1); p.tri = v (0.3f, 0.8f); p.square = v (0, 0.15f); p.saw = 0; p.unison = 2 + r.nextInt (3); p.detune = v (4, 9);
            p.cutoff = v (2500, 6000); p.reso = v (0.05f, 0.18f); p.fenv = v (0.5f, 1.5f); p.fA = 0.001f; p.fD = v (0.3f, 0.9f); p.fS = 0.5f; p.fR = 0.8f;
            if (i % 2 == 0) { p.A = v (0.2f, 0.8f); p.D = 1; p.S = 0.85f; p.R = v (1.5f, 3); } else { p.A = 0.002f; p.D = v (0.5f, 1.4f); p.S = v (0, 0.2f); p.R = v (0.6f, 1.4f); p.pitchEnv = v (0.0f, 5.0f); p.pitchDecay = 0.03f; }
            p.lfoRate = v (0.2f, 1.2f); p.lfoFilter = v (0.05f, 0.3f); p.space = v (0.55f, 0.85f); p.width = 1; p.chorus = v (0.5f, 0.9f); p.level = 0.6f;
            break;
        case 9: // botanica: organic, woody, breathy, natural
            p.sine = v (0.5f, 1); p.tri = v (0.4f, 1); p.saw = 0; p.noise = v (0.02f, 0.12f); p.unison = 1 + r.nextInt (2); p.detune = v (2, 6);
            p.cutoff = v (800, 2200); p.reso = v (0.08f, 0.22f); p.fenv = v (0.8f, 2.0f); p.fA = v (0.001f, 0.08f); p.fD = v (0.2f, 0.7f); p.fS = 0.3f; p.fR = 0.5f;
            if (i == 1 || i == 10 || i == 13) { p.A = v (0.04f, 0.15f); p.D = 0.5f; p.S = 0.8f; p.R = v (0.4f, 0.9f); p.vibrato = v (0.06f, 0.15f); p.lfoRate = 5.0f; p.noise = v (0.08f, 0.18f); }
            else if (i % 3 == 0) { p.A = v (0.5f, 1.5f); p.D = 1.5f; p.S = 0.8f; p.R = v (1.5f, 3); p.lfoFilter = 0.3f; p.lfoRate = 0.2f; }
            else { p.A = 0.002f; p.D = v (0.4f, 1.1f); p.S = 0.05f; p.R = v (0.4f, 0.9f); }
            p.space = v (0.4f, 0.75f); p.width = v (0.5f, 0.9f); p.chorus = v (0.1f, 0.35f); p.level = 0.65f;
            break;
        default: // chorale: choir voices with formants
            p.saw = 1; p.tri = 0.3f; p.unison = 4 + r.nextInt (3); p.detune = v (6, 12); p.vowel = (float) (i % 5) * 0.8f + v (0.0f, 0.3f);
            p.cutoff = v (3500, 6000); p.reso = 0.05f; p.fenv = 0.2f; p.fA = 0.5f; p.fD = 1; p.fS = 0.8f; p.fR = 1.5f;
            p.A = v (0.3f, 1.2f); p.D = 1; p.S = 0.9f; p.R = v (1.2f, 2.8f); p.vibrato = v (0.05f, 0.12f); p.lfoRate = v (4.5f, 5.5f); p.noise = v (0.0f, 0.05f);
            p.space = v (0.6f, 0.9f); p.width = 1; p.chorus = v (0.4f, 0.8f); p.level = 0.8f;
            if (i == 3 || i == 7) p.octave = -1;
            break;
    }
    return p;
}

// fine settings / macros on top of a preset: every value at its "neutral" position keeps the preset as designed
struct Tweaks { float bright = 0.5f, space = 0.4f, motion = 0.3f, attack = 0.2f, release = 0.35f, width = 0.6f, warmth = 0.2f, vibrato = 0.1f, glide = 0.0f, filterEnv = 0.7f, reso = 0.2f, detune = 0.25f; int octave = 0; bool mono = false; float volume = 0.7f; };
inline Patch applyTweaks (Patch p, const Tweaks& t)
{
    auto f = [] (float v, float neutral, float range) { return std::pow (2.0f, (v - neutral) * range); };
    p.cutoff = juce::jlimit (40.0f, 18000.0f, p.cutoff * f (t.bright, 0.5f, 4.0f));
    p.space = juce::jlimit (0.0f, 1.0f, p.space * f (t.space, 0.4f, 2.5f) + (t.space - 0.4f) * 0.3f);
    p.lfoFilter += (t.motion - 0.3f) * 2.0f; p.lfoFilter = juce::jmax (0.0f, p.lfoFilter);
    p.A = juce::jlimit (0.001f, 4.0f, p.A * f (t.attack, 0.2f, 7.0f)); p.fA = juce::jlimit (0.001f, 4.0f, p.fA * f (t.attack, 0.2f, 5.0f));
    p.R = juce::jlimit (0.02f, 6.0f, p.R * f (t.release, 0.35f, 5.0f));
    p.width = juce::jlimit (0.0f, 1.0f, p.width + (t.width - 0.6f));
    p.drive = juce::jlimit (0.0f, 1.0f, p.drive + (t.warmth - 0.2f) * 0.8f);
    p.vibrato = juce::jmax (0.0f, p.vibrato + (t.vibrato - 0.1f) * 0.8f);
    if (t.glide > 0.001f) p.glide = t.glide * 0.4f;
    p.fenv *= f (t.filterEnv, 0.7f, 2.0f);
    p.reso = juce::jlimit (0.0f, 0.95f, p.reso + (t.reso - 0.2f));
    p.detune *= f (t.detune, 0.25f, 3.0f);
    p.octave += (float) t.octave;
    if (t.mono) p.mono = true;
    p.level *= t.volume / 0.7f;
    return p;
}

// Accords: the sound of the chords
inline const juce::StringArray& chordSoundNames() { static const juce::StringArray n { "Keys doux", "Pad", "Pluck", "Cordes", "Orgue", TK_FR ("Muet (MIDI seul)") }; return n; }
inline Patch chordPatch (int i)
{
    switch (i)
    {
        case 1: return makeSynthPatch (join (0, 0));
        case 2: return makeSynthPatch (join (1, 4));
        case 3: return makeSynthPatch (join (3, 0));
        case 4: return makeSynthPatch (join (5, 3));
        default: return makeSynthPatch (join (5, 0));
    }
}

// Basse: 24 sounds. Every one has a strong sine at the note (the sub you feel), the "Sub" knob adds a layer
// one octave below (infra-bass), "Distorsion" adds harmonics so the bass is heard on small speakers too.
inline const juce::StringArray& bassSoundNames()
{
    static const juce::StringArray n { "808 Profonde", "808 Courte", "808 Distordue", "808 Drill", "808 Punch", TK_FR ("808 Géante"), "Sub Rond", "Sub Doux",
                                       "Reese", "Moog Chaud", "Basse Funk", "Basse Pluck", "Acid", "Wobble", "Basse Afro", "Basse House",
                                       "808 Tsunami", "Sub Abyssal", "808 Glissade", TK_FR ("Basse Rêve"), "808 Cristal", "Sub Pulse", TK_FR ("Basse Néon"), TK_FR ("808 Montée") };
    return n;
}
inline Patch bassPatch (int i, float length, float glide, float dist, float subAmt = 0.4f)
{
    Patch p; p.mono = true; p.space = 0.0f; p.width = 0.1f; p.level = 0.95f; p.saw = 0; p.reso = 0.1f;
    const float lenK = 0.35f + length * 1.3f;
    auto eight08 = [&] (float dec, float drive, float clickAmt, float boom = 7.0f)
    { p.sine = 1; p.pitchEnv = boom; p.pitchDecay = 0.04f; p.A = 0.002f; p.D = dec * lenK; p.S = 0; p.R = 0.12f; p.cutoff = 9000; p.fenv = 0; p.drive = drive; p.click = clickAmt; };
    auto withBody = [&] (float sine) { p.sine = sine; };   // the sub you feel, under the synth tone
    switch (i)
    {
        case 0: eight08 (1.4f, 0.25f, 0.15f); break;
        case 1: eight08 (0.45f, 0.25f, 0.25f); break;
        case 2: eight08 (1.2f, 0.7f, 0.1f); break;
        case 3: eight08 (1.3f, 0.35f, 0.1f); break;
        case 4: eight08 (0.9f, 0.45f, 0.6f, 10.0f); break;
        case 5: eight08 (2.2f, 0.3f, 0.1f, 9.0f); break;
        case 6: p.sine = 1; p.A = 0.006f; p.D = 0.3f; p.S = 0.7f + 0.3f * length; p.R = 0.14f; p.cutoff = 9000; p.fenv = 0; p.drive = 0.12f; break;
        case 7: p.sine = 1; p.tri = 0.35f; p.A = 0.01f; p.D = 0.3f; p.S = 0.85f; p.R = 0.18f; p.cutoff = 600; p.fenv = 0.2f; break;
        case 8: withBody (0.8f); p.saw = 0.6f; p.unison = 3; p.detune = 16; p.cutoff = 800; p.reso = 0.2f; p.fenv = 0.5f; p.A = 0.005f; p.S = 0.9f; p.R = 0.15f; p.drive = 0.15f; break;
        case 9: withBody (0.7f); p.saw = 0.7f; p.cutoff = 600; p.reso = 0.3f; p.fenv = 1.8f; p.fA = 0.001f; p.fD = 0.25f; p.fS = 0.2f; p.drive = 0.25f; p.A = 0.003f; p.S = 0.9f; p.R = 0.12f; break;
        case 10: withBody (0.7f); p.square = 0.6f; p.cutoff = 900; p.fenv = 2.5f; p.fA = 0.001f; p.fD = 0.15f; p.fS = 0.1f; p.A = 0.002f; p.D = 0.3f * lenK; p.S = 0.15f; p.R = 0.08f; break;
        case 11: withBody (0.7f); p.saw = 0.6f; p.cutoff = 700; p.fenv = 3.5f; p.fA = 0.001f; p.fD = 0.12f; p.fS = 0; p.A = 0.002f; p.D = 0.28f * lenK; p.S = 0.05f; p.R = 0.08f; break;
        case 12: withBody (0.6f); p.saw = 0.7f; p.cutoff = 500; p.reso = 0.7f; p.fenv = 4.0f; p.fA = 0.001f; p.fD = 0.2f; p.fS = 0.05f; p.A = 0.002f; p.D = 0.35f * lenK; p.S = 0.3f; p.R = 0.08f; break;
        case 13: withBody (0.8f); p.saw = 0.6f; p.cutoff = 420; p.reso = 0.45f; p.lfoFilter = 1.5f; p.lfoRate = 4.0f; p.drive = 0.3f; p.A = 0.005f; p.S = 1; p.R = 0.12f; break;
        case 14: withBody (0.9f); p.tri = 0.6f; p.cutoff = 1200; p.fenv = 1; p.fA = 0.001f; p.fD = 0.2f; p.fS = 0.3f; p.A = 0.002f; p.D = 0.45f * lenK; p.S = 0.2f; p.R = 0.1f; break;
        case 15: withBody (0.8f); p.square = 0.5f; p.cutoff = 1100; p.fenv = 2; p.fA = 0.001f; p.fD = 0.12f; p.fS = 0.1f; p.A = 0.002f; p.D = 0.24f * lenK; p.S = 0.08f; p.R = 0.06f; break;
        case 16: eight08 (2.6f, 0.45f, 0.2f, 12.0f); p.sub = 0.3f; break;                         // 808 Tsunami
        case 17: p.sine = 1; p.A = 0.01f; p.D = 0.3f; p.S = 1; p.R = 0.2f; p.cutoff = 9000; p.fenv = 0; p.sub = 0.5f; p.drive = 0.08f; break;   // Sub Abyssal
        case 18: eight08 (1.6f, 0.3f, 0.1f); p.glide = 0.12f; break;                              // 808 Glissade (always slides)
        case 19: withBody (0.9f); p.tri = 0.5f; p.cutoff = 700; p.fenv = 0.6f; p.A = 0.02f; p.D = 0.6f; p.S = 0.8f; p.R = 0.3f; p.chorus = 0.5f; p.vibrato = 0.05f; p.lfoRate = 5.0f; break; // Basse Rêve
        case 20: eight08 (1.2f, 0.2f, 0.9f, 14.0f); break;                                        // 808 Cristal
        case 21: p.sine = 1; p.A = 0.005f; p.S = 1; p.R = 0.12f; p.cutoff = 9000; p.fenv = 0; p.lfoFilter = 0; p.vibrato = 0.0f; p.tri = 0.25f; p.drive = 0.2f; break; // Sub Pulse
        case 22: withBody (0.8f); p.square = 0.45f; p.saw = 0.3f; p.cutoff = 1500; p.reso = 0.35f; p.fenv = 1.5f; p.fA = 0.001f; p.fD = 0.3f; p.fS = 0.3f; p.A = 0.003f; p.S = 0.8f; p.R = 0.1f; p.drive = 0.2f; break; // Basse Néon
        default: eight08 (1.5f, 0.3f, 0.1f, -10.0f); p.pitchDecay = 0.12f; break;                  // 808 Montée: rises into the note
    }
    if (i != 18) p.glide = 0.02f + glide * 0.22f; else p.glide = juce::jmax (0.08f, 0.02f + glide * 0.22f);
    p.drive = juce::jlimit (0.0f, 1.0f, p.drive + dist * 0.9f);
    p.sub = juce::jlimit (0.0f, 1.0f, p.sub + subAmt * 0.7f);
    return p;
}
} // namespace sounds
} // namespace tchk
