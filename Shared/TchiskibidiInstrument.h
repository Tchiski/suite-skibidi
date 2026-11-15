#pragma once

// Shared engine for the Tchiskibidi instruments (Accords, Synthé, Basse):
// music theory, a polyphonic subtractive synth, note sequences that follow the host
// transport, and MIDI / WAV export for drag & drop.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "TchiskibidiUI.h"

namespace tchk
{
//==============================================================================
// Music theory
namespace music
{
    inline const juce::StringArray& noteNames()
    {
        static const juce::StringArray n { TK_FR ("Do"), TK_FR ("Do#"), TK_FR ("Ré"), TK_FR ("Ré#"), TK_FR ("Mi"), TK_FR ("Fa"),
                                           TK_FR ("Fa#"), TK_FR ("Sol"), TK_FR ("Sol#"), TK_FR ("La"), TK_FR ("La#"), TK_FR ("Si") };
        return n;
    }
    inline const juce::StringArray& scaleNames()
    {
        static const juce::StringArray n { "Majeur", "Mineur", "Dorien", "Phrygien", TK_FR ("Mineur harm."), "Mixolydien" };
        return n;
    }
    inline const std::array<int, 7>& scale (int i)
    {
        static const std::array<std::array<int, 7>, 6> s { { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 },
                                                             { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 2, 4, 5, 7, 9, 10 } } };
        return s[(size_t) juce::jlimit (0, 5, i)];
    }
    // pitch of a scale degree (degree can go above 6 or below 0)
    inline int degreePitch (int root, int scaleIdx, int degree, int baseMidi)
    {
        const auto& sc = scale (scaleIdx);
        const int d = ((degree % 7) + 7) % 7;
        const int oct = (int) std::floor ((double) degree / 7.0);
        return baseMidi + root + sc[(size_t) d] + 12 * oct;
    }
    // chord name from a set of pitches ("Lam", "Fa", "Sol7" ...)
    inline juce::String chordName (std::vector<int> pitches)
    {
        if (pitches.empty()) return "-";
        std::sort (pitches.begin(), pitches.end());
        bool pcs[12] = {};
        for (int p : pitches) pcs[((p % 12) + 12) % 12] = true;
        struct Q { const char* name; std::vector<int> iv; };
        static const std::vector<Q> qs { { "maj7", { 0, 4, 7, 11 } }, { "7", { 0, 4, 7, 10 } }, { "m7", { 0, 3, 7, 10 } }, { "m7b5", { 0, 3, 6, 10 } },
                                         { "", { 0, 4, 7 } }, { "m", { 0, 3, 7 } }, { "dim", { 0, 3, 6 } }, { "aug", { 0, 4, 8 } }, { "sus4", { 0, 5, 7 } }, { "sus2", { 0, 2, 7 } } };
        const int bass = ((pitches.front() % 12) + 12) % 12;
        for (int pass = 0; pass < 2; ++pass)
            for (auto& q : qs)
                for (int r = 0; r < 12; ++r)
                {
                    if (pass == 0 && r != bass) continue;
                    bool ok = true; int count = 0;
                    for (int iv : q.iv) if (! pcs[(r + iv) % 12]) { ok = false; break; }
                    for (int k = 0; k < 12; ++k) count += pcs[k] ? 1 : 0;
                    if (ok && count <= (int) q.iv.size() + 1)
                        return noteNames()[r] + q.name;
                }
        return noteNames()[bass];
    }
    // simple key detection from a pitch-class histogram (Krumhansl profiles): returns root, isMinor
    inline std::pair<int, bool> detectKey (const std::array<float, 12>& hist)
    {
        static const float maj[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        static const float mnr[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
        float best = -1e9f; int bestRoot = 0; bool bestMinor = false;
        for (int r = 0; r < 12; ++r)
            for (int m = 0; m < 2; ++m)
            {
                float sc = 0.0f;
                for (int k = 0; k < 12; ++k) sc += hist[(size_t) ((r + k) % 12)] * (m ? mnr[k] : maj[k]);
                if (sc > best) { best = sc; bestRoot = r; bestMinor = m == 1; }
            }
        return { bestRoot, bestMinor };
    }
}

//==============================================================================
// A note of a sequence: positions in 16th-note steps
struct SeqNote
{
    int pitch = 60;
    int start = 0;     // in steps
    int length = 4;    // in steps
    bool glide = false;
    float velocity = 0.8f;
};

inline juce::String notesToString (const std::vector<SeqNote>& notes)
{
    juce::StringArray a;
    for (auto& n : notes) a.add (juce::String (n.pitch) + ":" + juce::String (n.start) + ":" + juce::String (n.length) + ":" + (n.glide ? "1" : "0") + ":" + juce::String (juce::roundToInt (n.velocity * 127.0f)));
    return a.joinIntoString (";");
}
inline std::vector<SeqNote> notesFromString (const juce::String& s)
{
    std::vector<SeqNote> out;
    for (auto& tok : juce::StringArray::fromTokens (s, ";", ""))
    {
        auto f = juce::StringArray::fromTokens (tok, ":", "");
        if (f.size() < 3) continue;
        SeqNote n; n.pitch = f[0].getIntValue(); n.start = f[1].getIntValue(); n.length = juce::jmax (1, f[2].getIntValue());
        n.glide = f.size() > 3 && f[3] == "1"; n.velocity = f.size() > 4 ? juce::jlimit (0.05f, 1.0f, (float) f[4].getIntValue() / 127.0f) : 0.8f;
        out.push_back (n);
    }
    return out;
}

//==============================================================================
// Synth patch: everything that makes one sound
struct Patch
{
    float sine = 0, tri = 0, saw = 1, square = 0, noise = 0, sub = 0;
    int unison = 1; float detune = 8.0f;           // cents
    float cutoff = 4000, reso = 0.15f, fenv = 1.0f; // fenv in octaves
    float fA = 0.005f, fD = 0.4f, fS = 0.3f, fR = 0.4f;
    float A = 0.005f, D = 0.3f, S = 0.8f, R = 0.3f;
    float drive = 0, pitchEnv = 0, pitchDecay = 0.05f;
    float lfoRate = 4.0f, vibrato = 0, lfoFilter = 0;
    float space = 0.15f, width = 0.5f, glide = 0, level = 0.7f, click = 0;
    bool mono = false;
    float octave = 0;
    float vowel = -1.0f;   // >= 0: choir voice (0 a, 1 o, 2 ou, 3 é, 4 i), formant filters instead of the low-pass
    float chorus = 0.0f;   // soft stereo chorus
};

// Simple TPT state-variable low-pass (stable when modulated)
struct SVF
{
    float ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }
    float process (float x, float g, float k)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        return v2;
    }
    float bandpass (float x, float g, float k)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        return v1;
    }
};

// vowel formants (Hz) and their levels, interpolated between a, o, ou, é, i
inline void formants (float vowel, float* f, float* gn)
{
    static const float tab[5][3] = { { 800, 1150, 2900 }, { 450, 800, 2830 }, { 325, 700, 2700 }, { 400, 1700, 2600 }, { 300, 2100, 3000 } };
    static const float lv[5][3] = { { 1.0f, 0.5f, 0.12f }, { 1.0f, 0.35f, 0.08f }, { 1.0f, 0.2f, 0.05f }, { 1.0f, 0.3f, 0.12f }, { 1.0f, 0.25f, 0.1f } };
    const float v = juce::jlimit (0.0f, 3.999f, vowel);
    const int i = (int) v; const float t = v - (float) i;
    for (int k = 0; k < 3; ++k) { f[k] = tab[i][k] + (tab[i + 1][k] - tab[i][k]) * t; gn[k] = lv[i][k] + (lv[i + 1][k] - lv[i][k]) * t; }
}

struct Env
{
    enum Stage { Off, Att, Dec, Sus, Rel } stage = Off;
    float v = 0;
    void on() { stage = Att; }
    void off() { if (stage != Off) stage = Rel; }
    float next (float A, float D, float S, float R, float sr)
    {
        switch (stage)
        {
            case Att: v += 1.0f / juce::jmax (1.0f, A * sr); if (v >= 1.0f) { v = 1.0f; stage = Dec; } break;
            case Dec: v += (S - v) * (1.0f - std::exp (-1.0f / juce::jmax (1.0f, D * sr * 0.3f))); if (std::abs (v - S) < 1e-4f) stage = Sus; break;
            case Sus: v = S; break;
            case Rel: v -= v * (1.0f - std::exp (-1.0f / juce::jmax (1.0f, R * sr * 0.3f))); if (v < 1e-5f) { v = 0; stage = Off; } break;
            case Off: default: v = 0; break;
        }
        return v;
    }
};

inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

class Voice
{
public:
    int note = -1; bool held = false;
    juce::uint32 age = 0;
    bool active() const { return amp.stage != Env::Off; }

    void start (int midi, float vel, const Patch& p, float fromMidi, bool legato)
    {
        note = midi; held = true; velocity = vel;
        target = (float) midi + p.octave * 12.0f;
        current = (fromMidi >= 0 && p.glide > 0.0005f) ? fromMidi + p.octave * 12.0f : target;
        if (! legato || ! active())
        {
            if (! active() || amp.v < 0.01f) { for (auto& ph : phase) ph = juce::Random::getSystemRandom().nextFloat(); fl.reset(); fr.reset(); for (auto& b : fb) b.reset(); }
            amp.on(); fil.on(); pitchEnvV = 1.0f; clickV = 1.0f;
        }
        else if (amp.stage == Env::Rel || amp.stage == Env::Dec || amp.stage == Env::Sus)
        {
            amp.on(); // legato slide: keep the sound going, swell back from where it is
        }
    }
    void release() { held = false; amp.off(); fil.off(); }
    void kill() { amp.stage = Env::Off; amp.v = 0; note = -1; }

    void render (const Patch& p, float sr, float* L, float* R, int n, float lfo, float lfoInc)
    {
        const int U = juce::jlimit (1, 7, p.unison);
        const float glideCoef = p.glide > 0.0005f ? 1.0f - std::exp (-1.0f / (p.glide * sr * 0.25f)) : 1.0f;
        const float pdCoef = std::exp (-1.0f / juce::jmax (1.0f, p.pitchDecay * sr));
        const float k = 2.0f - 1.9f * juce::jlimit (0.0f, 1.0f, p.reso);
        std::array<float, 7> ratio {}, panL {}, panR {};
        for (int u = 0; u < U; ++u)
        {
            const float spread = U > 1 ? ((float) u / (float) (U - 1)) * 2.0f - 1.0f : 0.0f;
            ratio[(size_t) u] = std::exp2 (spread * p.detune / 1200.0f);
            const float pan = 0.5f + spread * 0.5f * p.width;
            panL[(size_t) u] = 1.0f - pan; panR[(size_t) u] = pan;
        }
        float g = 0.1f;
        for (int i = 0; i < n; ++i)
        {
            current += (target - current) * glideCoef;
            const float vib = p.vibrato > 0 ? std::sin (lfo) * p.vibrato * 0.5f : 0.0f;
            lfo += lfoInc;
            const float pitch = current + vib + p.pitchEnv * pitchEnvV;
            pitchEnvV *= pdCoef;
            const float baseHz = 440.0f * std::exp2 ((pitch - 69.0f) / 12.0f);
            float sl = 0, sr2 = 0;
            for (int u = 0; u < U; ++u)
            {
                const float dt = juce::jmin (0.45f, baseHz * ratio[(size_t) u] / sr);
                float& ph = phase[(size_t) u];
                float s = 0;
                if (p.sine > 0) s += p.sine * std::sin (ph * juce::MathConstants<float>::twoPi);
                if (p.tri > 0) s += p.tri * (4.0f * std::abs (ph - 0.5f) - 1.0f);
                if (p.saw > 0) s += p.saw * ((2.0f * ph - 1.0f) - polyBlep (ph, dt));
                if (p.square > 0) { float sq = ph < 0.5f ? 1.0f : -1.0f; sq += polyBlep (ph, dt); float t2 = ph + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f; sq -= polyBlep (t2, dt); s += p.square * sq; }
                ph += dt; if (ph >= 1.0f) ph -= 1.0f;
                sl += s * panL[(size_t) u]; sr2 += s * panR[(size_t) u];
            }
            const float norm = 1.0f / std::sqrt ((float) U);
            sl *= norm * 1.4f; sr2 *= norm * 1.4f;
            if (p.sub > 0) { const float sv = p.sub * std::sin (subPh * juce::MathConstants<float>::twoPi); subPh += baseHz * 0.5f / sr; if (subPh >= 1.0f) subPh -= 1.0f; sl += sv; sr2 += sv; }
            if (p.noise > 0) { const float nz = p.noise * (rng.nextFloat() * 2.0f - 1.0f); sl += nz; sr2 += nz; }
            const float fe = fil.next (p.fA, p.fD, p.fS, p.fR, sr);
            if ((i & 7) == 0)
            {
                const float cutoff = juce::jlimit (30.0f, sr * 0.45f, p.cutoff * std::exp2 (p.fenv * fe + (p.lfoFilter > 0 ? p.lfoFilter * std::sin (lfo * 0.5f) : 0.0f)) * (0.6f + 0.4f * velocity));
                g = std::tan (juce::MathConstants<float>::pi * cutoff / sr);
            }
            float yl, yr;
            if (p.vowel >= 0.0f)
            {
                if ((i & 31) == 0)
                {
                    float f[3], gn[3];
                    formants (p.vowel + 0.15f * std::sin (lfo * 0.21f), f, gn);
                    for (int q = 0; q < 3; ++q) { fbG[(size_t) q] = std::tan (juce::MathConstants<float>::pi * juce::jmin (f[q], sr * 0.45f) / sr); fbGain[(size_t) q] = gn[q]; }
                }
                const float kq = 0.12f;
                yl = 0; yr = 0;
                for (int q = 0; q < 3; ++q)
                {
                    yl += fbGain[(size_t) q] * kq * 2.5f * fb[(size_t) q].bandpass (sl, fbG[(size_t) q], kq);
                    yr += fbGain[(size_t) q] * kq * 2.5f * fb[(size_t) (q + 3)].bandpass (sr2, fbG[(size_t) q], kq);
                }
                yl = fl.process (yl * 0.9f, g, k); yr = fr.process (yr * 0.9f, g, k);
            }
            else { yl = fl.process (sl, g, k); yr = fr.process (sr2, g, k); }
            if (p.drive > 0.01f) { const float d = 1.0f + p.drive * 8.0f, nd = 1.0f / std::tanh (d); yl = std::tanh (yl * d) * nd; yr = std::tanh (yr * d) * nd; }
            const float a = amp.next (p.A, p.D, p.S, p.R, sr) * velocity * p.level;
            float cl = 0;
            if (p.click > 0 && clickV > 0.001f) { cl = p.click * clickV * std::sin (clickPh); clickPh += juce::MathConstants<float>::twoPi * 1800.0f / sr; clickV *= 0.994f; }
            L[i] += yl * a + cl * a; R[i] += yr * a + cl * a;
        }
        if (! active()) note = -1;
    }

private:
    float target = 60, current = 60, velocity = 0.8f, pitchEnvV = 0, subPh = 0, clickV = 0, clickPh = 0;
    std::array<float, 7> phase {};
    Env amp, fil;
    SVF fl, fr;
    std::array<SVF, 6> fb;
    std::array<float, 3> fbG {}, fbGain {};
    juce::Random rng;
};

// Polyphonic synth (with a mono/legato mode for basses and leads) + reverb
class SynthEngine
{
public:
    static constexpr int numVoices = 16;
    Patch patch;
    float outputGain = 1.0f;   // overall level before the soft limiter (polyphonic pads need headroom)

    void prepare (double sampleRate, int maxBlock)
    {
        sr = (float) sampleRate;
        tmpL.assign ((size_t) juce::jmax (1, maxBlock), 0.0f); tmpR = tmpL;
        reverb.setSampleRate (sampleRate);
        for (auto& v : voices) v.kill();
    }
    void noteOn (int midi, float vel, bool glide = false)
    {
        if (patch.mono)
        {
            auto& v = voices[0];
            const bool legato = v.active() && (v.held || glide);
            const float from = (glide || (legato && patch.glide > 0)) && lastNote >= 0 ? (float) lastNote : -1.0f;
            v.start (midi, vel, patch, from, legato && glide);
            lastNote = midi; heldStack.removeAllInstancesOf (midi); heldStack.add (midi);
            return;
        }
        Voice* best = nullptr;
        for (auto& v : voices) if (v.note == midi && v.active()) { best = &v; break; }
        if (best == nullptr) for (auto& v : voices) if (! v.active()) { best = &v; break; }
        if (best == nullptr) { best = &voices[0]; for (auto& v : voices) if (v.age < best->age) best = &v; }
        best->age = ++counter;
        best->start (midi, vel, patch, glide && lastNote >= 0 ? (float) lastNote : -1.0f, false);
        lastNote = midi;
    }
    void noteOff (int midi)
    {
        if (patch.mono)
        {
            heldStack.removeAllInstancesOf (midi);
            auto& v = voices[0];
            if (v.note == midi)
            {
                if (! heldStack.isEmpty()) { const int back = heldStack.getLast(); v.start (back, 0.8f, patch, (float) midi, true); lastNote = back; }
                else v.release();
            }
            return;
        }
        for (auto& v : voices) if (v.note == midi && v.held) v.release();
    }
    void allOff() { for (auto& v : voices) v.release(); heldStack.clear(); }
    void panic() { for (auto& v : voices) v.kill(); heldStack.clear(); }

    void handleMidi (const juce::MidiMessage& m)
    {
        if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) allOff();
    }

    // renders (adds) into the buffer between start and start + n
    void render (juce::AudioBuffer<float>& buf, int start, int n)
    {
        if (n <= 0) return;
        if ((int) tmpL.size() < n) { tmpL.resize ((size_t) n); tmpR.resize ((size_t) n); }
        std::fill (tmpL.begin(), tmpL.begin() + n, 0.0f); std::fill (tmpR.begin(), tmpR.begin() + n, 0.0f);
        const float lfoInc = juce::MathConstants<float>::twoPi * patch.lfoRate / sr;
        bool any = false;
        for (auto& v : voices) if (v.active()) { v.render (patch, sr, tmpL.data(), tmpR.data(), n, lfoPhase, lfoInc); any = true; }
        lfoPhase = std::fmod (lfoPhase + lfoInc * (float) n, juce::MathConstants<float>::twoPi * 2.0f);
        juce::Reverb::Parameters rp; rp.roomSize = 0.55f + 0.4f * patch.space; rp.damping = 0.5f; rp.wetLevel = patch.space * 0.55f; rp.dryLevel = 1.0f - patch.space * 0.25f; rp.width = 1.0f;
        reverb.setParameters (rp);
        if (patch.chorus > 0.01f) applyChorus (n);
        if (any || patch.space > 0.01f) reverb.processStereo (tmpL.data(), tmpR.data(), n);
        auto* L = buf.getWritePointer (0, start);
        auto* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1, start) : nullptr;
        for (int i = 0; i < n; ++i)
        {
            L[i] += softClip (tmpL[(size_t) i] * outputGain);
            if (R != nullptr) R[i] += softClip (tmpR[(size_t) i] * outputGain);
        }
    }
    bool anyActive() const { for (auto& v : voices) if (v.active()) return true; return false; }

private:
    // clean below 0.6, then a gentle knee (no fizz on normal levels)
    static float softClip (float x)
    {
        const float a = std::abs (x);
        if (a < 0.6f) return x;
        const float y = 0.6f + 0.4f * std::tanh ((a - 0.6f) / 0.4f);
        return x < 0 ? -y : y;
    }
    void applyChorus (int n)
    {
        if (chL.empty()) { chL.assign (4096, 0.0f); chR.assign (4096, 0.0f); }
        const float depth = 0.0025f * sr, base = 0.012f * sr, mix = juce::jlimit (0.0f, 0.6f, patch.chorus * 0.6f);
        const float inc = juce::MathConstants<float>::twoPi * 0.35f / sr;
        for (int i = 0; i < n; ++i)
        {
            chL[(size_t) chW] = tmpL[(size_t) i]; chR[(size_t) chW] = tmpR[(size_t) i];
            auto read = [&] (const std::vector<float>& b, float d)
            {
                float pos = (float) chW - d; while (pos < 0) pos += 4096.0f;
                const int i0 = (int) pos; const float fr = pos - (float) i0;
                return b[(size_t) i0] + (b[(size_t) ((i0 + 1) & 4095)] - b[(size_t) i0]) * fr;
            };
            const float dl = base + depth * std::sin (chPh), dr = base + depth * std::sin (chPh + 1.9f);
            const float wl = read (chL, dl), wr = read (chR, dr);
            tmpL[(size_t) i] = tmpL[(size_t) i] * (1.0f - mix * 0.5f) + wr * mix;
            tmpR[(size_t) i] = tmpR[(size_t) i] * (1.0f - mix * 0.5f) + wl * mix;
            chW = (chW + 1) & 4095; chPh += inc; if (chPh > juce::MathConstants<float>::twoPi) chPh -= juce::MathConstants<float>::twoPi;
        }
    }
    std::vector<float> chL, chR;
    int chW = 0; float chPh = 0;
    std::array<Voice, numVoices> voices;
    juce::Reverb reverb;
    std::vector<float> tmpL, tmpR;
    float sr = 44100.0f, lfoPhase = 0;
    juce::uint32 counter = 0;
    int lastNote = -1;
    juce::Array<int> heldStack;
};

//==============================================================================
// Plays a looping note sequence in sync with the host: position comes from the
// host's play head, so a 4- or 8-bar loop in the DAW always restarts at the top.
class SequencePlayer
{
public:
    std::function<void (int pitch, float vel, bool glide, int sampleOffset)> onNoteOn;
    std::function<void (int pitch, int sampleOffset)> onNoteOff;
    std::atomic<double> playStep { -1.0 };   // for the editor's playhead
    std::atomic<bool> playing { false };
    double bpm = 120.0;

    void setNotes (const std::vector<SeqNote>& n, int totalSteps)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        pending = n; pendingSteps = totalSteps; hasPending = true;
    }

    // call once per block
    void process (juce::AudioPlayHead* ph, int numSamples, double sampleRate)
    {
        {
            const juce::SpinLock::ScopedTryLockType sl (lock);
            if (sl.isLocked() && hasPending) { notes = pending; steps = juce::jmax (1, pendingSteps); hasPending = false; }
        }
        bool isPlaying = false; double ppq = 0.0;
        if (ph != nullptr)
            if (auto pos = ph->getPosition())
            {
                isPlaying = pos->getIsPlaying();
                if (auto p = pos->getPpqPosition()) ppq = *p; else isPlaying = false;
                if (auto b = pos->getBpm()) bpm = *b;
            }
        if (! isPlaying)
        {
            if (wasPlaying) stopAll (0);
            wasPlaying = false; playing = false; playStep = -1.0;
            return;
        }
        const double stepsPerSample = (bpm / 60.0) * 4.0 / sampleRate;
        const double s0 = ppq * 4.0, s1 = s0 + stepsPerSample * numSamples;
        if (! wasPlaying || std::abs (s0 - expected) > 0.5) { stopAll (0); lastPos = s0 - 1e-9; } // jump / loop in the host
        wasPlaying = true; playing = true;
        const double L = (double) steps;
        // note offs first, then note ons
        for (int i = (int) active.size(); --i >= 0;)
        {
            auto& a = active[(size_t) i];
            if (a.endAbs <= s1)
            {
                const int off = juce::jlimit (0, numSamples - 1, (int) ((a.endAbs - s0) / stepsPerSample));
                if (onNoteOff) onNoteOff (a.pitch, off);
                active.erase (active.begin() + i);
            }
        }
        const double startLoop = std::floor (lastPos / L), endLoop = std::floor (s1 / L);
        for (double loop = startLoop; loop <= endLoop; loop += 1.0)
            for (auto& n : notes)
            {
                const double at = loop * L + n.start;
                if (at > lastPos && at <= s1 && at >= s0 - 1e-6)
                {
                    const int off = juce::jlimit (0, numSamples - 1, (int) ((at - s0) / stepsPerSample));
                    for (int i = (int) active.size(); --i >= 0;)   // same pitch still ringing: cut it
                        if (active[(size_t) i].pitch == n.pitch) { if (onNoteOff) onNoteOff (n.pitch, off); active.erase (active.begin() + i); }
                    if (onNoteOn) onNoteOn (n.pitch, n.velocity, n.glide, off);
                    active.push_back ({ n.pitch, at + n.length - 0.02 });
                }
            }
        lastPos = s1; expected = s1;
        playStep = std::fmod (s0, L);
    }
    void stopAll (int offset)
    {
        for (auto& a : active) if (onNoteOff) onNoteOff (a.pitch, offset);
        active.clear();
    }

private:
    struct Active { int pitch; double endAbs; };
    std::vector<SeqNote> notes, pending;
    std::vector<Active> active;
    int steps = 64, pendingSteps = 64;
    bool hasPending = false, wasPlaying = false;
    double lastPos = 0, expected = -100;
    juce::SpinLock lock;
};

//==============================================================================
// Drag & drop export
namespace exportfiles
{
    inline juce::File tempDir()
    {
        auto d = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Tchiskibidi");
        d.createDirectory();
        return d;
    }
    inline juce::File writeMidi (const std::vector<SeqNote>& notes, double bpm, const juce::String& name, int channel = 1)
    {
        juce::MidiMessageSequence seq;
        const int tpq = 960, tps = tpq / 4;
        seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / juce::jmax (20.0, bpm))), 0);
        for (auto& n : notes)
        {
            seq.addEvent (juce::MidiMessage::noteOn (channel, n.pitch, (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (n.velocity * 127.0f))), n.start * tps);
            seq.addEvent (juce::MidiMessage::noteOff (channel, n.pitch), (n.start + n.length) * tps - 1);
        }
        seq.updateMatchedPairs();
        juce::MidiFile mf; mf.setTicksPerQuarterNote (tpq); mf.addTrack (seq);
        auto f = tempDir().getChildFile (juce::File::createLegalFileName (name) + ".mid");
        f.deleteFile();
        juce::FileOutputStream out (f);
        if (out.openedOk()) mf.writeTo (out);
        return f;
    }
    // renders the sequence with a fresh copy of the synth (offline, at the given tempo)
    inline juce::File writeWav (const std::vector<SeqNote>& notes, int totalSteps, double bpm, const Patch& patch, const juce::String& name, int loops = 1)
    {
        const double sr = 44100.0, stepSec = 60.0 / juce::jmax (20.0, bpm) / 4.0;
        const int total = (int) (stepSec * totalSteps * loops * sr) + (int) (sr * 2.0);
        juce::AudioBuffer<float> buf (2, total); buf.clear();
        SynthEngine eng; eng.patch = patch; eng.prepare (sr, 512);
        struct Ev { int sample; bool on; int pitch; float vel; bool glide; };
        std::vector<Ev> evs;
        for (int l = 0; l < loops; ++l)
            for (auto& n : notes)
            {
                const double t0 = (l * totalSteps + n.start) * stepSec, t1 = t0 + n.length * stepSec - 0.005;
                evs.push_back ({ (int) (t0 * sr), true, n.pitch, n.velocity, n.glide });
                evs.push_back ({ (int) (t1 * sr), false, n.pitch, 0.0f, false });
            }
        std::stable_sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b) { return a.sample < b.sample || (a.sample == b.sample && ! a.on && b.on); });
        int pos = 0; size_t e = 0;
        while (pos < total)
        {
            while (e < evs.size() && evs[e].sample <= pos)
            {
                if (evs[e].on) eng.noteOn (evs[e].pitch, evs[e].vel, evs[e].glide); else eng.noteOff (evs[e].pitch);
                ++e;
            }
            const int next = e < evs.size() ? evs[e].sample : total;
            const int len = juce::jmin (total - pos, juce::jlimit (1, 512, next - pos));
            eng.render (buf, pos, len);
            pos += len;
        }
        buf.applyGain (0.9f);
        auto f = tempDir().getChildFile (juce::File::createLegalFileName (name) + ".wav");
        f.deleteFile();
        juce::WavAudioFormat wav;
        if (auto* os = new juce::FileOutputStream (f))
        {
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os, sr, 2, 24, {}, 0));
            if (w != nullptr) w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
            else delete os;
        }
        return f;
    }
}

//==============================================================================
// Thread-safe little queue of notes to audition from the editor (clicks on the grid)
class Audition
{
public:
    void push (int pitch, float vel, int lengthMs)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        if (queue.size() < 32) queue.push_back ({ pitch, vel, lengthMs });
    }
    // call from the audio thread
    void process (SynthEngine& eng, int numSamples, double sr)
    {
        {
            const juce::SpinLock::ScopedTryLockType sl (lock);
            if (sl.isLocked())
            {
                for (auto& q : queue) { eng.noteOn (q.pitch, q.vel); ringing.push_back ({ q.pitch, (int) (q.ms * 0.001 * sr) }); }
                queue.clear();
            }
        }
        for (int i = (int) ringing.size(); --i >= 0;)
        {
            ringing[(size_t) i].left -= numSamples;
            if (ringing[(size_t) i].left <= 0) { eng.noteOff (ringing[(size_t) i].pitch); ringing.erase (ringing.begin() + i); }
        }
    }
private:
    struct Q { int pitch; float vel; int ms; };
    struct Ring { int pitch; int left; };
    std::vector<Q> queue;
    std::vector<Ring> ringing;
    juce::SpinLock lock;
};
} // namespace tchk
