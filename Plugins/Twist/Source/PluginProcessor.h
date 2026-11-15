#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "TchiskibidiUI.h"
#include "TchiskibidiDSP.h"

// Tchiskibidi Twist: effects placed at precise moments of the song (tape stop, slow down,
// stutter, reverse, filter sweeps, cut, echo throw, vinyl, pump, lo-fi), triggered by the host position.
namespace twist
{
    enum Fx { TapeStop, TapeStart, BulletTime, Rewind, Scratch, Skip, Accordion, Freeze, Reverse,
              Roll, Stutter, Hiccup, Glitch, Bug8,
              Helium, Demon, Robot, Elevator, Arp, PitchDrop,
              Spin, BlackHole, Ghost, Aquarium, Radio, NumFx };
    inline const juce::StringArray& fxNames()
    {
        static const juce::StringArray n { "Tape stop", "Tape start", "Bullet time", "Rewind", "Scratch", TK_FR ("Disque rayé"), TK_FR ("Accordéon"), "Freeze", "Reverse",
                                           "Rafale", "Stutter", "Hoquet", "Glitch", "Bug 8-bit",
                                           TK_FR ("Hélium"), TK_FR ("Démon"), "Robot", "Ascenseur", TK_FR ("Arpège fou"), "Pitch chute",
                                           "Toupie", "Trou noir", TK_FR ("Fantôme"), "Aquarium", "Station radio" };
        return n;
    }
    // families: first effect of each family, and its name
    inline const std::vector<std::pair<int, juce::String>>& families()
    {
        static const std::vector<std::pair<int, juce::String>> f { { TapeStop, "TEMPS" }, { Roll, "HACHAGE" }, { Helium, "VOIX FOLLES" }, { Spin, "ESPACE FOU" } };
        return f;
    }
    inline juce::Colour fxColour (int i)
    {
        static const juce::uint32 c[] = { 0xffff5f4d, 0xffff7d4d, 0xffff9a4d, 0xffffb84d, 0xffffd34d, 0xffffe14d, 0xfffff08a, 0xffe8ff6b, 0xffc6ff3d,
                                          0xff8dff5d, 0xff5dff7a, 0xff5dff9a, 0xff3dffc8, 0xff3de8ff,
                                          0xff3dc8ff, 0xff5d8cff, 0xff7d7dff, 0xff9a7dff, 0xffb48cff, 0xffd08bff,
                                          0xffff8ad8, 0xffff6b9e, 0xffffb3d9, 0xff7fe0ff, 0xffffd2a0 };
        return juce::Colour (c[juce::jlimit (0, NumFx - 1, i)]);
    }
    inline const juce::StringArray& fxHelp()
    {
        static const juce::StringArray h {
            TK_FR ("Le son ralentit jusqu'à l'arrêt, comme une cassette qu'on coupe."), TK_FR ("Le son repart en accélérant, comme un vieux moteur."),
            TK_FR ("Ralenti façon Matrix : tout se fige presque... puis repart d'un coup."), TK_FR ("Le DJ rembobine le disque en arrière."),
            TK_FR ("Allers-retours de platine sur le son."), TK_FR ("Le disque saute et répète le même bout, comme un CD abîmé."),
            TK_FR ("La vitesse s'étire et se compresse comme un accordéon."), TK_FR ("Le son se fige sur un instant et bourdonne."), TK_FR ("Joue à l'envers ce qui vient de passer."),
            TK_FR ("Une rafale qui accélère : 1/4, 1/8, 1/16, 1/32... le roulement avant le drop."), TK_FR ("Répète un petit morceau en boucle, b-b-b-bégaiement."),
            TK_FR ("Le son a le hoquet : des trous au hasard."), TK_FR ("Sauts au hasard dans le son, façon bug."), TK_FR ("Ta prod plante comme une vieille console de jeu."),
            TK_FR ("Tout passe dans les aigus, voix de souris (le tempo ne bouge pas)."), TK_FR ("Tout descend dans le grave, voix de monstre (le tempo ne bouge pas)."),
            TK_FR ("Son métallique de robot."), TK_FR ("Le son monte marche par marche, demi-ton par demi-ton."), TK_FR ("Chaque petit bout saute d'octave au hasard."),
            TK_FR ("Le son tombe vers le grave, comme une batterie à plat."),
            TK_FR ("Le son tourne autour de ta tête de plus en plus vite."), TK_FR ("Le son est aspiré : il ralentit, s'assombrit et disparaît."),
            TK_FR ("Un souffle à l'envers qui arrive juste avant le coup."), TK_FR ("Le son plonge sous l'eau avec des bulles."), TK_FR ("Comme quand on tourne le bouton d'une vieille radio.") };
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
    std::array<std::atomic<bool>, thumbLen> hits;   // the song's hits (transients), per 16th

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
    double vinylPh = 0; float crush[2] {}; int crushCount = 0;
    juce::Random rng;
    juce::AudioBuffer<float> dry;
    // per-zone helpers
    int lastStep = -1; double stepStartWrite = 0, stepJump = 0, stepRate = 1; bool stepMute = false; float gateG = 1.0f;
    double grainPh = 0; float ringPh = 0, panPh = 0;
    juce::Reverb ghostVerb;
    float envFast = 0, envSlow = 0; double lastHitBeat = -10;
    float pitchShift (int ch, double wAbs, double ratio);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwistProcessor)
};
