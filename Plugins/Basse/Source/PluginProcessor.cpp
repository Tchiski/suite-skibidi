#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace { juce::AudioParameterFloatAttributes pct() { return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + " %"; }); } }

BasseProcessor::BasseProcessor() : tchk::InstrumentProcessor (false), apvts (*this, nullptr, "BasseState", createLayout())
{
    apvts.state.setProperty ("root", 9, nullptr);
    apvts.state.setProperty ("scale", 1, nullptr);
    apvts.state.setProperty ("bars", 4, nullptr);
    apvts.state.setProperty ("style", 0, nullptr);
    apvts.state.setProperty ("follow", 0, nullptr);
    generate();
    startTimerHz (8);
}

juce::AudioProcessorValueTreeState::ParameterLayout BasseProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "sound", 1 }, "Son", tchk::sounds::bassSoundNames(), 0));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "length", 1 }, "Longueur", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct()));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "glide", 1 }, "Glide", juce::NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct()));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "dist", 1 }, "Distorsion", juce::NormalisableRange<float> (0.0f, 1.0f), 0.1f, pct()));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "volume", 1 }, "Volume", juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f, pct()));
    return l;
}

std::vector<tchk::gen::ChordEvent> BasseProcessor::getChords() const
{
    std::vector<tchk::gen::ChordEvent> out;
    for (auto& tok : juce::StringArray::fromTokens (apvts.state.getProperty ("chords").toString(), ";", ""))
    {
        auto f = juce::StringArray::fromTokens (tok, ":", "");
        if (f.size() == 3) out.push_back ({ f[0].getIntValue(), f[1].getIntValue(), f[2].getIntValue() });
    }
    return out;
}

void BasseProcessor::generate (const std::vector<tchk::gen::ChordEvent>* chords)
{
    const int bars = getInt ("bars", 4);
    auto ch = chords != nullptr ? *chords : tchk::gen::randomBassChords (bars, rng, lastProg);
    juce::StringArray s;
    for (auto& c : ch) s.add (juce::String (c.degree) + ":" + juce::String (c.start) + ":" + juce::String (c.length));
    apvts.state.setProperty ("chords", s.joinIntoString (";"), nullptr);
    setNotes (tchk::gen::generateBass (getInt ("root", 9), getInt ("scale", 1), bars, getInt ("style", 0), ch, rng));
}

tchk::Patch BasseProcessor::currentPatch()
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto p = tchk::sounds::bassPatch ((int) v ("sound"), v ("length"), v ("glide"), v ("dist"));
    p.level *= v ("volume") / 0.7f;
    return p;
}

bool BasseProcessor::handleIncomingMidi (const juce::MidiMessage& m, int)
{
    if (m.isNoteOn()) held[m.getNoteNumber()] = true;
    else if (m.isNoteOff()) held[m.getNoteNumber()] = false;
    return getInt ("follow", 0) != 0;   // following: the incoming chords are listened to, not played
}

void BasseProcessor::beforeSequence (juce::AudioPlayHead* ph, int n)
{
    if (getInt ("follow", 0) == 0) { lastHalf = -1; return; }
    double ppq = 0; bool playing = false;
    if (ph != nullptr) if (auto pos = ph->getPosition()) { playing = pos->getIsPlaying(); if (auto p = pos->getPpqPosition()) ppq = *p; else playing = false; }
    if (! playing) { lastHalf = -1; return; }
    const juce::SpinLock::ScopedTryLockType sl (followLock);
    if (! sl.isLocked()) return;
    halves = juce::jlimit (2, 32, getInt ("bars", 4) * 2);
    const double L = halves * 2.0;
    const int half = juce::jlimit (0, halves - 1, (int) std::floor (std::fmod (ppq, L) / 2.0));
    if (half != lastHalf)
    {
        if (half == 0 && lastHalf == halves - 1) { published = chroma; newChords = true; }   // one full loop heard
        chroma[(size_t) half].fill (0.0f);
        lastHalf = half;
    }
    for (int k = 0; k < 128; ++k) if (held[k]) chroma[(size_t) half][(size_t) (k % 12)] += (float) n;
}

std::vector<std::array<float, 12>> BasseProcessor::takeFollowed()
{
    const juce::SpinLock::ScopedLockType sl (followLock);
    newChords = false;
    return std::vector<std::array<float, 12>> (published.begin(), published.begin() + halves);
}

void BasseProcessor::timerCallback() { if (newChords.load()) followUpdate(); }

// a full loop of incoming chords has been heard: find the key, the chords, and make a bass line on them
void BasseProcessor::followUpdate()
{
    const auto data = takeFollowed();
    const auto heard = tchk::gen::chordsFromChroma (data);
    std::array<float, 12> hist {};
    for (auto& h : data) for (int k = 0; k < 12; ++k) hist[(size_t) k] += h[(size_t) k];
    float tot = 0; for (float v : hist) tot += v;
    if (tot < 1e-6f || heard.empty()) return;
    const auto key = tchk::music::detectKey (hist);
    const int scaleIdx = key.second ? 1 : 0;
    const auto& sc = tchk::music::scale (scaleIdx);
    std::vector<tchk::gen::ChordEvent> ev;
    juce::StringArray names; juce::String sig;
    for (auto& c : heard)
    {
        int best = 0, bestDist = 99;
        for (int d = 0; d < 7; ++d)
        {
            const int dist = std::abs (((key.first + sc[(size_t) d]) % 12) - c.root);
            if (juce::jmin (dist, 12 - dist) < bestDist) { bestDist = juce::jmin (dist, 12 - dist); best = d; }
        }
        ev.push_back ({ best, c.start, c.length });
        names.add (tchk::music::noteNames()[c.root] + (c.minor ? "m" : ""));
        sig << c.root << (c.minor ? "m" : "") << c.start << ",";
    }
    sig << key.first << scaleIdx;
    if (sig == lastFollowSig) return;
    lastFollowSig = sig;
    followText = names.joinIntoString (" - ");
    setInt ("root", key.first); setInt ("scale", scaleIdx);
    generate (&ev);
    ++stateVersion;
}

juce::AudioProcessorEditor* BasseProcessor::createEditor() { return new BasseEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BasseProcessor(); }
