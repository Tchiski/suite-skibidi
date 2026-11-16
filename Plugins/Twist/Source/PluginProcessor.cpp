#include "PluginProcessor.h"
#include "TchiskibidiDSP.h"
#include "PluginEditor.h"

using namespace twist;

TwistProcessor::TwistProcessor()
    : juce::AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TwistState", createLayout())
{
    for (auto& t : thumb) t.store (0.0f);
    for (auto& h : hits) h.store (false);
    // a first example: a roll before bar 3, tape stop at the end of bar 4, a spin in bar 6, a black hole at the end
    std::vector<Zone> z { { Roll, 6.0, 8.0, 0.9f, 0.5f, 1.0f }, { TapeStop, 15.0, 16.0, 0.9f, 0.5f, 1.0f }, { Spin, 20.0, 24.0, 0.8f, 0.5f, 1.0f }, { BlackHole, 29.0, 32.0, 0.9f, 0.5f, 1.0f } };
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
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };
    lp.prepare (spec); lp.setType (juce::dsp::StateVariableTPTFilterType::lowpass); lp.setCutoffFrequency (20000.0f); lp.setResonance (0.8f);
    hp.prepare (spec); hp.setType (juce::dsp::StateVariableTPTFilterType::highpass); hp.setCutoffFrequency (20.0f); hp.setResonance (0.8f);
    dry.setSize (2, juce::jmax (1, samplesPerBlock));
    ghostVerb.setSampleRate (sampleRate);
    juce::Reverb::Parameters rp; rp.roomSize = 0.92f; rp.damping = 0.3f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f; ghostVerb.setParameters (rp);
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
        static const int shortFx[] = { TapeStop, Stutter, Roll, Reverse, Rewind, Scratch, Glitch, Freeze, Skip, Ghost, Hiccup };
        static const int longFx[] = { BulletTime, Accordion, Helium, Demon, Robot, Elevator, Arp, PitchDrop, Spin, BlackHole, Aquarium, Radio, Bug8, TapeStart };
        Zone zo;
        if (phraseEnd || r.nextFloat() < 0.5f) { zo.fx = shortFx[r.nextInt (11)]; zo.start = at; zo.end = at + 0.5; if ((zo.fx == TapeStop || zo.fx == Roll || zo.fx == Ghost) && r.nextBool()) zo.start = at - 1.5; }
        else { zo.fx = longFx[r.nextInt (14)]; const double len = (double) (1 << r.nextInt (3)); zo.end = at + 0.5; zo.start = juce::jmax (0.0, zo.end - len); }
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

// two crossfaded taps moving through the history: pitch changes, tempo stays
float TwistProcessor::pitchShift (int ch, double wAbs, double ratio)
{
    juce::ignoreUnused (ratio);
    const double W = 0.06 * sr;
    const double p1 = grainPh, p2 = std::fmod (grainPh + 0.5, 1.0);
    const float g1 = (float) std::sin (juce::MathConstants<double>::pi * p1), g2 = (float) std::sin (juce::MathConstants<double>::pi * p2);
    return readAt (ch, wAbs - 1.0 - p1 * W) * g1 + readAt (ch, wAbs - 1.0 - p2 * W) * g2;
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
            // hits: the fast envelope jumps above the slow one
            envFast += (a - envFast) * (a > envFast ? 0.05f : 0.002f);
            envSlow += (a - envSlow) * 0.0002f;
            if (envFast > envSlow * 1.8f + 0.02f && beat - lastHitBeat > 0.2)
            {
                lastHitBeat = beat;
                hits[(size_t) juce::jlimit (0, thumbLen - 1, (int) std::round (beat * 4.0))].store (true);
            }
        }
        if (z >= 0 && z != curZone)
        {
            curZone = z; zoneStartWrite = wAbs; readPos = wAbs; xfade = 0.0f; lastStep = -1; gateG = 1.0f; grainPh = 0; ghostVerb.reset();
            if (zonesRT[(size_t) z].fx == TapeStart || zonesRT[(size_t) z].fx == Rewind) readPos = wAbs - 1.0;
        }
        const bool inZone = z >= 0;
        xfade = juce::jlimit (0.0f, 1.0f, xfade + (inZone ? fadeStep : -fadeStep));
        float out[2] = { in[0], in[1] };
        if (curZone >= 0 && curZone < (int) zonesRT.size() && xfade > 0.0f)
        {
            const auto& zo = zonesRT[(size_t) curZone];
            const double len = juce::jmax (1.0, (zo.end - zo.start) * spb);
            const double el = wAbs - zoneStartWrite;
            const float u = (float) juce::jlimit (0.0, 1.0, el / len);
            const float us = std::pow (u, std::exp2 ((zo.curve - 0.5f) * 3.0f));
            const float amt = zo.amount;
            float wet[2] = { in[0], in[1] };
            const double step16 = spb / 4.0;
            const int step = (int) std::floor (el / step16);
            if (step != lastStep)   // a new 16th inside the zone: new random choices
            {
                lastStep = step; stepStartWrite = wAbs;
                stepJump = (double) rng.nextInt (8) * step16 + step16; stepRate = rng.nextFloat() < 0.3f ? 2.0 : 1.0;
                stepMute = rng.nextFloat() < 0.45f * amt;
                static const double arp[] = { 0.5, 1.0, 2.0, 1.5 };
                if (zo.fx == Arp) stepRate = arp[rng.nextInt (4)];
            }
            const double inStep = wAbs - stepStartWrite;
            auto vari = [&] (double rate) { readPos += rate; readPos = juce::jmin (readPos, wAbs); for (int c = 0; c < 2; ++c) wet[c] = readAt (c, readPos); };
            auto pshift = [&] (double ratio)
            {
                for (int c = 0; c < 2; ++c) wet[c] = pitchShift (c, wAbs, ratio);
                const double W = 0.06 * sr; grainPh += (1.0 - ratio) / W; grainPh -= std::floor (grainPh);
            };
            switch (zo.fx)
            {
                case TapeStop: { const double rate = juce::jmax (0.0, 1.0 - us * amt); vari (rate); const float g = (float) juce::jmin (1.0, rate * 6.0 + (1.0 - amt)); wet[0] *= g; wet[1] *= g; } break;
                case TapeStart: { const double rate = 1.0 - (1.0 - us) * amt; vari (rate); const float g = (float) juce::jmin (1.0, rate * 6.0 + (1.0 - amt)); wet[0] *= g; wet[1] *= g; } break;
                case BulletTime: { const double low = 1.0 - 0.8 * amt; const double rate = u < 0.45f ? 1.0 + (low - 1.0) * (u / 0.45) : u < 0.8f ? low : low + (1.0 - low) * ((u - 0.8) / 0.2); vari (rate); } break;
                case Rewind: { readPos -= 0.5 + 2.5 * amt * u; for (int c = 0; c < 2; ++c) wet[c] = readAt (c, readPos); } break;
                case Scratch:
                {
                    const double per = spb / 4.0, a2 = (spb / 10.0) * amt;
                    const double pos = wAbs - a2 * (1.0 - std::cos (juce::MathConstants<double>::twoPi * el / per));
                    for (int c = 0; c < 2; ++c) wet[c] = readAt (c, pos);
                } break;
                case Skip:
                {
                    const double loop = spb / 2.0, ph = std::fmod (el, loop);
                    const float win = (float) juce::jmin (1.0, juce::jmin (ph, loop - ph) / (0.002 * sr));
                    const float tick = ph < 0.0015 * sr ? (rng.nextFloat() - 0.5f) * 0.4f : 0.0f;
                    for (int c = 0; c < 2; ++c) wet[c] = readAt (c, zoneStartWrite + ph) * win + tick;
                } break;
                case Accordion: { const double cyc = juce::jmax (1.0, zo.end - zo.start); vari (1.0 + 0.45 * amt * std::sin (juce::MathConstants<double>::twoPi * u * cyc)); } break;
                case Freeze:
                {
                    const double g = 0.05 * sr;
                    for (int c = 0; c < 2; ++c)
                    {
                        const double p1 = std::fmod (el, g), p2 = std::fmod (el + g * 0.5, g);
                        const float w1 = (float) std::sin (juce::MathConstants<double>::pi * p1 / g), w2 = (float) std::sin (juce::MathConstants<double>::pi * p2 / g);
                        wet[c] = (readAt (c, zoneStartWrite - g + p1) * w1 + readAt (c, zoneStartWrite - g + p2) * w2) * 0.8f;
                    }
                } break;
                case Reverse: { for (int c = 0; c < 2; ++c) wet[c] = readAt (c, zoneStartWrite - el); } break;
                case Roll:
                {
                    const double div = u < 0.25f ? 1.0 : u < 0.5f ? 2.0 : u < 0.75f ? 4.0 : 8.0;
                    const double slice = spb / div, ph = std::fmod (el, slice);
                    const float win = (float) juce::jmin (1.0, juce::jmin (ph, slice - ph) / (0.002 * sr));
                    const float g = 0.6f + 0.4f * u;
                    for (int c = 0; c < 2; ++c) wet[c] = readAt (c, zoneStartWrite + ph) * win * g;
                } break;
                case Stutter:
                {
                    const double slice = spb / (amt > 0.66f ? 4.0 : amt > 0.33f ? 2.0 : 1.0);
                    const double ph = std::fmod (el, slice);
                    const float win = (float) juce::jmin (1.0, juce::jmin (ph, slice - ph) / (0.003 * sr));
                    for (int c = 0; c < 2; ++c) wet[c] = readAt (c, zoneStartWrite - slice + ph) * win;
                } break;
                case Hiccup: { gateG += ((stepMute ? 0.0f : 1.0f) - gateG) * 0.01f; for (int c = 0; c < 2; ++c) wet[c] = in[c] * gateG; } break;
                case Glitch:
                {
                    const float win = (float) juce::jmin (1.0, juce::jmin (inStep, step16 - inStep) / (0.002 * sr));
                    for (int c = 0; c < 2; ++c) wet[c] = readAt (c, stepStartWrite - stepJump + inStep * stepRate) * win;
                } break;
                case Bug8:
                {
                    const double tiny = spb / 8.0, ph = std::fmod (inStep, tiny);
                    const double pos = stepMute ? stepStartWrite - tiny + ph : wAbs;
                    if (crushCount++ % 6 == 0) for (int c = 0; c < 2; ++c) crush[c] = readAt (c, pos);
                    for (int c = 0; c < 2; ++c) wet[c] = std::round (crush[c] * 24.0f) / 24.0f;
                } break;
                case Helium: pshift (1.0 + amt); break;
                case Demon: pshift (1.0 - 0.5 * amt); break;
                case Robot:
                {
                    ringPh += juce::MathConstants<float>::twoPi * 70.0f / (float) sr; if (ringPh > juce::MathConstants<float>::twoPi) ringPh -= juce::MathConstants<float>::twoPi;
                    const float m = std::sin (ringPh);
                    for (int c = 0; c < 2; ++c) wet[c] = in[c] * (1.0f - amt) + in[c] * m * amt * 1.1f;
                } break;
                case Elevator: pshift (std::exp2 (std::floor (u * 12.0f * amt) / 12.0)); break;
                case Arp: pshift (stepRate); break;
                case PitchDrop: pshift (1.0 - 0.45 * amt * us); break;
                case Spin:
                {
                    panPh += juce::MathConstants<float>::twoPi * (1.0f + 7.0f * u * amt) / (float) sr; if (panPh > juce::MathConstants<float>::twoPi) panPh -= juce::MathConstants<float>::twoPi;
                    const float pan = std::sin (panPh), mono = (in[0] + in[1]) * 0.5f;
                    wet[0] = mono * std::sqrt (0.5f * (1.0f - pan)) * 1.3f; wet[1] = mono * std::sqrt (0.5f * (1.0f + pan)) * 1.3f;
                } break;
                case BlackHole:
                {
                    vari (1.0 - 0.75 * amt * (double) us);
                    if ((i & 31) == 0) lp.setCutoffFrequency (juce::jlimit (60.0f, 20000.0f, 20000.0f * std::pow (0.01f, us * amt)));
                    const float g = 1.0f - 0.95f * us * amt;
                    for (int c = 0; c < 2; ++c) wet[c] = lp.processSample (c, wet[c]) * g;
                } break;
                case Ghost:
                {
                    float l = readAt (0, zoneStartWrite - el), r = readAt (1, zoneStartWrite - el);
                    ghostVerb.processStereo (&l, &r, 1);
                    const float g = (0.15f + 0.6f * amt * u) * 0.35f;
                    wet[0] = in[0] * 0.25f + l * g; wet[1] = in[1] * 0.25f + r * g;
                } break;
                case Aquarium:
                {
                    vinylPh += juce::MathConstants<double>::twoPi * 3.0 / sr;
                    const double d = 0.003 * sr + 0.002 * sr * std::sin (vinylPh);
                    if ((i & 31) == 0) lp.setCutoffFrequency (juce::jlimit (300.0f, 20000.0f, 700.0f + 3000.0f * (1.0f - amt)));
                    for (int c = 0; c < 2; ++c) wet[c] = lp.processSample (c, 0.6f * readAt (c, wAbs - d) + 0.5f * in[c]);
                } break;
                case Radio:
                {
                    if ((i & 31) == 0)
                    {
                        hp.setCutoffFrequency (300.0f + 2000.0f * std::abs (std::sin (u * 7.0f)));
                        lp.setCutoffFrequency (900.0f + 3000.0f * std::abs (std::sin (u * 5.0f)));
                    }
                    const float hiss = (rng.nextFloat() - 0.5f) * 0.04f * amt;
                    for (int c = 0; c < 2; ++c) wet[c] = std::tanh (3.0f * lp.processSample (c, hp.processSample (c, in[c]))) * 0.6f + hiss;
                } break;
                default: break;
            }
            const float w = xfade * zo.mix;
            for (int c = 0; c < 2; ++c) out[c] = in[c] * (1.0f - w) + wet[c] * w;
        }
        else if (xfade <= 0.0f) curZone = inZone ? curZone : -1;

        for (int c = 0; c < chans; ++c) buffer.setSample (c, i, out[c]);
        pk = juce::jmax (pk, std::abs (out[0]), std::abs (out[1]));
        ++wcount;
    }
    activeZone = curZone;
    outputPeakDb = tchk::gainToDb (pk);
    if (tchk::sanitize (buffer)) { hist.clear(); lp.reset(); hp.reset(); ghostVerb.reset(); curZone = -1; }
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
