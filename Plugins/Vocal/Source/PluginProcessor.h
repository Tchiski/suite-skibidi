#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TchiskibidiDSP.h"
#include "TchiskibidiParams.h"
#include "AutoTune.h"

// Tchiskibidi Vocal: complete vocal chain.
// Input -> Autotune -> Auto-level -> Low cut -> De-esser -> EQ (mud, presence, air) -> Compressor -> Warmth
//       -> Doubler -> Delay (send) -> Reverb (send) -> Output
class VocalProcessor : public juce::AudioProcessor
{
public:
    VocalProcessor();
    ~VocalProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& d) override { tchk::saveState (apvts, d); }
    void setStateInformation (const void* data, int size) override { tchk::loadState (apvts, data, size); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::StringArray getPresetNames();
    static constexpr int numGenericPresets = 7; // the rest are artist-style presets
    static juce::StringArray getDivisionNames();
    void applyPreset (int index);
    static std::vector<tchk::BiquadCoeffs> makeEqCoeffs (const juce::AudioProcessorValueTreeState&, double sampleRate);

    juce::AudioProcessorValueTreeState apvts;
    tchk::AutoTune autotune;
    std::atomic<float> levelerDb { 0.0f }, deesserGrDb { 0.0f }, compGrDb { 0.0f }, inputPeakDb { -100.0f }, outputPeakDb { -100.0f };

private:
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    double hostBpm() const;

    double sr = 44100.0;
    int maxBlockSize = 512;

    // auto-level
    float levelPow = 0.0f, levelGainDb = 0.0f, kLevelPow = 0.0f, kLevelGain = 0.0f;
    // de-esser
    tchk::LRSplit deessSplit;
    tchk::StereoBiquad sibDetect;
    float sibEnv = 0.0f, fullEnv = 0.0f, deessGr = 0.0f, kEnvAtt = 0.0f, kEnvRel = 0.0f, lastDeessFreq = -1.0f;
    // eq
    std::array<tchk::StereoBiquad, 5> eq; // lowcut x2, mud, presence, air
    std::array<float, 4> lastEq {};
    // comp
    tchk::Compressor comp;
    float compAvgPow = 1.0e-4f, kCompAvg = 0.0f;
    // warmth
    tchk::DCBlocker dc;
    // doubler
    tchk::DelayLine doublerLine;
    float lfo1 = 0.0f, lfo2 = 0.0f;
    // delay
    tchk::DelayLine delayLine;
    tchk::StereoBiquad delayHp, delayLp;
    juce::SmoothedValue<float> delaySamples;
    // reverb
    juce::dsp::Reverb reverb;
    tchk::StereoBiquad reverbHp;
    juce::AudioBuffer<float> fxBuffer, reverbBuffer;

    juce::SmoothedValue<float> inGain, outGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalProcessor)
};
