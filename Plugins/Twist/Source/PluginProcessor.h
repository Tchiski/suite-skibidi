#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "TchiskibidiUI.h"
#include "TchiskibidiDSP.h"

// Tchiskibidi Twist: effects placed at precise moments of the song (tape stop, slow down,
// stutter, reverse, filter sweeps, cut, echo throw, vinyl, pump, lo-fi), triggered by the host position.
namespace twist
{
    enum Fx { TapeStop, TapeStart, Slow, Stutter, Reverse, FilterDown, FilterUp, Cut, Echo, Vinyl, Pump, LoFi, NumFx };
    inline const juce::StringArray& fxNames()
    {
        static const juce::StringArray n { "Tape stop", "Tape start", "Ralenti", "Stutter", "Reverse", TK_FR ("Filtre ↓"), TK_FR ("Filtre ↑"), "Coupure", TK_FR ("Écho"), "Vinyle", "Pompe", "Lo-fi" };
        return n;
    }
    inline juce::Colour fxColour (int i)
    {
        static const juce::uint32 c[] = { 0xffff5f4d, 0xffffa94d, 0xffffd34d, 0xffc6ff3d, 0xff3dffc8, 0xff3dc8ff, 0xff7d8cff, 0xffb48cff, 0xffff8ad8, 0xffffb38a, 0xffff6b9e, 0xffd8d4e8 };
        return juce::Colour (c[juce::jlimit (0, NumFx - 1, i)]);
    }
    inline const juce::StringArray& fxHelp()
    {
        static const juce::StringArray h {
            TK_FR ("Le son ralentit jusqu'à l'arrêt, comme une cassette qu'on coupe."), TK_FR ("L'inverse : le son repart en accélérant."),
            TK_FR ("Tout passe au ralenti, plus grave."), TK_FR ("Répète un petit morceau en rafale (bégaiement)."), TK_FR ("Joue à l'envers ce qui vient de passer."),
            TK_FR ("Le son devient étouffé, comme sous l'eau."), TK_FR ("On enlève les basses : effet de montée avant un drop."), TK_FR ("Silence net : le drop qui fait respirer."),
            TK_FR ("La fin du passage part en écho."), TK_FR ("Le son ondule et grésille un peu, comme un vieux disque."), TK_FR ("Le son respire à chaque temps, comme un sidechain."),
            TK_FR ("Son plus petit et granuleux.") };
        return h;
    }
    struct Zone { int fx = 0; double start = 0, end = 1; float amount = 0.8f, curve = 0.5f, mix = 1.0f; }; // positions in beats from the song start

    inline juce::String toString (const std::vector<Zone>& z)
    {
        juce::StringArray a;
        for (auto& x : z) a.add (juce::String (x.fx) + ":" + juce::String (x.start, 3) + ":" + juce::String (x.end, 3) + ":" + juce::String (x.amount, 3) + ":" + juce::String (x.curve, 3) + ":" + juce::String (x.mix, 3));
        return a.joinIntoString (";");
    }
    inline std::vector<Zone> fromString (const juce::String& s)
    {
        std::vector<Zone> out;
        for (auto& tok : juce::StringArray::fromTokens (s, ";", ""))
        {
            auto f = juce::StringArray::fromTokens (tok, ":", "");
            if (f.size() < 6) continue;
            Zone z; z.fx = juce::jlimit (0, NumFx - 1, f[0].getIntValue()); z.start = f[1].getDoubleValue(); z.end = juce::jmax (z.start + 0.25, f[2].getDoubleValue());
            z.amount = (float) f[3].getDoubleValue(); z.curve = (float) f[4].getDoubleValue(); z.mix = (float) f[5].getDoubleValue();
            out.push_back (z);
        }
        return out;
    }
}

class TwistProcessor : public juce::AudioProcessor
{
public:
    TwistProcessor();
    const juce::String getName() const override { return "Tchiskibidi Twist"; }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    // zones, edited on the message thread
    std::vector<twist::Zone> getZones() const { return twist::fromString (apvts.state.getProperty ("zones").toString()); }
    void setZones (const std::vector<twist::Zone>& z);
    void sprinkle (int density, int bars);

    std::atomic<double> playBeat { -1.0 };
    std::atomic<bool> playing { false };
    std::atomic<int> activeZone { -1 };
    std::atomic<float> outputPeakDb { -100.0f };
    std::atomic<int> stateVersion { 0 };
    // a picture of the song built while it plays: level per quarter of a beat, for 128 bars
    static constexpr int thumbLen = 128 * 16;
    std::array<std::atomic<float>, thumbLen> thumb;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pushZones();
    float readAt (int ch, double pos) const;

    juce::SpinLock zoneLock;
    std::vector<twist::Zone> zonesRT, pendingZones;
    bool hasPending = false;
    double sr = 44100.0;
    // history (8 s) for the time effects
    juce::AudioBuffer<float> hist;
    int histLen = 1; juce::int64 wcount = 0;
    // zone playback state
    int curZone = -1; double zoneStartWrite = 0, readPos = 0, readRate = 1, zoneElapsed = 0, zoneLen = 1;
    float xfade = 1.0f; bool jumping = false;
    // filters, echo, vinyl
    juce::dsp::StateVariableTPTFilter<float> lp, hp;
    juce::AudioBuffer<float> echoBuf; int echoPos = 0; float echoFb = 0;
    double vinylPh = 0; float crush[2] {}; int crushCount = 0;
    juce::Random rng;
    juce::SmoothedValue<float> gainS, lpS, hpS;
    juce::AudioBuffer<float> dry;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwistProcessor)
};
