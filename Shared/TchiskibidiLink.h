#pragma once

// "Lien": Tchiskibidi Accords shares its progression with the other Tchiskibidi instruments
// (Synthé, Basse) without any routing in the DAW. Accords writes a small text file each time
// its chords change; the others read it and play / follow the same chords with the transport.
// Everything happens on the message thread (never on the audio thread).

#include "TchiskibidiInstrument.h"

namespace tchk::link
{
inline juce::File file()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Tchiskibidi").getChildFile ("Lien-accords.txt");
}

struct Data
{
    std::vector<SeqNote> notes;
    int steps = 64, root = -1, scale = 0;
    bool ok = false;
};

// Accords: writes "steps|root|scale|notes"
inline void publish (const std::vector<SeqNote>& notes, int steps, int root, int scale)
{
    auto f = file();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::String (steps) + "|" + juce::String (root) + "|" + juce::String (scale) + "|" + notesToString (notes));
}

inline Data read()
{
    Data d;
    const auto f = file();
    if (! f.existsAsFile()) return d;
    const auto parts = juce::StringArray::fromTokens (f.loadFileAsString(), "|", "");
    if (parts.size() < 4) return d;
    d.steps = juce::jlimit (16, 256, parts[0].getIntValue());
    d.root = juce::jlimit (0, 11, parts[1].getIntValue());
    d.scale = juce::jlimit (0, 5, parts[2].getIntValue());
    d.notes = notesFromString (parts[3]);
    d.ok = ! d.notes.empty();
    return d;
}

// Synthé / Basse: call often (timer); returns true when Accords published something new
class Follower
{
public:
    bool poll (Data& out)
    {
        const auto f = file();
        if (! f.existsAsFile()) return false;
        const auto t = f.loadFileAsString().hashCode64();   // the file is tiny
        if (t == last) return false;
        last = t;
        out = read();
        return out.ok;
    }
    void reset() { last = 0; }   // the next poll returns the current progression again
private:
    juce::int64 last = 0;
};
} // namespace tchk::link
