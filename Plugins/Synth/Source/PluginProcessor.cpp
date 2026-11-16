#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::AudioParameterFloatAttributes pct() { return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + " %"; }); }
}

const juce::StringArray& SynthProcessor::tweakIds()
{
    static const juce::StringArray ids { "bright", "space", "motion", "attack", "release", "width", "warmth", "vibrato", "glide", "filter", "reso", "detune" };
    return ids;
}
float SynthProcessor::neutral (const juce::String& id)
{
    static const float v[] = { 0.5f, 0.4f, 0.3f, 0.2f, 0.35f, 0.6f, 0.2f, 0.1f, 0.0f, 0.7f, 0.2f, 0.25f };
    const int i = tweakIds().indexOf (id);
    return i >= 0 ? v[i] : 0.5f;
}

SynthProcessor::SynthProcessor() : tchk::InstrumentProcessor (false), apvts (*this, nullptr, "SynthState", createLayout())
{
    apvts.state.setProperty ("bars", 4, nullptr);
    synth.outputGain = 0.32f;   // several notes of a wide pad must stay clean (no fizz)
    startTimerHz (4);
}

void SynthProcessor::setLink (bool on)
{
    apvts.state.setProperty ("link", on ? 1 : 0, nullptr);
    follower.reset();
    if (! on) { linkText = {}; setNotes ({}); }
    timerCallback();
}

void SynthProcessor::timerCallback()
{
    if (! linkOn()) return;
    tchk::link::Data d;
    if (! follower.poll (d)) return;
    apvts.state.setProperty ("bars", juce::jlimit (1, 16, d.steps / 16), nullptr);
    setNotes (d.notes);
    // names of the chords, for the editor
    juce::StringArray names; std::vector<int> starts;
    for (auto& n : d.notes) if (std::find (starts.begin(), starts.end(), n.start) == starts.end()) starts.push_back (n.start);
    std::sort (starts.begin(), starts.end());
    for (int st : starts)
    {
        std::vector<int> ps; for (auto& n : d.notes) if (n.start == st) ps.push_back (n.pitch);
        if (ps.size() >= 2 && names.size() < 8) names.add (tchk::music::chordName (ps));
    }
    linkText = names.joinIntoString (" - ");
}

juce::AudioProcessorValueTreeState::ParameterLayout SynthProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "preset", 1 }, "Preset", 0, tchk::sounds::count() - 1, 0));
    const juce::StringArray names { "Brillance", "Espace", "Mouvement", "Attaque", TK_FR ("Relâchement"), "Largeur", "Chaleur", "Vibrato", "Glide", "Filtre", TK_FR ("Résonance"), TK_FR ("Détune") };
    for (int i = 0; i < tweakIds().size(); ++i)
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { tweakIds()[i], 1 }, names[i], juce::NormalisableRange<float> (0.0f, 1.0f), neutral (tweakIds()[i]), pct()));
    l.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "octave", 1 }, "Octave", -2, 2, 0));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "mono", 1 }, "Mono", false));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "volume", 1 }, "Volume", juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f, pct()));
    return l;
}

void SynthProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void SynthProcessor::selectPreset (int index)
{
    setParam ("preset", (float) juce::jlimit (0, tchk::sounds::count() - 1, index));
    for (auto& id : tweakIds()) setParam (id, neutral (id));
}

tchk::Patch SynthProcessor::currentPatch()
{
    const int idx = presetIndex();
    if (idx != cachedIndex) { cached = tchk::sounds::makeSynthPatch (idx); cachedIndex = idx; }
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    tchk::sounds::Tweaks t;
    t.bright = v ("bright"); t.space = v ("space"); t.motion = v ("motion"); t.attack = v ("attack"); t.release = v ("release"); t.width = v ("width");
    t.warmth = v ("warmth"); t.vibrato = v ("vibrato"); t.glide = v ("glide"); t.filterEnv = v ("filter"); t.reso = v ("reso"); t.detune = v ("detune");
    t.octave = (int) v ("octave"); t.mono = v ("mono") > 0.5f; t.volume = v ("volume");
    auto patch = tchk::sounds::applyTweaks (cached, t);
    return patch;
}

juce::AudioProcessorEditor* SynthProcessor::createEditor() { return new SynthEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SynthProcessor(); }
