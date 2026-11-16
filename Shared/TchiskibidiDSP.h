#pragma once

// Shared DSP building blocks for the Tchiskibidi suite.
// Header-only, allocation-free on the audio thread (everything is sized in prepare()).

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <complex>
#include <vector>

namespace tchk
{
constexpr float kPi = juce::MathConstants<float>::pi;

// Fast log2 / exp2 (polynomial approximations, ~1e-4 relative error = 0.001 dB):
// these run per sample in every dynamics processor, so they are worth the speed.
inline float fastLog2 (float x) noexcept
{
    std::uint32_t i; std::memcpy (&i, &x, 4);
    const float e = (float) ((int) ((i >> 23) & 255) - 127);
    i = (i & 0x007FFFFFu) | 0x3F800000u;
    float m; std::memcpy (&m, &i, 4);
    return e + 1.4426950f * (-1.7417939f + (2.8212026f + (-1.4699568f + (0.44717955f - 0.056570851f * m) * m) * m) * m);
}
inline float fastExp2 (float x) noexcept
{
    x = juce::jlimit (-126.0f, 126.0f, x);
    const float xi = std::floor (x), f = x - xi;
    const float p = 1.0f + f * (0.69314718f + f * (0.24022650f + f * (0.05550411f + f * (0.00961813f + f * 0.00133336f))));
    const std::uint32_t bits = (std::uint32_t) ((int) xi + 127) << 23;
    float scale; std::memcpy (&scale, &bits, 4);
    return p * scale;
}
inline float dbToGain (float db) noexcept { return fastExp2 (db * 0.16609640f); }           // 10^(db/20)
inline float gainToDb (float g, float floorDb = -120.0f) noexcept
{
    return g > 1.0e-6f ? juce::jmax (floorDb, 6.0205999f * fastLog2 (g)) : floorDb;      // 20 log10
}
inline float powerToDb (double p, float floorDb = -120.0f) noexcept
{
    return p > 1.0e-12 ? juce::jmax (floorDb, 3.0103f * fastLog2 ((float) p)) : floorDb;  // 10 log10
}
// One-pole smoothing coefficient for a time constant in ms
inline float onePoleCoef (double sampleRate, double ms) noexcept
{
    return ms <= 0.0 ? 0.0f : (float) std::exp (-1.0 / (0.001 * ms * sampleRate));
}

//==============================================================================
// Biquad (RBJ cookbook)
enum class FilterType { lowPass, highPass, bandPass, peak, lowShelf, highShelf, notch };

struct BiquadCoeffs
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

    static BiquadCoeffs make (FilterType type, double sampleRate, double freq, double q, double gainDb = 0.0)
    {
        freq = juce::jlimit (5.0, sampleRate * 0.49, freq);
        q = juce::jmax (0.05, q);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

        switch (type)
        {
            case FilterType::lowPass:  b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case FilterType::highPass: b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case FilterType::bandPass: b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case FilterType::notch:    b0 = 1; b1 = -2 * cw; b2 = 1; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case FilterType::peak:
                b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
                a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
            case FilterType::lowShelf:
            {
                const double s = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) - (A - 1) * cw + s); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - s);
                a0 = (A + 1) + (A - 1) * cw + s; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - s; break;
            }
            case FilterType::highShelf:
            {
                const double s = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) + (A - 1) * cw + s); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - s);
                a0 = (A + 1) - (A - 1) * cw + s; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - s; break;
            }
        }

        BiquadCoeffs c;
        c.b0 = (float) (b0 / a0); c.b1 = (float) (b1 / a0); c.b2 = (float) (b2 / a0);
        c.a1 = (float) (a1 / a0); c.a2 = (float) (a2 / a0);
        return c;
    }

    // Magnitude response in dB (for drawing curves)
    float magnitudeDb (double freq, double sampleRate) const
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
        const auto num = (double) b0 + (double) b1 * z1 + (double) b2 * z2;
        const auto den = 1.0 + (double) a1 * z1 + (double) a2 * z2;
        return (float) (20.0 * std::log10 (std::abs (num / den) + 1.0e-12));
    }
};

// Biquad with independent state for up to 2 channels (transposed direct form II)
struct StereoBiquad
{
    BiquadCoeffs c;
    float z1[2] {}, z2[2] {};

    void set (const BiquadCoeffs& coeffs) noexcept { c = coeffs; }
    void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }

    inline float process (int ch, float x) noexcept
    {
        const float y = c.b0 * x + z1[ch];
        z1[ch] = c.b1 * x - c.a1 * y + z2[ch];
        z2[ch] = c.b2 * x - c.a2 * y;
        return y;
    }
};

//==============================================================================
// 2-band Linkwitz-Riley split (4th order) for up to 2 channels
struct LRSplit
{
    juce::dsp::LinkwitzRileyFilter<float> f;

    void prepare (double sampleRate, float freq)
    {
        f.prepare ({ sampleRate, 512, 2 });
        f.setCutoffFrequency (freq);
        f.reset();
    }
    inline void process (int ch, float x, float& low, float& high) noexcept { f.processSample (ch, x, low, high); }
};

// 3-band crossover with allpass compensation (bands sum back flat in magnitude and phase-aligned)
struct Crossover3
{
    LRSplit split1, split2, allpass;

    void prepare (double sampleRate, float f1, float f2)
    {
        split1.prepare (sampleRate, f1);
        split2.prepare (sampleRate, f2);
        allpass.prepare (sampleRate, f2);
    }
    inline void process (int ch, float x, float& low, float& mid, float& high) noexcept
    {
        float l, rest, apL, apH;
        split1.process (ch, x, l, rest);
        split2.process (ch, rest, mid, high);
        allpass.process (ch, l, apL, apH);
        low = apL + apH;
    }
};

//==============================================================================
// Feed-forward compressor gain computer with soft knee, smoothing in the gain domain.
struct Compressor
{
    float thresholdDb = -18.0f, ratio = 2.0f, kneeDb = 6.0f;
    float attackCoef = 0.0f, releaseCoef = 0.0f;
    float grDb = 0.0f; // current gain reduction (<= 0)

    void setTimes (double sampleRate, float attackMs, float releaseMs) noexcept
    {
        attackCoef = onePoleCoef (sampleRate, attackMs);
        releaseCoef = onePoleCoef (sampleRate, releaseMs);
    }
    void reset() noexcept { grDb = 0.0f; }

    float staticCurve (float levelDb) const noexcept
    {
        const float over = levelDb - thresholdDb;
        const float slope = 1.0f / ratio - 1.0f;
        if (2.0f * over < -kneeDb) return 0.0f;
        if (2.0f * std::abs (over) <= kneeDb)
        {
            const float t = over + kneeDb * 0.5f;
            return slope * t * t / (2.0f * kneeDb);
        }
        return slope * over;
    }

    // levelDb: detector level. Returns smoothed gain change in dB (<= 0)
    inline float process (float levelDb) noexcept
    {
        const float target = staticCurve (levelDb);
        const float coef = target < grDb ? attackCoef : releaseCoef;
        grDb = target + coef * (grDb - target);
        return grDb;
    }
};

//==============================================================================
// Saturation curves
enum class SatMode { tape = 0, tube = 1, crunch = 2 };

inline float saturate (float x, float drive, SatMode mode) noexcept
{
    switch (mode)
    {
        case SatMode::tape:   return std::tanh (drive * x) / drive;
        case SatMode::tube:
        {
            const float bias = 0.15f;
            return (std::tanh (drive * (x + bias)) - std::tanh (drive * bias)) / drive;
        }
        case SatMode::crunch: return std::atan (drive * x) / drive;
    }
    return x;
}

// Soft clipper: linear below knee point, smooth tanh shoulder up to the ceiling
inline float softClip (float x, float ceiling) noexcept
{
    const float k = 0.75f * ceiling;
    const float a = std::abs (x);
    if (a <= k) return x;
    const float y = k + (ceiling - k) * std::tanh ((a - k) / (ceiling - k));
    return x < 0.0f ? -y : y;
}

struct DCBlocker
{
    float x1[2] {}, y1[2] {};
    float r = 0.9995f;
    void prepare (double sampleRate) { r = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 10.0 / sampleRate); x1[0] = x1[1] = y1[0] = y1[1] = 0; }
    inline float process (int ch, float x) noexcept
    {
        const float y = x - x1[ch] + r * y1[ch];
        x1[ch] = x; y1[ch] = y;
        return y;
    }
};

//==============================================================================
// Look-ahead brickwall limiter (stereo linked). Sliding-minimum + box filter
// guarantees the gain is already down when the peak arrives: no overshoot.
class LookaheadLimiter
{
public:
    void prepare (double sampleRate, int lookaheadSamples)
    {
        sr = sampleRate;
        L = juce::jmax (1, lookaheadSamples);
        for (auto& d : delay) d.assign ((size_t) L, 0.0f);
        boxRing.assign ((size_t) L, 1.0f);
        dqVal.assign ((size_t) L + 4, 1.0f);
        dqIdx.assign ((size_t) L + 4, 0);
        reset();
        setRelease (80.0f);
    }

    void reset()
    {
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (boxRing.begin(), boxRing.end(), 1.0f);
        boxSum = (double) L;
        boxPos = delayPos = 0;
        dqHead = dqCount = 0;
        sampleIndex = 0;
        env = 1.0f;
    }

    void setCeiling (float linear) noexcept { ceiling = juce::jmax (1.0e-4f, linear); }
    void setRelease (float ms) noexcept { releaseCoef = onePoleCoef (sr, ms); }
    int getLatency() const noexcept { return L; }

    // Processes in place. Returns the minimum gain applied in this block.
    float process (float* const* data, int numChannels, int numSamples) noexcept
    {
        const int cap = (int) dqVal.size();
        float minGain = 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = juce::jmax (peak, std::abs (data[ch][i]));

            const float req = peak > ceiling ? ceiling / peak : 1.0f;

            // monotonic deque: sliding minimum over the last L+1 samples
            while (dqCount > 0 && dqVal[(size_t) ((dqHead + dqCount - 1) % cap)] >= req)
                --dqCount;
            const int back = (dqHead + dqCount) % cap;
            dqVal[(size_t) back] = req;
            dqIdx[(size_t) back] = sampleIndex;
            ++dqCount;
            while (dqIdx[(size_t) dqHead] <= sampleIndex - (L + 1))
            {
                dqHead = (dqHead + 1) % cap;
                --dqCount;
            }
            const float m = dqVal[(size_t) dqHead];

            // box filter of length L
            boxSum += (double) m - (double) boxRing[(size_t) boxPos];
            boxRing[(size_t) boxPos] = m;
            if (++boxPos >= L) boxPos = 0;
            const float s = juce::jmin (1.0f, (float) (boxSum / (double) L));

            // release: always stays <= s, so never overshoots
            env = s < env ? s : s + (env - s) * releaseCoef;
            minGain = juce::jmin (minGain, env);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float delayed = delay[(size_t) ch][(size_t) delayPos];
                delay[(size_t) ch][(size_t) delayPos] = data[ch][i];
                data[ch][i] = delayed * env;
            }
            if (++delayPos >= L) delayPos = 0;
            ++sampleIndex;

            if ((sampleIndex & 0xFFFF) == 0) // fight float drift of the running sum
            {
                double sum = 0.0;
                for (auto v : boxRing) sum += v;
                boxSum = sum;
            }
        }
        return minGain;
    }

private:
    double sr = 44100.0;
    int L = 1;
    std::array<std::vector<float>, 2> delay;
    std::vector<float> boxRing, dqVal;
    std::vector<long long> dqIdx;
    double boxSum = 1.0;
    int boxPos = 0, delayPos = 0, dqHead = 0, dqCount = 0;
    long long sampleIndex = 0;
    float env = 1.0f, ceiling = 1.0f, releaseCoef = 0.99f;
};

//==============================================================================
// ITU-R BS.1770 / EBU R128 loudness meter: momentary, short-term, integrated (gated).
class LoudnessMeter
{
public:
    std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // K-weighting stage 1 (high shelf) and stage 2 (high pass), valid for any sample rate
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / sr);
            const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.c.b0 = (float) ((Vh + Vb * K / Q + K * K) / a0);
            shelf.c.b1 = (float) (2.0 * (K * K - Vh) / a0);
            shelf.c.b2 = (float) ((Vh - Vb * K / Q + K * K) / a0);
            shelf.c.a1 = (float) (2.0 * (K * K - 1.0) / a0);
            shelf.c.a2 = (float) ((1.0 - K / Q + K * K) / a0);
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / sr);
            const double a0 = 1.0 + K / Q + K * K;
            hp.c.b0 = 1.0f; hp.c.b1 = -2.0f; hp.c.b2 = 1.0f;
            hp.c.a1 = (float) (2.0 * (K * K - 1.0) / a0);
            hp.c.a2 = (float) ((1.0 - K / Q + K * K) / a0);
        }
        blockLength = juce::jmax (1, (int) std::round (sr * 0.1));
        resetAll();
    }

    void requestIntegratedReset() noexcept { resetRequested.store (true); }

    void process (const float* const* data, int numChannels, int numSamples) noexcept
    {
        if (resetRequested.exchange (false))
            resetIntegratedState();

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
            {
                const float k = hp.process (ch, shelf.process (ch, data[ch][i]));
                blockSum += (double) k * (double) k;
            }
            if (++blockCount >= blockLength)
                finishBlock();
        }
    }

private:
    void resetAll()
    {
        shelf.reset(); hp.reset();
        blocks.fill (0.0); blockPos = 0; blocksFilled = 0;
        blockSum = 0.0; blockCount = 0;
        resetIntegratedState();
        momentary = shortTerm = -100.0f;
    }

    void resetIntegratedState()
    {
        histCount.fill (0); histPower.fill (0.0);
        integrated = -100.0f;
    }

    static float toLufs (double power) noexcept { return power > 1.0e-12 ? (float) (-0.691 + 10.0 * std::log10 (power)) : -100.0f; }

    void finishBlock() noexcept
    {
        blocks[(size_t) blockPos] = blockSum / (double) blockLength;
        blockPos = (blockPos + 1) % (int) blocks.size();
        blocksFilled = juce::jmin (blocksFilled + 1, (int) blocks.size());
        blockSum = 0.0; blockCount = 0;

        auto meanOfLast = [this] (int n)
        {
            n = juce::jmin (n, blocksFilled);
            if (n == 0) return 0.0;
            double s = 0.0;
            for (int k = 1; k <= n; ++k)
                s += blocks[(size_t) ((blockPos - k + (int) blocks.size()) % (int) blocks.size())];
            return s / n;
        };

        const double mPower = meanOfLast (4);
        momentary = toLufs (mPower);
        shortTerm = toLufs (meanOfLast (30));

        // integrated: histogram of 400 ms block loudness (0.1 LU bins from -70 to +10)
        const float mL = toLufs (mPower);
        if (blocksFilled >= 4 && mL > -70.0f)
        {
            const int bin = juce::jlimit (0, (int) histCount.size() - 1, (int) ((mL + 70.0f) * 10.0f));
            ++histCount[(size_t) bin];
            histPower[(size_t) bin] += mPower;
        }

        double sum = 0.0; long long count = 0;
        for (size_t b = 0; b < histCount.size(); ++b) { sum += histPower[b]; count += histCount[b]; }
        if (count == 0) { integrated = -100.0f; return; }

        const float relGate = toLufs (sum / (double) count) - 10.0f;
        const int startBin = juce::jlimit (0, (int) histCount.size() - 1, (int) std::ceil ((relGate + 70.0f) * 10.0f));
        sum = 0.0; count = 0;
        for (size_t b = (size_t) startBin; b < histCount.size(); ++b) { sum += histPower[b]; count += histCount[b]; }
        integrated = count > 0 ? toLufs (sum / (double) count) : -100.0f;
    }

    double sr = 48000.0;
    StereoBiquad shelf, hp;
    std::array<double, 30> blocks {};
    int blockPos = 0, blocksFilled = 0, blockLength = 4800, blockCount = 0;
    double blockSum = 0.0;
    std::array<long long, 800> histCount {};
    std::array<double, 800> histPower {};
    std::atomic<bool> resetRequested { false };
};

//==============================================================================
// Simple stereo delay line with fractional read (for doublers, delays)
struct DelayLine
{
    std::array<std::vector<float>, 2> buf;
    int size = 1, pos = 0;

    void prepare (int maxSamples)
    {
        size = juce::jmax (4, maxSamples + 4);
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        pos = 0;
    }
    void reset() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); }

    inline void write (int ch, float x) noexcept { buf[(size_t) ch][(size_t) pos] = x; }
    inline void advance() noexcept { if (++pos >= size) pos = 0; }

    inline float read (int ch, float delaySamples) const noexcept
    {
        delaySamples = juce::jlimit (1.0f, (float) (size - 3), delaySamples);
        float rp = (float) pos - delaySamples;
        if (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp;
        const float frac = rp - (float) i0;
        const int i1 = (i0 + 1) % size;
        const auto& b = buf[(size_t) ch];
        return b[(size_t) i0] + frac * (b[(size_t) i1] - b[(size_t) i0]);
    }
};


// Safety net at the end of every processBlock: a broken value (NaN, infinity, absurd level)
// never reaches the speakers; the block is muted instead. Returns true when it had to act.
inline bool sanitize (juce::AudioBuffer<float>& b)
{
    bool bad = false;
    for (int c = 0; c < b.getNumChannels() && ! bad; ++c)
    {
        const float* d = b.getReadPointer (c);
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (d[i]) || std::abs (d[i]) > 8.0f) { bad = true; break; }
    }
    if (bad) b.clear();
    return bad;
}
} // namespace tchk
