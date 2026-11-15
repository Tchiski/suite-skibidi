#pragma once

#include "TchiskibidiInstrumentProcessor.h"
#include "TchiskibidiSounds.h"

// Tchiskibidi Synthé: 152 presets in 8 families, 4 big macros and 8 fine settings.
class SynthProcessor : public tchk::InstrumentProcessor
{
public:
    SynthProcessor();
    const juce::String getName() const override { return "Tchiskibidi Synthe"; }
    juce::AudioProcessorEditor* createEditor() override;

    juce::AudioProcessorValueTreeState apvts;
    static const juce::StringArray& tweakIds();
    static float neutral (const juce::String& id);
    void setParam (const juce::String& id, float value);   // real value, with host notification
    int presetIndex() const { return (int) apvts.getRawParameterValue ("preset")->load(); }
    void selectPreset (int index);                          // also puts the tweaks back to neutral

protected:
    tchk::Patch currentPatch() override;
    juce::ValueTree& stateTree() override { return apvts.state; }
    void replaceState (const juce::ValueTree& t) override { apvts.replaceState (t); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    int cachedIndex = -1;
    tchk::Patch cached;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthProcessor)
};
