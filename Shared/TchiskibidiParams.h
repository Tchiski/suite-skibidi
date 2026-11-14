#pragma once

// Small helpers to declare parameters with readable values (dB, %, Hz, ms...)

#include <juce_audio_processors/juce_audio_processors.h>

namespace tchk
{
enum class Unit { dB, percent, hz, ms, ratio, plain };

inline juce::String formatValue (float v, Unit unit)
{
    switch (unit)
    {
        case Unit::dB:      return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
        case Unit::percent: return juce::String (juce::roundToInt (v * 100.0f)) + " %";
        case Unit::hz:      return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
        case Unit::ms:      return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms";
        case Unit::ratio:   return juce::String (v, 1) + ":1";
        case Unit::plain:   return juce::String (v, 2);
    }
    return juce::String (v);
}

struct ParamList
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    void add (const char* id, const char* name, float lo, float hi, float def, Unit unit, float skew = 1.0f)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, 0.0f, skew), def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([unit] (float v, int) { return formatValue (v, unit); })));
    }
    void addBool (const char* id, const char* name, bool def)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    }
    void addChoice (const char* id, const char* name, const juce::StringArray& choices, int def)
    {
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, def));
    }
    juce::AudioProcessorValueTreeState::ParameterLayout build() { return { params.begin(), params.end() }; }
};

inline void saveState (juce::AudioProcessorValueTreeState& apvts, juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

inline void loadState (juce::AudioProcessorValueTreeState& apvts, const void* data, int size)
{
    if (auto xml = juce::AudioProcessor::getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

inline void setParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}
} // namespace tchk
