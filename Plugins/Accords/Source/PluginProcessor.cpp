#include "PluginProcessor.h"
#include "PluginEditor.h"

AccordsProcessor::AccordsProcessor()
    : tchk::InstrumentProcessor (true), apvts (*this, nullptr, "AccordsState", createLayout())
{
    apvts.state.setProperty ("root", 9, nullptr);
    apvts.state.setProperty ("scale", 1, nullptr);
    apvts.state.setProperty ("bars", 4, nullptr);
    apvts.state.setProperty ("mood", 0, nullptr);
    apvts.state.setProperty ("seventh", 0, nullptr);
    synth.outputGain = 0.3f;   // 3 or 4 notes at once: keep headroom
    generate();
}

juce::AudioProcessorValueTreeState::ParameterLayout AccordsProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "sound", 1 }, "Son", tchk::sounds::chordSoundNames(), 0));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "volume", 1 }, "Volume", juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + " %"; })));
    return l;
}

void AccordsProcessor::generate (const std::vector<int>* avoid)
{
    auto res = tchk::gen::generateChords (getInt ("root", 9), getInt ("scale", 1), getInt ("bars", 4), getInt ("mood", 0), getInt ("seventh", 0) != 0, rng, avoid, &lastDegrees);
    lastDegrees = res.degrees;
    setNotes (res.notes);
}

tchk::Patch AccordsProcessor::currentPatch()
{
    const int s = (int) apvts.getRawParameterValue ("sound")->load();
    if (s != lastSound) { soundPatch = tchk::sounds::chordPatch (s); lastSound = s; }
    auto p = soundPatch;
    p.level *= apvts.getRawParameterValue ("volume")->load() / 0.7f;
    return p;
}
bool AccordsProcessor::soundEnabled() { return (int) apvts.getRawParameterValue ("sound")->load() != tchk::sounds::chordSoundNames().size() - 1; }

void AccordsProcessor::startListening (int mode)
{
    const juce::SpinLock::ScopedLockType sl (listenLock);
    for (auto& c : chroma) c.fill (0.0f);
    listenHalves = juce::jlimit (2, 32, getInt ("bars", 4) * 2);
    listenStart = -1.0; curHalf = -1; listenDone = false; listenProgress = 0.0f;
    listenMode = mode;
}

std::vector<std::array<float, 12>> AccordsProcessor::takeListenResult()
{
    const juce::SpinLock::ScopedLockType sl (listenLock);
    return std::vector<std::array<float, 12>> (chroma.begin(), chroma.begin() + listenHalves);
}

bool AccordsProcessor::handleIncomingMidi (const juce::MidiMessage& m, int)
{
    if (m.isNoteOn()) held[m.getNoteNumber()] = true;
    else if (m.isNoteOff()) held[m.getNoteNumber()] = false;
    return listenMode.load() == 1; // while listening to MIDI, the incoming notes are not played
}

void AccordsProcessor::handleSidechain (juce::AudioBuffer<float>& sc)
{
    if (listenMode.load() != 2) return;
    analyzer.push (sc, sr, scBlock);
}

void AccordsProcessor::beforeSequence (juce::AudioPlayHead* ph, int n)
{
    const int mode = listenMode.load();
    if (mode == 0 || listenDone.load()) { scBlock.fill (0.0f); return; }
    double ppq = 0; bool playing = false;
    if (ph != nullptr) if (auto pos = ph->getPosition()) { playing = pos->getIsPlaying(); if (auto p = pos->getPpqPosition()) ppq = *p; else playing = false; }
    if (! playing) { scBlock.fill (0.0f); return; }
    const juce::SpinLock::ScopedTryLockType sl (listenLock);
    if (! sl.isLocked()) return;
    if (listenStart < 0) listenStart = std::floor (ppq / 4.0) * 4.0;   // start at the bar
    const int half = (int) std::floor ((ppq - listenStart) / 2.0);
    if (half < 0) return;
    if (half >= listenHalves) { listenDone = true; listenProgress = 1.0f; return; }
    curHalf = half;
    auto& c = chroma[(size_t) half];
    if (mode == 1) { for (int k = 0; k < 128; ++k) if (held[k]) c[(size_t) (k % 12)] += (float) n; }
    else { for (int k = 0; k < 12; ++k) c[(size_t) k] += scBlock[(size_t) k]; }
    scBlock.fill (0.0f);
    listenProgress = (float) ((ppq - listenStart) / (listenHalves * 2.0));
}

juce::AudioProcessorEditor* AccordsProcessor::createEditor() { return new AccordsEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AccordsProcessor(); }
