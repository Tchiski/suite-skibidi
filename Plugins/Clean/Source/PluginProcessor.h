#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TchiskibidiDSP.h"
#include "TchiskibidiParams.h"

// Spectral noise reduction (STFT, 2048 points, 75% overlap) with a learnable noise profile
// and an adaptive noise-floor tracker.
class SpectralDenoiser
{
public:
    static constexpr float kAdaptiveBias = 2.2f; // minimum tracking under-estimates the mean noise power
    static constexpr int order = 11, size = 1 << order, hop = size / 4, numBins = size / 2 + 1;

    SpectralDenoiser() : fft (order) {}

    void prepare();
    void reset();
    // Returns the delayed, denoised sample. Latency = size samples.
    float process (float x) noexcept;

    // shared settings, written once per block
    float floorGain = 0.1f;     // maximum attenuation
    float sensitivity = 1.5f;   // threshold multiplier over the noise floor
    bool enabled = true, learning = false, adaptive = true;

    std::vector<float>* noiseProfile = nullptr; // learned profile (shared between channels)
    std::vector<float>* learnAccum = nullptr;
    int* learnFrames = nullptr;
    std::vector<float>* spectrumOut = nullptr;  // optional: input power for the display
    const std::vector<float>& getAdaptiveNoise() const noexcept { return adaptiveNoise; }

private:
    void processFrame() noexcept;

    juce::dsp::FFT fft;
    std::vector<float> window, inRing, outRing, frame, adaptiveNoise, smoothGain, powerBuf, smoothPower;
    int pos = 0, hopCount = 0;
};

// Tchiskibidi Clean: audio repair. Low cut -> De-hum -> De-click -> De-noise -> Output
class CleanProcessor : public juce::AudioProcessor,
                       private juce::Timer
{
public:
    static constexpr int displayBins = 96;

    CleanProcessor();
    ~CleanProcessor() override { stopTimer(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& d) override;
    void setStateInformation (const void* data, int size) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    bool hasLearnedProfile() const noexcept { return profileReady.load(); }
    void clearProfile();

    juce::AudioProcessorValueTreeState apvts;

    // display data (log-spaced bins, dB)
    std::array<std::atomic<float>, displayBins> displayInput {}, displayNoise {};
    std::atomic<int> clicksRepaired { 0 };
    std::atomic<float> learnProgress { 0.0f };

private:
    void timerCallback() override;
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void updateHumFilters (float baseFreq, int harmonics);

    double sr = 44100.0;
    int maxBlockSize = 512;

    tchk::StereoBiquad lowCut1, lowCut2;
    float lastLowCut = -1.0f;

    static constexpr int maxHarmonics = 8;
    std::array<tchk::StereoBiquad, maxHarmonics> humNotches;
    float lastHumFreq = -1.0f; int lastHarmonics = -1;

    // de-click (look-ahead repair)
    static constexpr int clickDelay = 64, clickRing = 256;
    std::array<std::array<float, clickRing>, 2> clickBuf {};
    std::array<bool, clickRing> clickFlag {};
    int clickPos = 0;
    std::array<float, 2> prev1 {}, prev2 {}, lastOut {};
    float errEnv = 1.0e-6f, kErr = 0.0f;
    int flagUntil = -1;
    struct Repair { int remaining = 0, length = 0; float start[2] {}, end[2] {}; } repair;
    long long sampleCounter = 0;

    // de-noise
    std::array<SpectralDenoiser, 2> denoisers;
    std::vector<float> noiseProfile, learnAccum, spectrum;
    int learnFrames = 0;
    enum LearnState { idle, learningNow, done };
    std::atomic<int> learnState { idle };
    std::atomic<bool> profileReady { false };
    juce::SpinLock profileLock;
    tchk::DelayLine dryDelay;
    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float> outGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CleanProcessor)
};
