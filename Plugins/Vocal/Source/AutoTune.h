#pragma once

// Tchiskibidi Vocal - real-time pitch correction ("autotune").
//   1. Pitch detection: YIN on a decimated (~11-12 kHz) mono copy of the voice.
//   2. The detected note is pulled to the nearest note of the chosen key / scale.
//   3. Pitch shifting: two crossfaded taps on a delay line (granular shifter) whose
//      grain length follows the voice period, so the taps stay in phase (no chorus/comb).
// "Speed" is the retune time: 0 ms = instant (robot effect), 100+ ms = natural.

#include <juce_core/juce_core.h>
#include "TchiskibidiDSP.h"

namespace tchk
{
class AutoTune
{
public:
    static juce::StringArray keyNames()   { return { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" }; }
    static juce::StringArray scaleNames() { return { "Chromatique", "Majeur", "Mineur" }; }
    static juce::StringArray rangeNames() { return { juce::String ("Voix grave (60-500 Hz)"), juce::String ("Toutes les voix (70-1000 Hz)"), juce::String (juce::CharPointer_UTF8 ("Voix aiguë (140-1100 Hz)")) }; }

    // Narrower range = more reliable detection and less CPU
    void setVoiceRange (int r) noexcept
    {
        static constexpr float lo[] = { 60.0f, 70.0f, 140.0f }, hi[] = { 500.0f, 1000.0f, 1100.0f };
        r = juce::jlimit (0, 2, r);
        minHz = lo[r]; maxHz = hi[r];
        tauMin = juce::jmax (2, (int) (dsr / maxHz));
        tauMax = juce::jmin (tauAlloc, (int) (dsr / minHz) + 2);
    }

    // Read by the editor
    std::atomic<float> detectedNote { -1.0f }, targetNote { -1.0f }, shiftSemis { 0.0f };
    std::atomic<bool> displayWanted { false }; // the editor is open: keep detecting for the note display

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        decim = juce::jmax (1, (int) std::round (sr / 11025.0));
        dsr = sr / decim;
        aaf1.set (BiquadCoeffs::make (FilterType::lowPass, sr, juce::jmin (2000.0, dsr * 0.4), 0.7071));
        aaf2.set (BiquadCoeffs::make (FilterType::lowPass, sr, juce::jmin (2000.0, dsr * 0.4), 0.7071));
        tauAlloc = (int) (dsr / 55.0) + 2; // buffers sized for the widest range
        tauMin = juce::jmax (2, (int) (dsr / maxHz));
        tauMax = juce::jmin (tauAlloc, (int) (dsr / minHz) + 2);
        win = juce::jmax (256, (int) (dsr * 0.040));
        ring.assign ((size_t) (win + tauAlloc + 8), 0.0f);
        frame.assign (ring.size(), 0.0f);
        diff.assign ((size_t) tauAlloc + 2, 0.0f);
        hop = juce::jmax (16, (int) (dsr * 0.008));

        maxGrain = (float) (0.060 * sr);
        minGrain = (float) (0.018 * sr);
        line.prepare ((int) maxGrain + 64);
        reset();
    }

    void reset()
    {
        line.reset(); aaf1.reset(); aaf2.reset();
        std::fill (ring.begin(), ring.end(), 0.0f);
        ringPos = decimCount = hopCount = 0;
        period = 0.0f; voiced = false; heldNote = -1;
        shift = 0.0f; targetShift = 0.0f; rmsPow = 0.0f;
        grainA = grainB = (float) (0.030 * sr);
        phaseA = 0.25f; phaseB = 0.75f;
        mix.reset (sr, 0.03); mix.setCurrentAndTargetValue (0.0f);
        detectedNote = targetNote = -1.0f; shiftSemis = 0.0f;
    }

    // key 0..11 (0 = Do), scale 0 chromatic / 1 major / 2 minor, speedMs 0..400, amount 0..1
    void setParams (bool on, int key, int scale, float speedMs, float amountIn)
    {
        mix.setTargetValue (on ? 1.0f : 0.0f);
        keyRoot = juce::jlimit (0, 11, key);
        scaleType = juce::jlimit (0, 2, scale);
        retuneMs = juce::jmax (0.0f, speedMs);
        amount = juce::jlimit (0.0f, 1.0f, amountIn);
    }

    bool isActive() const { return mix.getCurrentValue() > 0.0f || mix.getTargetValue() > 0.0f; }

    // In-place processing of 1 or 2 channels
    void process (float* const* ch, int numCh, int n)
    {
        if (! isActive())
        {
            // switched off: only analyse while the editor shows the notes (saves CPU otherwise)
            if (displayWanted.load (std::memory_order_relaxed))
                for (int i = 0; i < n; ++i) analyse (numCh > 1 ? 0.5f * (ch[0][i] + ch[1][i]) : ch[0][i]);
            else if (detectedNote.load() >= 0.0f)
                unvoiced();
            return;
        }

        const float sepCoef = 1.0f - std::exp (-1.0f / (float) (juce::jmax (0.5f, retuneMs) * 0.001 * sr));

        for (int i = 0; i < n; ++i)
        {
            const float mono = numCh > 1 ? 0.5f * (ch[0][i] + ch[1][i]) : ch[0][i];
            analyse (mono);

            // retune glide (0 ms = jump instantly)
            shift = retuneMs < 0.5f ? targetShift : shift + (targetShift - shift) * sepCoef;
            if (shift != lastShift) { lastShift = shift; shiftRatio = fastExp2 (shift * (1.0f / 12.0f)); }
            float ratio = shiftRatio;

            // "parking": when no correction is needed, slowly bring one tap to full gain so
            // the two taps never sit half-mixed (which would colour the sound)
            if (std::abs (shift) < 0.03f)
            {
                const float gA = gain (phaseA), gB = gain (phaseB);
                const float p = gA >= gB ? phaseA : phaseB;
                const float push = voiced ? 0.002f : 0.02f; // a few cents while singing, faster on breaths/consonants
                if (std::abs (p - 0.5f) > 0.03f)
                    ratio += (p < 0.5f ? -push : push);
            }

            for (int c = 0; c < numCh; ++c) line.write (c, ch[c][i]);

            const float dA = 2.0f + phaseA * grainA, dB = 2.0f + phaseB * grainB;
            const float gA = gain (phaseA), gB = gain (phaseB);
            const float norm = 1.0f / juce::jmax (0.5f, gA + gB);
            const float m = mix.getNextValue();
            for (int c = 0; c < numCh; ++c)
            {
                const float wet = (line.read (c, dA) * gA + line.read (c, dB) * gB) * norm;
                ch[c][i] += m * (wet - ch[c][i]);
            }
            line.advance();

            // advance both taps; a tap only changes grain length while it is silent (at wrap)
            const float rate = 1.0f - ratio; // delay grows by (1 - ratio) samples per sample
            advanceTap (phaseA, grainA, rate);
            advanceTap (phaseB, grainB, rate);
        }
        shiftSemis = shift;
    }

private:
    float minHz = 70.0f, maxHz = 1000.0f;
    int tauAlloc = 160;

    static float gain (float p) noexcept { const float s = std::sin (juce::MathConstants<float>::pi * p); return s * s; }

    float wantedGrain() const noexcept
    {
        if (! voiced || period <= 0.0f) return (float) (0.030 * sr);
        // whole number of periods, at least ~18 ms: the two taps then stay in phase
        float g = period * std::ceil (minGrain / period);
        while (g > maxGrain && g - period >= period) g -= period;
        return juce::jlimit (period, maxGrain, g);
    }

    void advanceTap (float& phase, float& grain, float rate) noexcept
    {
        phase += rate / grain;
        if (phase >= 1.0f || phase < 0.0f)
        {
            grain = wantedGrain();
            phase = phase >= 1.0f ? 0.0f : 0.9999f;
        }
    }

    void analyse (float mono) noexcept
    {
        rmsPow = mono * mono + 0.999f * (rmsPow - mono * mono);
        const float s = aaf2.process (0, aaf1.process (0, mono));
        if (++decimCount < decim) return;
        decimCount = 0;
        ring[(size_t) ringPos] = s;
        if (++ringPos >= (int) ring.size()) ringPos = 0;
        if (++hopCount < hop) return;
        hopCount = 0;
        detect();
    }

    void detect() noexcept
    {
        const int len = (int) ring.size();
        for (int k = 0; k < len; ++k) frame[(size_t) k] = ring[(size_t) ((ringPos + k) % len)];

        if (powerToDb (rmsPow) < -50.0f) { unvoiced(); return; }

        // YIN difference function + cumulative mean normalisation, stopping as soon as the
        // first dip under the threshold has reached its minimum (most of the lags are skipped)
        diff[0] = 1.0f;
        float running = 0.0f;
        int best = -1;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float d = 0.0f;
            const float* a = frame.data();
            const float* b = frame.data() + tau;
            for (int j = 0; j < win; ++j) { const float e = a[j] - b[j]; d += e * e; }
            running += d;
            diff[(size_t) tau] = running > 0.0f ? d * (float) tau / running : 1.0f;
            if (best < 0)
            {
                if (tau > juce::jmax (2, tauMin) && diff[(size_t) tau - 1] < 0.15f && diff[(size_t) tau] >= diff[(size_t) tau - 1])
                {
                    best = tau - 1;
                    break;
                }
            }
        }
        if (best < 0)
            for (int tau = juce::jmax (2, tauMin); tau < tauMax; ++tau)
                if (diff[(size_t) tau] < 0.15f) { best = tau; break; }
        if (best < 0) { unvoiced(); return; }

        // parabolic interpolation for sub-sample precision
        const float a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
        const float den = a - 2.0f * b + c;
        const float tauF = (float) best + (std::abs (den) > 1.0e-9f ? 0.5f * (a - c) / den : 0.0f);
        const float hz = (float) dsr / tauF;
        if (hz < minHz || hz > maxHz) { unvoiced(); return; }

        voiced = true;
        period = (float) sr / hz;
        const float note = 69.0f + 12.0f * std::log2 (hz / 440.0f);
        const int target = nearestNote (note);
        detectedNote = note;
        targetNote = (float) target;
        targetShift = ((float) target - note) * amount;
    }

    void unvoiced() noexcept
    {
        voiced = false;
        targetShift = 0.0f;
        detectedNote = -1.0f;
        targetNote = -1.0f;
    }

    bool inScale (int midi) const noexcept
    {
        static constexpr bool major[12] = { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 };
        static constexpr bool minor[12] = { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 };
        const int deg = ((midi - keyRoot) % 12 + 12) % 12;
        return scaleType == 0 ? true : (scaleType == 1 ? major[deg] : minor[deg]);
    }

    int nearestNote (float note) noexcept
    {
        // keep the current note unless the voice clearly moved (avoids flicker between two notes)
        if (heldNote >= 0 && inScale (heldNote) && std::abs (note - (float) heldNote) < 0.65f)
            return heldNote;
        int bestNote = (int) std::round (note);
        float bestDist = 99.0f;
        for (int m = (int) std::floor (note) - 2; m <= (int) std::ceil (note) + 2; ++m)
            if (inScale (m) && std::abs ((float) m - note) < bestDist) { bestDist = std::abs ((float) m - note); bestNote = m; }
        heldNote = bestNote;
        return bestNote;
    }

    double sr = 44100.0, dsr = 11025.0;
    int decim = 4, decimCount = 0, tauMin = 11, tauMax = 160, win = 441, hop = 66, hopCount = 0;
    std::vector<float> ring, frame, diff;
    int ringPos = 0;
    StereoBiquad aaf1, aaf2;
    float rmsPow = 0.0f, period = 0.0f;
    bool voiced = false;
    int heldNote = -1;

    int keyRoot = 0, scaleType = 0;
    float retuneMs = 20.0f, amount = 1.0f;
    float shift = 0.0f, targetShift = 0.0f, lastShift = 0.0f, shiftRatio = 1.0f;

    DelayLine line;
    float grainA = 1323.0f, grainB = 1323.0f, phaseA = 0.25f, phaseB = 0.75f, maxGrain = 2646.0f, minGrain = 794.0f;
    juce::SmoothedValue<float> mix;
};
} // namespace tchk
