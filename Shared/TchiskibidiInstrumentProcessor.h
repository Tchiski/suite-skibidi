#pragma once

// Base processor of the Tchiskibidi instruments: internal synth, a note sequence that
// plays with the host transport (and is sent out as MIDI), auditions from the editor,
// and an optional sidechain input for listening.

#include "TchiskibidiInstrument.h"
#include "TchiskibidiDSP.h"

namespace tchk
{
class InstrumentProcessor : public juce::AudioProcessor
{
public:
    explicit InstrumentProcessor (bool withSidechain)
        : juce::AudioProcessor (withSidechain ? BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                                                                  .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
                                              : BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
        player.onNoteOn = [this] (int p, float v, bool gl, int off) { events.push_back ({ off, true, p, v, gl }); };
        player.onNoteOff = [this] (int p, int off) { events.push_back ({ off, false, p, 0.0f, false }); };
    }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()) return false;
        for (int i = 0; i < layouts.inputBuses.size(); ++i)
        {
            const auto& set = layouts.inputBuses.getReference (i);
            if (! set.isDisabled() && set != juce::AudioChannelSet::stereo() && set != juce::AudioChannelSet::mono()) return false;
        }
        return true;
    }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        sr = sampleRate;
        synth.prepare (sampleRate, juce::jmax (512, samplesPerBlock));
        events.reserve (512);
        peakDb = -100.0f;
    }
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;
        const int n = buffer.getNumSamples();
        synth.patch = currentPatch();

        // sidechain (read before the buffer is cleared for our own sound)
        if (getBusCount (true) > 0 && getBus (true, 0) != nullptr && getBus (true, 0)->isEnabled())
        {
            auto sc = getBusBuffer (buffer, true, 0);
            if (sc.getNumChannels() > 0) handleSidechain (sc);
        }

        events.clear();
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (! handleIncomingMidi (m, meta.samplePosition) && (m.isNoteOnOrOff() || m.isAllNotesOff() || m.isAllSoundOff()))
                events.push_back ({ meta.samplePosition, m.isNoteOn(), m.getNoteNumber(), m.getFloatVelocity(), false, true });
        }
        midi.clear();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);

        auto* ph = getPlayHead();
        beforeSequence (ph, n);
        player.process (ph, n, sr);
        audition.process (synth, n, sr);

        std::stable_sort (events.begin(), events.end(), [] (const Ev& a, const Ev& b) { return a.offset < b.offset; });
        const bool sound = soundEnabled();
        int pos = 0;
        for (auto& e : events)
        {
            const int off = juce::jlimit (0, n - 1, e.offset);
            if (sound && off > pos) { synth.render (buffer, pos, off - pos); pos = off; }
            if (e.on) { synth.noteOn (e.pitch, e.vel, e.glide); if (! e.external) midi.addEvent (juce::MidiMessage::noteOn (1, e.pitch, e.vel), off); onNotePlayed (e.pitch, e.vel, e.external); }
            else { synth.noteOff (e.pitch); if (! e.external) midi.addEvent (juce::MidiMessage::noteOff (1, e.pitch), off); }
        }
        if (sound && pos < n) synth.render (buffer, pos, n - pos);
        if (! sound) for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
        if (buffer.getNumChannels() > 1 && getMainBusNumOutputChannels() == 1) buffer.addFrom (0, 0, buffer, 1, 0, n, 0.5f);

        if (sanitize (buffer)) synth.panic();
        float pk = 0.0f;
        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch) pk = juce::jmax (pk, buffer.getMagnitude (ch, 0, n));
        peakDb = gainToDb (pk);
    }

    // --- sequence shared with the editor
    std::vector<SeqNote> getNotes() const { return notesFromString (state().getProperty ("notes").toString()); }
    void setNotes (const std::vector<SeqNote>& notes)
    {
        stateTree().setProperty ("notes", notesToString (notes), nullptr);
        player.setNotes (notes, totalSteps());
    }
    int totalSteps() const { return 16 * juce::jlimit (1, 16, (int) state().getProperty ("bars", 4)); }
    void preview (int pitch, float vel = 0.8f, int ms = 350) { audition.push (pitch, vel, ms); }
    Patch exportPatch() { return currentPatch(); }

    // called by the editor's timer: when the protection acted, write what happened in
    // ~/Music/Tchiskibidi/Journal.txt (to find the cause), and return true so the editor can say it
    bool flushDiagnostics()
    {
        const int t = synth.guardTrips.load();
        if (t == lastTrips) return false;
        lastTrips = t;
        const auto& d = synth.lastDiag;
        juce::String pitches; for (int p : d.pitches) if (p >= 0) pitches << p << " ";
        auto f = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Tchiskibidi").getChildFile ("Journal.txt");
        f.getParentDirectory().createDirectory();
        f.appendText (juce::Time::getCurrentTime().toString (true, true) + "  " + getName() + "  protection #" + juce::String (t)
                      + "  sr " + juce::String (d.sampleRate, 0) + " block " + juce::String (d.blockSize) + " rms " + juce::String (d.rms, 3) + " zcr " + juce::String (d.zcr, 3)
                      + " voices " + juce::String (d.voices) + " [" + pitches.trim() + "] cutoff " + juce::String (d.cutoff, 0) + " reso " + juce::String (d.reso, 2)
                      + " space " + juce::String (d.space, 2) + " chorus " + juce::String (d.chorus, 2) + " drive " + juce::String (d.drive, 2) + " unison " + juce::String (d.unison)
                      + " vowel " + juce::String (d.vowel, 1) + " mono " + juce::String ((int) d.mono) + " level " + juce::String (d.level, 2) + "\n");
        return true;
    }

    // --- state
    void getStateInformation (juce::MemoryBlock& dest) override
    {
        if (auto xml = stateTree().createXml()) copyXmlToBinary (*xml, dest);
    }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size))
            if (xml->hasTagName (stateTree().getType()))
            {
                replaceState (juce::ValueTree::fromXml (*xml));
                player.setNotes (getNotes(), totalSteps());
                ++stateVersion;
            }
    }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    bool hasEditor() const override { return true; }

    SynthEngine synth;
    SequencePlayer player;
    Audition audition;
    std::atomic<float> peakDb { -100.0f };
    std::atomic<int> stateVersion { 0 };

protected:
    int lastTrips = 0;
    struct Ev { int offset; bool on; int pitch; float vel; bool glide; bool external = false; };
    // hooks
    virtual Patch currentPatch() = 0;
    virtual bool soundEnabled() { return true; }
    virtual void handleSidechain (juce::AudioBuffer<float>&) {}
    virtual bool handleIncomingMidi (const juce::MidiMessage&, int) { return false; } // true = consumed (not played)
    virtual void beforeSequence (juce::AudioPlayHead*, int) {}
    virtual void onNotePlayed (int, float, bool) {}
    virtual juce::ValueTree& stateTree() = 0;
    const juce::ValueTree& state() const { return const_cast<InstrumentProcessor*> (this)->stateTree(); }
    virtual void replaceState (const juce::ValueTree& t) = 0;

    double sr = 44100.0;
    std::vector<Ev> events;
};

// pitch-class energy of an audio block (for listening through the sidechain)
class ChromaAnalyzer
{
public:
    ChromaAnalyzer() : fft (12), window (4096, juce::dsp::WindowingFunction<float>::hann) {}
    // adds the chroma of every full 4096-sample window to `out`; returns true when something was added
    bool push (const juce::AudioBuffer<float>& buf, double sampleRate, std::array<float, 12>& out)
    {
        bool added = false;
        const int n = buf.getNumSamples(), chans = buf.getNumChannels();
        for (int i = 0; i < n; ++i)
        {
            float s = 0; for (int c = 0; c < chans; ++c) s += buf.getSample (c, i);
            fifo[(size_t) fill++] = s / (float) juce::jmax (1, chans);
            if (fill == 4096)
            {
                std::fill (data.begin(), data.end(), 0.0f);
                std::copy (fifo.begin(), fifo.end(), data.begin());
                window.multiplyWithWindowingTable (data.data(), 4096);
                fft.performFrequencyOnlyForwardTransform (data.data());
                for (int b = 4; b < 2048; ++b)
                {
                    const double hz = b * sampleRate / 4096.0;
                    if (hz < 55.0 || hz > 2000.0) continue;
                    const int pc = ((int) std::lround (12.0 * std::log2 (hz / 440.0)) + 9 + 1200) % 12;
                    out[(size_t) pc] += data[(size_t) b] * data[(size_t) b];
                }
                // hop of half a window
                std::copy (fifo.begin() + 2048, fifo.end(), fifo.begin());
                fill = 2048; added = true;
            }
        }
        return added;
    }
private:
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, 4096> fifo {};
    std::array<float, 8192> data {};
    int fill = 0;
};
} // namespace tchk
