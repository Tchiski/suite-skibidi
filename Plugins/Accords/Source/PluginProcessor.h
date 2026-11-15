#pragma once

#include "TchiskibidiInstrumentProcessor.h"
#include "TchiskibidiGenerators.h"
#include "TchiskibidiSounds.h"

// Tchiskibidi Accords: generates chord progressions in a key, plays them with the host
// transport (and sends them out as MIDI), can listen to your tracks to propose a matching progression.
class AccordsProcessor : public tchk::InstrumentProcessor
{
public:
    AccordsProcessor();
    const juce::String getName() const override { return "Tchiskibidi Accords"; }
    juce::AudioProcessorEditor* createEditor() override;

    juce::AudioProcessorValueTreeState apvts;

    // settings kept in the state (saved with the project and the presets)
    int getInt (const char* id, int def) const { return (int) apvts.state.getProperty (id, def); }
    void setInt (const char* id, int v) { apvts.state.setProperty (id, v, nullptr); }

    // new progression with the current settings (message thread)
    void generate (const std::vector<int>* avoid = nullptr);
    std::vector<int> lastDegrees;

    // listening: 0 off, 1 MIDI in, 2 audio sidechain
    void startListening (int mode);
    void stopListening() { listenMode = 0; }
    std::atomic<int> listenMode { 0 };
    std::atomic<bool> listenDone { false };
    std::atomic<float> listenProgress { 0.0f };
    std::vector<std::array<float, 12>> takeListenResult();

protected:
    tchk::Patch currentPatch() override;
    bool soundEnabled() override;
    void handleSidechain (juce::AudioBuffer<float>& sc) override;
    bool handleIncomingMidi (const juce::MidiMessage& m, int) override;
    void beforeSequence (juce::AudioPlayHead* ph, int n) override;
    juce::ValueTree& stateTree() override { return apvts.state; }
    void replaceState (const juce::ValueTree& t) override { apvts.replaceState (t); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::Random rng;
    int lastSound = -1;
    tchk::Patch soundPatch;
    // listening
    bool held[128] {};
    std::array<std::array<float, 12>, 32> chroma {};
    int listenHalves = 8, curHalf = -1;
    double listenStart = -1.0;
    tchk::ChromaAnalyzer analyzer;
    std::array<float, 12> scBlock {};
    juce::SpinLock listenLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AccordsProcessor)
};
