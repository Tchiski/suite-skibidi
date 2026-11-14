#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TchiskibidiDSP.h"

// Tchiskibidi Master: complete mastering chain.
// Input -> Spectral (auto tonal balance) -> Comp (3-band glue + detail) -> EQ -> Stereo
//       -> [4x oversampled: Analog -> Clip -> True-peak limiter] -> Output
class MasterProcessor : public juce::AudioProcessor,
                        private juce::Timer
{
public:
    static constexpr int numSpectralBands = 6;
    static constexpr std::array<float, numSpectralBands> spectralFreqs { 60.0f, 180.0f, 500.0f, 1500.0f, 4500.0f, 12000.0f };

    MasterProcessor();
    ~MasterProcessor() override;

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

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::StringArray getTargetNames();
    static float getTargetLufs (int index);
    static juce::StringArray getPresetNames();

    void applyPreset (int index);
    void switchAB();
    void copyToOtherSlot();
    int getCurrentSlot() const noexcept { return currentSlot; }

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    // --- Meters, read by the editor ---
    tchk::LoudnessMeter inputLoudness, outputLoudness;
    std::atomic<float> inputPeakDb { -100.0f }, truePeakDb { -100.0f }, truePeakHoldDb { -100.0f };
    std::atomic<float> limiterGrDb { 0.0f }, clipGrDb { 0.0f }, correlation { 1.0f };
    std::array<std::atomic<float>, 3> bandGrDb {};
    std::array<std::atomic<float>, numSpectralBands> spectralGainDb {};

    // Limiter quality (settings wheel): true = 4x oversampling, false = 2x (less CPU)
    void setOversamplingHQ (bool hq);
    bool isOversamplingHQ() const noexcept { return osFactorLog2 == 2; }

    // SPICE MOI CA BABY: one macro that sets the whole master (called from the editor)
    void applySpice (float amount);

    // Vectorscope feed (only filled while the editor shows it)
    static constexpr int scopeSize = 2048;
    std::atomic<bool> scopeWanted { false };
    juce::AbstractFifo scopeFifo { scopeSize };
    std::array<float, scopeSize> scopeL {}, scopeR {};

private:
    void timerCallback() override; // AUTO loudness targeting
    void setParam (const juce::String& id, float value);
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void updateSpectral (float amount);
    void setSpectralFilters();

    double sr = 44100.0;
    int maxBlockSize = 512;
    int osFactorLog2 = 2; // 4x
    int latencySamples = 0;

    // Spectral
    std::array<tchk::StereoBiquad, numSpectralBands> spectralAnalysis, spectralFilters;
    std::array<float, numSpectralBands> bandEnergy {}, spectralCorr {};
    int spectralCounter = 0;
    float spectralEnergyCoef = 0.0f, spectralSmoothK = 0.0f;

    // Comp
    tchk::Crossover3 crossover;
    std::array<tchk::Compressor, 3> compressors;
    std::array<float, 3> bandAvgPow {}, bandFastPow {}, upwardDb {};
    float avgCoef = 0.0f, fastCoef = 0.0f, upAttK = 0.0f, upRelK = 0.0f;

    // EQ + Stereo
    tchk::StereoBiquad bassShelf, airShelf;
    float lastBass = 99.0f, lastAir = 99.0f;
    tchk::LRSplit monoSplit;

    // Oversampled section
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    tchk::DCBlocker dcBlocker;
    tchk::LookaheadLimiter limiter;

    // Bypass / match
    tchk::DelayLine dryDelay;
    juce::AudioBuffer<float> dryBuffer;

    juce::SmoothedValue<float> inputGain, outputGain, widthSmoothed, bypassMix, matchGain;
    juce::SmoothedValue<float> driveSmoothed, clipGainSmoothed, limGainSmoothed;

    float corrLR = 0.0f, corrLL = 0.0f, corrRR = 0.0f, corrCoef = 0.0f;
    float peakHold = -100.0f;
    int peakHoldSamples = 0;

    std::array<juce::ValueTree, 2> abSlots;
    int currentSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterProcessor)
};
