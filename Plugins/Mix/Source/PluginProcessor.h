#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TchiskibidiDSP.h"
#include "TchiskibidiParams.h"

// Tchiskibidi Mix: complete channel strip for any track.
// Input -> Low cut -> 4-band EQ -> Compressor -> Transient shaper -> [2x OS: Exciter -> Clipper] -> Width -> Output
class MixProcessor : public juce::AudioProcessor
{
public:
    MixProcessor();
    ~MixProcessor() override = default;

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
    void getStateInformation (juce::MemoryBlock& d) override { tchk::saveState (apvts, d); }
    void setStateInformation (const void* data, int size) override { tchk::loadState (apvts, data, size); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::StringArray getPresetNames();
    void applyPreset (int index);

    // EQ coefficients for the editor's curve (computed from parameters)
    static std::array<tchk::BiquadCoeffs, 5> makeEqCoeffs (const juce::AudioProcessorValueTreeState&, double sampleRate);
    // MULTI mode: 10 fixed bands (graphic EQ)
    static constexpr int numGeqBands = 10;
    static constexpr std::array<float, numGeqBands> geqFreqs { 31.0f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
    static constexpr float geqQ = 1.4f;
    static juce::String geqId (int band) { return "geq_" + juce::String (band); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> compGrDb { 0.0f }, transientDb { 0.0f }, clipGrDb { 0.0f }, inputPeakDb { -100.0f }, outputPeakDb { -100.0f };
    std::atomic<float> compInDb { -100.0f }; // compressor detector level (for the curve display)

private:
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    double sr = 44100.0;
    int maxBlockSize = 512;
    std::array<tchk::StereoBiquad, 5> eq;    // low cut, low shelf, mid, high-mid, high shelf
    tchk::StereoBiquad lowCut2;               // second stage -> 24 dB/oct low cut
    std::array<float, 12> lastEqParams {};
    std::array<tchk::StereoBiquad, numGeqBands> geq;
    std::array<float, numGeqBands> lastGeq {};
    tchk::Compressor comp;
    float fastAttEnv = 0, slowAttEnv = 0, fastRelEnv = 0, slowRelEnv = 0;
    float kFastAtt = 0, kSlowAtt = 0, kRel = 0, kFastRel = 0, kSlowRel = 0, kRelAtt = 0;
    tchk::StereoBiquad exciterHp;
    float lastExcFreq = -1.0f;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::SmoothedValue<float> inGain, outGain, widthSm, compMix, excAmount, clipCeil;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixProcessor)
};
