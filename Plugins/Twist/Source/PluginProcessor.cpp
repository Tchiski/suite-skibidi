#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace twist;

TwistProcessor::TwistProcessor()
    : juce::AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TwistState", createLayout())
{
    for (auto& t : thumb) t.store (0.0f);
    // a first example: tape stop at the end of bar 4, filter rising in bar 8, cut on the last beat
    std::vector<Zone> z { { TapeStop, 15.0, 16.0, 0.9f, 0.5f, 1.0f }, { FilterUp, 28.0, 31.0, 0.7f, 0.6f, 1.0f }, { Cut, 31.0, 32.0, 1.0f, 0.5f, 1.0f } };
    apvts.state.setProperty ("zones", toString (z), nullptr);
    pendingZones = z; hasPending = true;
}

juce::AudioProcessorValueTreeState::ParameterLayout TwistProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "active", 1 }, "Actif", true));
    return l;
}

bool TwistProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()) && layouts.getMainInputChannelSet() == out;
}

void TwistProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    histLen = (int) (sampleRate * 8.0);
    hist.setSize (2, histLen); hist.clear(); wcount = 0;
    echoBuf.setSize (2, (int) (sampleRate * 2.0)); echoBuf.clear(); echoPos = 0;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };
    lp.prepare (spec); lp.setType (juce::dsp::StateVariableTPTFilterType::lowpass); lp.setCutoffFrequency (20000.0f); lp.setResonance (0.8f);
    hp.prepare (spec); hp.setType (juce::dsp::StateVariableTPTFilterType::highpass); hp.setCutoffFrequency (20.0f); hp.setResonance (0.8f);
    dry.setSize (2, juce::jmax (1, samplesPerBlock));
    curZone = -1;
}

void TwistProcessor::setZones (const std::vector<Zone>& z)
{
    apvts.state.setProperty ("zones", toString (z), nullptr);
    pushZones();
}
void TwistProcessor::pushZones()
{
    const juce::SpinLock::ScopedLockType sl (zoneLock);
    pendingZones = getZones(); hasPending = true;
}

void TwistProcessor::sprinkle (int density, int bars)
{
    auto& r = rng;
    const int n = density == 0 ? 3 : density == 1 ? 6 : 10;
    std::vector<double> spots;
    for (int b = 4; b <= bars; b += 4) { spots.push_back (b * 4.0 - 0.5); spots.push_back (b * 4.0 - 1.0); }
    for (int b = 2; b <= bars; b += 4) spots.push_back (b * 4.0 - 0.5);
    std::vector<Zone> z;
    for (int k = 0; k < n && ! spots.empty(); ++k)
    {
        const int i = r.nextInt ((int) spots.size());
        const double at = spots[(size_t) i]; spots.erase (spots.begin() + i);
        const bool phraseEnd = std::fmod (at + 0.5, 16.0) < 0.01;
        static const int shortFx[] = { TapeStop, Stutter, Cut, Reverse, TapeStop, Stutter };
        static const int longFx[] = { FilterDown, FilterUp, Echo, Pump, Vinyl, Slow, LoFi, TapeStart };
        Zone zo;
        if (phraseEnd || r.nextFloat() < 0.5f) { zo.fx = shortFx[r.nextInt (6)]; zo.start = at; zo.end = at + 0.5; if (zo.fx == TapeStop && r.nextBool()) zo.start = at - 0.5; }
        else { zo.fx = longFx[r.nextInt (8)]; const double len = (double) (1 << r.nextInt (3)); zo.end = at + 0.5; zo.start = juce::jmax (0.0, zo.end - len); }
        zo.amount = 0.6f + 0.4f * r.nextFloat(); zo.curve = 0.5f; zo.mix = 1.0f;
        bool overlap = false;
        for (auto& o : z) if (zo.start < o.end && zo.end > o.start) overlap = true;
        if (! overlap) z.push_back (zo);
    }
    std::sort (z.begin(), z.end(), [] (const Zone& a, const Zone& b) { return a.start < b.start; });
    setZones (z);
    ++stateVersion;
}

float TwistProcessor::readAt (int ch, double pos) const
{
    double p = std::fmod (pos, (double) histLen); if (p < 0) p += histLen;
    const int i0 = juce::jlimit (0, histLen - 1, (int) p), i1 = (i0 + 1) % histLen;
    const float fr = (float) (p - std::floor (p));
    const auto* d = hist.getReadPointer (juce::jmin (ch, hist.getNumChannels() - 1));
    return d[i0] + (d[i1] - d[i0]) * fr;
}

void TwistProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), chans = juce::jmin (2, buffer.getNumChannels());
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    {
        const juce::SpinLock::ScopedTryLockType sl (zoneLock);
        if (sl.isLocked() && hasPending) { zonesRT = pendingZones; hasPending = false; curZone = -1; }
    }
    bool isPlaying = false; double ppq = 0, bpm = 120;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            isPlaying = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppq = *p; else isPlaying = false;
            if (auto b = pos->getBpm()) bpm = *b;
        }
    const bool active = apvts.getRawParameterValue ("active")->load() > 0.5f;
    playing = isPlaying;
    playBeat = isPlaying ? ppq : -1.0;
    const double spb = sr * 60.0 / juce::jmax (20.0, bpm);   // samples per beat
    const float fadeStep = 1.0f / (float) (0.008 * sr);
    float pk = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float in[2] = { buffer.getSample (0, i), buffer.getSample (chans > 1 ? 1 : 0, i) };
        const int wIdx = (int) (wcount % histLen);
        for (int c = 0; c < 2; ++c) hist.setSample (c, wIdx, in[c]);
        const double wAbs = (double) wcount;
        const double beat = ppq + (double) i / spb;

        // which zone are we in?
        int z = -1;
        if (isPlaying && active)
            for (int k = 0; k < (int) zonesRT.size(); ++k)
                if (beat >= zonesRT[(size_t) k].start && beat < zonesRT[(size_t) k].end) { z = k; break; }
        if (isPlaying && beat >= 0 && beat * 4.0 < thumbLen)
        {
            auto& t = thumb[(size_t) (beat * 4.0)];
            const float a = juce::jmax (std::abs (in[0]), std::abs (in[1]));
            if (a > t.load()) t.store (a);
        }
        if (z >= 0 && z != curZone)
        {
            curZone = z; zoneStartWrite = wAbs; readPos = wAbs; xfade = 0.0f;
            if (zonesRT[(size_t) z].fx == TapeStart) readPos = wAbs - 1.0;
        }
        const bool inZone = z >= 0;
        xfade = juce::jlimit (0.0f, 1.0f, xfade + (inZone ? fadeStep : -fadeStep));
        float out[2] = { in[0], in[1] };
        float echoSend = 0.0f;
        if (curZone >= 0 && curZone < (int) zonesRT.size() && xfade > 0.0f)
        {
            const auto& zo = zonesRT[(size_t) curZone];
            const double len = juce::jmax (1.0, (zo.end - zo.start) * spb);
            const double el = wAbs - zoneStartWrite;
            const float u = (float) juce::jlimit (0.0, 1.0, el / len);
            const float us = std::pow (u, std::exp2 ((zo.curve - 0.5f) * 3.0f));
            const float amt = zo.amount;
            float wet[2] = { in[0], in[1] };
            switch (zo.fx)
            {
                case TapeStop: { const double rate = juce::jmax (0.0, 1.0 - us * amt); readPos += rate; const float g = (float) juce::jmin (1.0, rate * 6.0 + (1.0 - amt));
                                 for (int c = 0; c < 2; ++c) wet[c] = readAt (c, juce::jmin (readPos, wAbs)) * g; } break;
                case TapeStart: { const double rate = 1.0 - (1.0 - us) * amt; readPos += rate; const float g = (float) juce::jmin (1.0, rate * 6.0 + (1.0 - amt));
                                  for (int c = 0; c < 2; ++c) wet[c] = readAt (c, juce::jmin (readPos, wAbs)) * g; } break;
                case Slow: { readPos += 1.0 - 0.5 * amt; for (int c = 0; c < 2; ++c) wet[c] = readAt (c, juce::jmin (readPos, wAbs)); } break;
                case Stutter:
                {
                    const double slice = spb / (amt > 0.66f ? 4.0 : amt > 0.33f ? 2.0 : 1.0);
                    const double ph = std::fmod (el, slice);
                    const float win = (float) juce::jmin (1.0, juce::jmin (ph, slice - ph) / (0.003 * sr));
                    for (int c = 0; c < 2; ++c) wet[c] = readAt (c, zoneStartWrite - slice + ph) * win;
                } break;
                case Reverse: { const double p = zoneStartWrite - el * (0.5 + 0.5 * amt); for (int c = 0; c < 2; ++c) wet[c] = readAt (c, p); } break;
                case FilterDown:
                    if ((i & 31) == 0) lp.setCutoffFrequency (juce::jlimit (40.0f, 20000.0f, 20000.0f * std::pow (0.015f, us * amt)));
                    for (int c = 0; c < 2; ++c) wet[c] = lp.processSample (c, in[c]);
                    break;
                case FilterUp:
                    if ((i & 31) == 0) hp.setCutoffFrequency (juce::jlimit (20.0f, 8000.0f, 20.0f * std::pow (200.0f, us * amt)));
                    for (int c = 0; c < 2; ++c) wet[c] = hp.processSample (c, in[c]);
                    break;
                case Cut: for (int c = 0; c < 2; ++c) wet[c] = in[c] * (1.0f - amt); break;
                case Echo: echoSend = amt; for (int c = 0; c < 2; ++c) wet[c] = in[c] * (1.0f - 0.5f * amt); break;
                case Vinyl:
                {
                    vinylPh += juce::MathConstants<double>::twoPi * 0.55 / sr;
                    const double d = 0.004 * sr + 0.0025 * sr * amt * (0.5 + 0.5 * std::sin (vinylPh));
                    if ((i & 31) == 0) lp.setCutoffFrequency (3500.0f);
                    const float crackle = rng.nextFloat() < 0.0004f * amt ? (rng.nextFloat() - 0.5f) * 0.5f : 0.0f;
                    for (int c = 0; c < 2; ++c) wet[c] = lp.processSample (c, readAt (c, wAbs - d)) + crackle;
                } break;
                case Pump: { const double ph = std::fmod (beat, 1.0); const float g = 1.0f - amt * 0.85f * (float) std::pow (1.0 - ph, 3.0); for (int c = 0; c < 2; ++c) wet[c] = in[c] * g; } break;
                case LoFi:
                {
                    const int hold = 1 + (int) (amt * 10.0f);
                    if (crushCount++ % hold == 0) for (int c = 0; c < 2; ++c) crush[c] = in[c];
                    const float levels = std::exp2 (16.0f - 12.0f * amt);
                    for (int c = 0; c < 2; ++c) wet[c] = std::round (crush[c] * levels) / levels;
                } break;
                default: break;
            }
            const float w = xfade * zo.mix;
            for (int c = 0; c < 2; ++c) out[c] = in[c] * (1.0f - w) + wet[c] * w;
        }
        else if (xfade <= 0.0f) curZone = inZone ? curZone : -1;

        // echo throw: the send only opens in an Écho zone, the tail keeps ringing after it
        const int eLen = echoBuf.getNumSamples();
        const int delay = juce::jlimit (1, eLen - 1, (int) (spb * 0.75));
        echoFb += ((echoSend > 0 ? 0.55f * echoSend : 0.42f) - echoFb) * 0.0005f;
        for (int c = 0; c < 2; ++c)
        {
            const float e = echoBuf.getSample (c, (echoPos - delay + eLen) % eLen);
            echoBuf.setSample (c, echoPos, out[1 - c] * echoSend * xfade + e * echoFb);  // ping-pong
            out[c] += e * 0.8f;
        }
        echoPos = (echoPos + 1) % eLen;

        for (int c = 0; c < chans; ++c) buffer.setSample (c, i, out[c]);
        pk = juce::jmax (pk, std::abs (out[0]), std::abs (out[1]));
        ++wcount;
    }
    activeZone = curZone;
    outputPeakDb = tchk::gainToDb (pk);
}

void TwistProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.state.createXml()) copyXmlToBinary (*xml, dest);
}
void TwistProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) { apvts.replaceState (juce::ValueTree::fromXml (*xml)); pushZones(); ++stateVersion; }
}

juce::AudioProcessorEditor* TwistProcessor::createEditor() { return new TwistEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TwistProcessor(); }
