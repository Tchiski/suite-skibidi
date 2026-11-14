#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

// Tchiskibidi Space: tape-flavoured stereo delay + reverb.
//
// Signal flow:
//   input --> delay line (feedback, low/high cut, wobble, ping-pong) --> echoes
//   input + echoes --> reverb
//   output = dry + (echoes * echo level + reverb * reverb level), balanced by Mix
class TchiskibidiSpaceProcessor : public juce::AudioProcessor
{
public:
    TchiskibidiSpaceProcessor();
    ~TchiskibidiSpaceProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockInternal (juce::AudioBuffer<float>&, juce::MidiBuffer&);

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::StringArray getDivisionNames();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inputPeakDb { -100.0f }, outputPeakDb { -100.0f }; // for the meters / animated sky

private:
    float readDelay (int channel, float delayInSamples) const noexcept;
    float computeDelayMs() const;
    double getHostBpm() const;

    // Parameters (read on the audio thread)
    std::atomic<float>* timeParam = nullptr;
    std::atomic<float>* syncParam = nullptr;
    std::atomic<float>* divisionParam = nullptr;
    std::atomic<float>* feedbackParam = nullptr;
    std::atomic<float>* pingPongParam = nullptr;
    std::atomic<float>* lowCutParam = nullptr;
    std::atomic<float>* highCutParam = nullptr;
    std::atomic<float>* wobbleParam = nullptr;
    std::atomic<float>* echoLevelParam = nullptr;
    std::atomic<float>* sizeParam = nullptr;
    std::atomic<float>* dampingParam = nullptr;
    std::atomic<float>* widthParam = nullptr;
    std::atomic<float>* reverbLevelParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;

    // Delay state
    double currentSampleRate = 44100.0;
    std::array<std::vector<float>, 2> delayLines;
    int delayLineSize = 0;
    int writePosition = 0;
    float maxDelaySamples = 0.0f;
    std::array<float, 2> lowpassState {}, highpassState {};
    float lfoPhase = 0.0f;

    juce::SmoothedValue<float> delaySamplesSmoothed, feedbackSmoothed,
                               echoLevelSmoothed, reverbLevelSmoothed,
                               mixSmoothed, outputGainSmoothed;

    // Reverb
    juce::dsp::Reverb reverb;

    juce::AudioBuffer<float> echoBuffer, reverbBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TchiskibidiSpaceProcessor)
};
