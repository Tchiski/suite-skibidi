#pragma once

#include "TchiskibidiInstrumentProcessor.h"
#include "TchiskibidiGenerators.h"
#include "TchiskibidiSounds.h"
#include "TchiskibidiLink.h"

// Tchiskibidi Basse: 808 and bass lines in a key and a style, played with the host transport.
// "Suivre mes accords": listens to the chords coming in as MIDI (e.g. from Tchiskibidi Accords) and follows them.
class BasseProcessor : public tchk::InstrumentProcessor, private juce::Timer
{
public:
    BasseProcessor();
    ~BasseProcessor() override { stopTimer(); }
    const juce::String getName() const override { return "Tchiskibidi Basse"; }
    juce::AudioProcessorEditor* createEditor() override;

    juce::AudioProcessorValueTreeState apvts;
    int getInt (const char* id, int def) const { return (int) apvts.state.getProperty (id, def); }
    void setInt (const char* id, int v) { apvts.state.setProperty (id, v, nullptr); }

    void generate (const std::vector<tchk::gen::ChordEvent>* chords = nullptr);
    std::vector<tchk::gen::ChordEvent> getChords() const;

    // following the incoming chords
    std::atomic<bool> newChords { false };
    std::vector<std::array<float, 12>> takeFollowed();
    juce::String followText;   // "Lam - Fa - Do - Sol" once chords have been heard

protected:
    tchk::Patch currentPatch() override;
    bool handleIncomingMidi (const juce::MidiMessage& m, int) override;
    void beforeSequence (juce::AudioPlayHead* ph, int n) override;
    void onNotePlayed (int, float, bool external) override { if (! external) ++notesPlayed; }
    juce::ValueTree& stateTree() override { return apvts.state; }
    void replaceState (const juce::ValueTree& t) override { apvts.replaceState (t); }

public:
    std::atomic<int> notesPlayed { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void followUpdate();
    void followFrom (const std::vector<std::array<float, 12>>& data, int knownRoot, int knownScale);
    tchk::link::Follower linkFollower;
    bool linkUsed = false;
    juce::String lastFollowSig;
    juce::Random rng;
    std::vector<int> lastProg;
    bool held[128] {};
    std::array<std::array<float, 12>, 32> chroma {}, published {};
    int halves = 8, lastHalf = -1;
    juce::SpinLock followLock;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasseProcessor)
};
