#pragma once

// Chord-progression and bass-line generators (Tchiskibidi Accords / Basse).
// Positions are in 16th-note steps (16 per bar).

#include "TchiskibidiInstrument.h"

namespace tchk
{
namespace gen
{
using Prog = std::vector<int>;
inline const juce::StringArray& moodNames() { static const juce::StringArray n { "Triste", "Joyeux", "Sombre", TK_FR ("Rêveur"), "Trap", "R&B" }; return n; }

struct ChordEvent { int degree = 0, start = 0, length = 16; };

inline const std::vector<Prog>& moodPool (int mood)
{
    static const std::vector<std::vector<Prog>> pools {
        { { 0, 5, 2, 6 }, { 0, 3, 4, 0 }, { 0, 5, 3, 4 }, { 5, 3, 0, 4 }, { 0, 5, 3, 4 }, { 5, 3, 0, 6 }, { 0, 6, 5, 6 }, { 0, 3, 5, 4 }, { 3, 5, 0, 4 } },
        { { 0, 4, 5, 3 }, { 0, 3, 4, 4 }, { 0, 5, 3, 4 }, { 3, 4, 0, 0 }, { 0, 5, 1, 4 }, { 3, 0, 4, 5 }, { 0, 2, 3, 4 }, { 5, 3, 0, 4 }, { 0, 4, 2, 3 } },
        { { 0, 1, 0, 6 }, { 0, 5, 1, 4 }, { 0, 6, 5, 4 }, { 0, 0, 5, 6 }, { 0, 3, 1, 0 }, { 0, 5, 4, 0 }, { 0, 1, 5, 6 }, { 5, 1, 0, 4 } },
        { { 3, 4, 2, 5 }, { 0, 2, 3, 3 }, { 5, 3, 0, 4 }, { 3, 0, 4, 5 }, { 0, 2, 5, 3 }, { 3, 5, 2, 4 }, { 5, 2, 3, 0 }, { 0, 3, 1, 4 } },
        { { 0, 5, 6, 5 }, { 0, 0, 5, 4 }, { 0, 6, 5, 5 }, { 0, 3, 5, 4 }, { 0, 5, 3, 6 }, { 0, 2, 5, 4 }, { 0, 1, 0, 5 }, { 5, 6, 0, 0 } },
        { { 1, 4, 0, 5 }, { 3, 2, 1, 4 }, { 5, 1, 4, 0 }, { 1, 2, 3, 4 }, { 1, 4, 2, 5 }, { 3, 4, 2, 5 }, { 1, 2, 3, 0 }, { 5, 1, 4, 0 }, { 3, 2, 5, 1 } } };
    return pools[(size_t) juce::jlimit (0, 5, mood)];
}
// rhythm templates for one bar (16 steps): {start, length, push}
struct Hit { int s, l; bool push; };
inline const std::vector<std::vector<Hit>>& moodRhythms (int mood)
{
    static const std::vector<std::vector<std::vector<Hit>>> r {
        { { { 0, 16, false } }, { { 0, 16, false } }, { { 0, 12, false }, { 12, 4, false } }, { { 0, 16, true } }, { { 0, 8, false }, { 8, 8, false } } },
        { { { 0, 8, false }, { 8, 8, false } }, { { 0, 12, false }, { 12, 4, false } }, { { 0, 16, false } }, { { 0, 6, false }, { 6, 10, false } }, { { 0, 16, true } } },
        { { { 0, 16, false } }, { { 0, 6, false }, { 6, 10, false } }, { { 0, 16, false } }, { { 0, 12, false }, { 12, 4, false } } },
        { { { 0, 16, false } }, { { 0, 16, true } }, { { 0, 12, false }, { 12, 4, false } }, { { 0, 16, false } } },
        { { { 0, 16, false } }, { { 0, 2, false }, { 6, 2, false }, { 12, 4, false } }, { { 0, 6, false }, { 6, 6, false }, { 12, 4, false } }, { { 0, 16, true } } },
        { { { 0, 8, false }, { 8, 8, false } }, { { 0, 6, false }, { 6, 10, false } }, { { 0, 16, true } }, { { 0, 12, false }, { 12, 4, false } }, { { 0, 4, false }, { 4, 12, false } } } };
    return r[(size_t) juce::jlimit (0, 5, mood)];
}

template <typename T> const T& pick (const std::vector<T>& v, juce::Random& r) { return v[(size_t) r.nextInt ((int) v.size())]; }

inline Prog makeDegrees (int mood, int bars, juce::Random& r)
{
    const auto& pool = moodPool (mood);
    auto phrase = [&]
    {
        Prog p = pick (pool, r);
        if (r.nextFloat() < 0.35f)
        {
            static const std::vector<std::vector<int>> subs { { 5, 2 }, { 3 }, { 0, 4 }, { 1, 5 }, { 6, 2 }, { 3, 0 }, { 4 } };
            const int i = 1 + r.nextInt (2);
            p[(size_t) i] = pick (subs[(size_t) p[(size_t) i]], r);
        }
        if (r.nextFloat() < 0.25f) std::reverse (p.begin(), p.end());
        return p;
    };
    Prog d = phrase();
    if (bars == 8) { Prog b = phrase(); if (r.nextFloat() < 0.6f) b[3] = pick (std::vector<int> { 4, 6, 3 }, r); d.insert (d.end(), b.begin(), b.end()); }
    return d;
}

inline std::vector<int> chordTones (int root, int scaleIdx, int degree, bool seventh)
{
    std::vector<int> n { music::degreePitch (root, scaleIdx, degree, 48), music::degreePitch (root, scaleIdx, degree + 2, 48), music::degreePitch (root, scaleIdx, degree + 4, 48) };
    if (seventh) n.push_back (music::degreePitch (root, scaleIdx, degree + 6, 48));
    return n;
}
// each chord takes the inversion closest to the previous one (smooth voice leading)
inline std::vector<int> voice (const std::vector<int>& tones, const std::vector<int>* prev)
{
    std::vector<int> pcs; for (int t : tones) pcs.push_back (t % 12);
    std::vector<int> best; int bestCost = 1 << 30;
    for (size_t inv = 0; inv < pcs.size(); ++inv)
        for (int oct = 3; oct <= 4; ++oct)
        {
            std::vector<int> out; int last = -1;
            for (size_t k = 0; k < pcs.size(); ++k)
            {
                int n = 12 * (oct + 1) + pcs[(inv + k) % pcs.size()];
                while (n <= last) n += 12;
                out.push_back (n); last = n;
            }
            if (out.front() < 50 || out.back() > 76) continue;
            int cost = 0;
            if (prev != nullptr && ! prev->empty()) for (size_t i = 0; i < out.size(); ++i) cost += std::abs (out[i] - (*prev)[juce::jmin (i, prev->size() - 1)]);
            else cost = std::abs (out.front() - 57);
            if (cost < bestCost) { bestCost = cost; best = out; }
        }
    return best.empty() ? tones : best;
}

struct ChordResult { Prog degrees; std::vector<ChordEvent> events; std::vector<SeqNote> notes; };

inline ChordResult generateChords (int root, int scaleIdx, int bars, int mood, bool seventh, juce::Random& r, const Prog* avoid = nullptr, const Prog* previous = nullptr)
{
    ChordResult res;
    for (int tries = 0; tries < 10; ++tries)
    {
        res.degrees = makeDegrees (mood, bars, r);
        const bool same = (previous != nullptr && res.degrees == *previous) || (avoid != nullptr && Prog (res.degrees.begin(), res.degrees.begin() + juce::jmin ((size_t) 4, res.degrees.size())) == *avoid);
        if (! same) break;
    }
    const auto& rhythms = moodRhythms (mood);
    for (size_t bar = 0; bar < res.degrees.size(); ++bar)
    {
        const int deg = res.degrees[bar];
        const auto& tpl = pick (rhythms, r);
        for (size_t k = 0; k < tpl.size(); ++k)
        {
            int d = deg;
            if (k > 0)
            {
                if (k == tpl.size() - 1 && tpl.size() > 1)
                    d = bar + 1 < res.degrees.size() ? pick (std::vector<int> { deg, (deg + 4) % 7, (res.degrees[bar + 1] + 6) % 7 }, r) : 4;
                else d = (deg + pick (std::vector<int> { 2, 5 }, r)) % 7;
            }
            int start = (int) bar * 16 + tpl[k].s, len = tpl[k].l;
            if (tpl[k].push && bar > 0) { start -= 2; len += 2; if (! res.events.empty()) res.events.back().length -= 2; }
            res.events.push_back ({ d, start, len });
        }
    }
    std::vector<int> prev;
    for (auto& e : res.events)
    {
        auto v = voice (chordTones (root, scaleIdx, e.degree, seventh), prev.empty() ? nullptr : &prev);
        prev = v;
        for (int p : v) { SeqNote n; n.pitch = p; n.start = e.start; n.length = juce::jmax (1, e.length); n.velocity = 0.75f; res.notes.push_back (n); }
    }
    return res;
}

//==============================================================================
// Bass lines
inline const juce::StringArray& bassStyles() { static const juce::StringArray n { "Trap", "Drill", "R&B", "Afro", "Boom bap", "Reggaeton", "House", "Funk" }; return n; }
inline double bassStyleBpm (int s) { static const double b[] = { 140, 142, 88, 106, 90, 95, 124, 104 }; return b[juce::jlimit (0, 7, s)]; }
// {start, length, offset (scale steps from the chord root, 99 = approach to the next chord), glide}
struct BHit { int s, l, off, glide; };
inline const std::vector<std::vector<BHit>>& bassTemplates (int style)
{
    static const std::vector<std::vector<std::vector<BHit>>> t {
        { { { 0, 6, 0, 0 }, { 6, 4, 0, 0 }, { 10, 6, 0, 0 } }, { { 0, 10, 0, 0 }, { 10, 6, 7, 1 } }, { { 0, 3, 0, 0 }, { 3, 7, 0, 0 }, { 10, 6, 0, 0 } }, { { 0, 16, 0, 0 } },
          { { 0, 6, 0, 0 }, { 7, 3, 7, 1 }, { 10, 6, 0, 1 } }, { { 0, 7, 0, 0 }, { 7, 3, 0, 0 }, { 11, 5, -1, 1 } } },
        { { { 0, 6, 0, 0 }, { 6, 4, 4, 1 }, { 10, 3, 0, 1 }, { 13, 3, -1, 1 } }, { { 0, 8, 0, 0 }, { 10, 6, 2, 1 } }, { { 0, 6, 0, 0 }, { 6, 7, 7, 1 }, { 13, 3, 4, 1 } }, { { 0, 3, 0, 0 }, { 6, 4, 0, 0 }, { 10, 6, 2, 1 } } },
        { { { 0, 8, 0, 0 }, { 8, 4, 4, 0 }, { 12, 4, 2, 0 } }, { { 0, 6, 0, 0 }, { 6, 2, 7, 0 }, { 8, 8, 0, 0 } }, { { 0, 12, 0, 0 }, { 12, 4, 99, 1 } }, { { 0, 4, 0, 0 }, { 4, 4, 4, 0 }, { 8, 4, 7, 0 }, { 12, 4, 99, 0 } } },
        { { { 0, 3, 0, 0 }, { 3, 3, 0, 0 }, { 6, 2, 4, 0 }, { 8, 3, 0, 0 }, { 11, 3, 7, 0 }, { 14, 2, 4, 0 } }, { { 0, 2, 0, 0 }, { 3, 2, 0, 0 }, { 6, 2, 0, 0 }, { 10, 2, 4, 0 }, { 12, 3, 0, 0 } }, { { 0, 4, 0, 0 }, { 6, 2, 7, 0 }, { 8, 4, 0, 0 }, { 14, 2, 99, 0 } } },
        { { { 0, 6, 0, 0 }, { 7, 3, 0, 0 }, { 10, 6, 4, 0 } }, { { 0, 5, 0, 0 }, { 10, 4, 0, 0 }, { 14, 2, 99, 0 } }, { { 0, 8, 0, 0 }, { 10, 3, 4, 0 }, { 13, 3, 7, 0 } } },
        { { { 0, 3, 0, 0 }, { 3, 5, 0, 0 }, { 8, 3, 0, 0 }, { 11, 5, 0, 0 } }, { { 0, 3, 0, 0 }, { 3, 3, 4, 0 }, { 8, 3, 0, 0 }, { 11, 3, 7, 0 }, { 14, 2, 4, 0 } }, { { 0, 6, 0, 0 }, { 8, 3, 0, 0 }, { 11, 5, 4, 0 } } },
        { { { 2, 2, 0, 0 }, { 6, 2, 0, 0 }, { 10, 2, 0, 0 }, { 14, 2, 0, 0 } }, { { 2, 2, 0, 0 }, { 6, 2, 7, 0 }, { 10, 2, 0, 0 }, { 14, 2, 4, 0 } }, { { 2, 1, 0, 0 }, { 3, 1, 0, 0 }, { 6, 2, 0, 0 }, { 10, 2, 7, 0 }, { 14, 2, 0, 0 } } },
        { { { 0, 2, 0, 0 }, { 3, 1, 7, 0 }, { 4, 2, 0, 0 }, { 7, 1, 0, 0 }, { 10, 2, 4, 0 }, { 12, 1, 7, 0 }, { 14, 2, 99, 0 } }, { { 0, 3, 0, 0 }, { 3, 1, 0, 0 }, { 6, 1, 7, 0 }, { 8, 2, 0, 0 }, { 11, 1, 4, 0 }, { 13, 2, 0, 0 }, { 15, 1, -1, 0 } } } };
    return t[(size_t) juce::jlimit (0, 7, style)];
}

inline std::vector<ChordEvent> randomBassChords (int bars, juce::Random& r, Prog& last)
{
    static const std::vector<Prog> progs { { 0, 5, 3, 4 }, { 0, 3, 4, 0 }, { 0, 5, 2, 6 }, { 0, 6, 5, 4 }, { 0, 2, 3, 4 }, { 5, 3, 0, 4 }, { 0, 4, 5, 3 }, { 0, 0, 3, 4 }, { 3, 4, 0, 5 } };
    Prog p;
    for (int tries = 0; tries < 8; ++tries) { p = pick (progs, r); if (p != last) break; }
    last = p;
    if (bars == 8) { const Prog b = r.nextBool() ? p : pick (progs, r); p.insert (p.end(), b.begin(), b.end()); }
    std::vector<ChordEvent> ev;
    for (size_t i = 0; i < p.size(); ++i) ev.push_back ({ p[i], (int) i * 16, 16 });
    return ev;
}

inline int bassPitch (int root, int scaleIdx, int degree, int off)
{
    int base = 24; if (base + root < 28) base += 12;
    return music::degreePitch (root, scaleIdx, degree + off, base);
}

inline std::vector<SeqNote> generateBass (int root, int scaleIdx, int bars, int style, const std::vector<ChordEvent>& chords, juce::Random& r)
{
    auto degreeAt = [&] (int step) { for (auto& c : chords) if (step >= c.start && step < c.start + c.length) return c.degree; return chords.empty() ? 0 : chords.back().degree; };
    const auto& tpls = bassTemplates (style);
    const auto& a = pick (tpls, r); const auto& b = pick (tpls, r);
    std::vector<SeqNote> out;
    for (int bar = 0; bar < bars; ++bar)
        for (auto& h : (bar % 2 ? b : a))
        {
            const int s = bar * 16 + h.s, deg = degreeAt (s);
            int p;
            if (h.off == 99) p = bassPitch (root, scaleIdx, degreeAt (bar * 16 + 16), 0) + (r.nextBool() ? -1 : 2);
            else p = bassPitch (root, scaleIdx, deg, h.off);
            if (style == 0 && r.nextFloat() < 0.12f) p += 12;
            SeqNote n; n.pitch = p; n.start = s; n.length = h.l; n.glide = h.glide != 0; n.velocity = 0.9f;
            out.push_back (n);
        }
    std::sort (out.begin(), out.end(), [] (const SeqNote& x, const SeqNote& y) { return x.start < y.start; });
    return out;
}

//==============================================================================
// Listening: chords from pitch-class energy, one chord per half bar (merged when equal)
struct HeardChord { int root, start, length; bool minor; };
inline std::vector<HeardChord> chordsFromChroma (const std::vector<std::array<float, 12>>& perHalfBar)
{
    std::vector<HeardChord> out;
    for (size_t i = 0; i < perHalfBar.size(); ++i)
    {
        const auto& c = perHalfBar[i];
        float tot = 0; for (float v : c) tot += v;
        if (tot < 1e-6f) { if (! out.empty()) out.back().length += 8; continue; }
        float best = -1; int br = 0; bool bm = false;
        for (int rr = 0; rr < 12; ++rr)
            for (int m = 0; m < 2; ++m)
            {
                const float sc = c[(size_t) rr] * 1.2f + c[(size_t) ((rr + (m ? 3 : 4)) % 12)] + c[(size_t) ((rr + 7) % 12)] * 0.8f - 0.3f * (c[(size_t) ((rr + (m ? 4 : 3)) % 12)] + c[(size_t) ((rr + 1) % 12)]);
                if (sc > best) { best = sc; br = rr; bm = m == 1; }
            }
        if (! out.empty() && out.back().root == br && out.back().minor == bm) out.back().length += 8;
        else out.push_back ({ br, (int) i * 8, 8, bm });
    }
    return out;
}
} // namespace gen
} // namespace tchk
