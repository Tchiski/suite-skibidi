#pragma once

// Live spectrum for the EQ displays: the audio thread pushes samples, the editor reads a smoothed curve.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace tchk
{
class SpectrumAnalyser
{
public:
    static constexpr int order = 11, size = 1 << order;   // 2048-point FFT
    std::atomic<bool> wanted { false };

    // audio thread: mono mix of the block (cheap, lock-free)
    void push (const juce::AudioBuffer<float>& b)
    {
        if (! wanted.load()) return;
        const int n = b.getNumSamples(), ch = juce::jmin (2, b.getNumChannels());
        int s1, n1, s2, n2;
        fifo.prepareToWrite (n, s1, n1, s2, n2);
        auto write = [&] (int start, int count, int offset)
        {
            for (int i = 0; i < count; ++i)
            {
                float v = 0; for (int c = 0; c < ch; ++c) v += b.getSample (c, offset + i);
                data[(size_t) (start + i)] = v / (float) juce::jmax (1, ch);
            }
        };
        if (n1 > 0) write (s1, n1, 0);
        if (n2 > 0) write (s2, n2, n1);
        fifo.finishedWrite (n1 + n2);
    }

    // editor thread: returns true when the curve changed. levels[] = dB per display point (log frequency 20 Hz .. 20 kHz)
    bool update (double sampleRate, int points)
    {
        bool changed = false;
        while (fifo.getNumReady() >= size / 2)
        {
            int s1, n1, s2, n2;
            fifo.prepareToRead (size / 2, s1, n1, s2, n2);
            std::move (window.begin() + size / 2, window.end(), window.begin());
            for (int i = 0; i < n1; ++i) window[(size_t) (size / 2 + i)] = data[(size_t) (s1 + i)];
            for (int i = 0; i < n2; ++i) window[(size_t) (size / 2 + n1 + i)] = data[(size_t) (s2 + i)];
            fifo.finishedRead (n1 + n2);
            std::copy (window.begin(), window.end(), fftData.begin());
            std::fill (fftData.begin() + size, fftData.end(), 0.0f);
            hann.multiplyWithWindowingTable (fftData.data(), size);
            fft.performFrequencyOnlyForwardTransform (fftData.data());
            if ((int) levels.size() != points) levels.assign ((size_t) points, -100.0f);
            for (int p = 0; p < points; ++p)
            {
                const double f0 = 20.0 * std::pow (1000.0, (double) p / points), f1 = 20.0 * std::pow (1000.0, (double) (p + 1) / points);
                const int b0 = juce::jlimit (1, size / 2 - 1, (int) (f0 * size / sampleRate)), b1 = juce::jlimit (b0, size / 2 - 1, (int) (f1 * size / sampleRate));
                float m = 0; for (int k = b0; k <= b1; ++k) m = juce::jmax (m, fftData[(size_t) k]);
                const float db = juce::Decibels::gainToDecibels (m * 4.0f / (float) size, -100.0f) + 3.0f * std::log2 ((float) (f0 / 1000.0) + 1.0f); // gentle tilt: highs easier to see
                auto& l = levels[(size_t) p];
                l = db > l ? l + (db - l) * 0.6f : l + (db - l) * 0.12f;
            }
            changed = true;
        }
        return changed;
    }
    std::vector<float> levels;

private:
    juce::AbstractFifo fifo { 16384 };
    std::array<float, 16384> data {};
    std::array<float, size> window {};
    std::array<float, size * 2> fftData {};
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> hann { (size_t) size, juce::dsp::WindowingFunction<float>::hann };
};
} // namespace tchk
