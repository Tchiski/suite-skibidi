#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tchk;

juce::AudioProcessorValueTreeState::ParameterLayout MixProcessor::createParameterLayout()
{
    ParamList p;
    p.add ("input", "Input", -18.0f, 18.0f, 0.0f, Unit::dB);
    p.add ("lowcut", "Low Cut", 20.0f, 500.0f, 20.0f, Unit::hz, 0.4f);

    p.addBool ("eq_on", "EQ On", true);
    p.add ("low_gain", "Low Gain", -12.0f, 12.0f, 0.0f, Unit::dB);
    p.add ("low_freq", "Low Freq", 30.0f, 400.0f, 100.0f, Unit::hz, 0.5f);
    p.add ("mid_gain", "Mid Gain", -12.0f, 12.0f, 0.0f, Unit::dB);
    p.add ("mid_freq", "Mid Freq", 150.0f, 5000.0f, 600.0f, Unit::hz, 0.4f);
    p.add ("mid_q", "Mid Q", 0.3f, 6.0f, 1.0f, Unit::plain, 0.5f);
    p.add ("hmid_gain", "High-Mid Gain", -12.0f, 12.0f, 0.0f, Unit::dB);
    p.add ("hmid_freq", "High-Mid Freq", 1000.0f, 12000.0f, 3500.0f, Unit::hz, 0.4f);
    p.add ("high_gain", "High Gain", -12.0f, 12.0f, 0.0f, Unit::dB);
    p.add ("high_freq", "High Freq", 3000.0f, 18000.0f, 10000.0f, Unit::hz, 0.5f);

    p.addBool ("comp_on", "Comp On", true);
    p.add ("threshold", "Threshold", -48.0f, 0.0f, -18.0f, Unit::dB);
    p.add ("ratio", "Ratio", 1.0f, 12.0f, 3.0f, Unit::ratio, 0.5f);
    p.add ("attack", "Attack", 0.1f, 100.0f, 10.0f, Unit::ms, 0.4f);
    p.add ("release", "Release", 10.0f, 1000.0f, 120.0f, Unit::ms, 0.4f);
    p.add ("comp_mix", "Comp Mix", 0.0f, 1.0f, 1.0f, Unit::percent);

    p.addBool ("trans_on", "Transient On", true);
    p.add ("punch", "Punch", -1.0f, 1.0f, 0.0f, Unit::percent);
    p.add ("sustain", "Sustain", -1.0f, 1.0f, 0.0f, Unit::percent);

    p.addBool ("exc_on", "Exciter On", true);
    p.add ("exc_drive", "Exciter Drive", 0.0f, 1.0f, 0.3f, Unit::percent);
    p.add ("exc_freq", "Exciter Tone", 1000.0f, 10000.0f, 3000.0f, Unit::hz, 0.5f);
    p.add ("exc_amount", "Exciter Amount", 0.0f, 1.0f, 0.0f, Unit::percent);

    p.addBool ("clip_on", "Clip On", true);
    p.add ("clip", "Clip Threshold", -18.0f, 0.0f, 0.0f, Unit::dB);
    p.add ("width", "Width", 0.0f, 2.0f, 1.0f, Unit::percent);
    p.add ("output", "Output", -18.0f, 18.0f, 0.0f, Unit::dB);
    return p.build();
}

juce::StringArray MixProcessor::getPresetNames()
{
    return { "Neutre", "Kick punchy", "808 / Basse", "Snare / Clap", "Hi-hats", "Guitare / Piano", "Pad large", "Bus batterie" };
}

void MixProcessor::applyPreset (int index)
{
    struct P { float lowcut, lg, lf, mg, mf, mq, hmg, hmf, hg, hf, th, ra, at, re, punch, sus, exd, exa, clip, width; };
    static const P presets[] = {
        { 20, 0, 100, 0, 600, 1, 0, 3500, 0, 10000, -18, 3, 10, 120, 0, 0, 0.3f, 0, 0, 1 },
        { 30, 3, 60, -4, 350, 1.2f, 2, 4000, 0, 10000, -20, 4, 15, 80, 0.5f, -0.2f, 0.3f, 0.1f, -2, 1 },
        { 25, 2, 55, -3, 250, 1, 0, 3500, -2, 10000, -18, 3, 20, 150, 0.1f, 0.1f, 0.5f, 0.25f, -3, 0.8f },
        { 100, 0, 100, -3, 450, 1.5f, 3, 5000, 2, 12000, -22, 4, 5, 100, 0.4f, -0.1f, 0.3f, 0.15f, -2, 1 },
        { 300, 0, 100, -2, 1000, 1, 1, 6000, 3, 13000, -20, 3, 3, 60, 0.3f, -0.3f, 0.3f, 0.1f, 0, 1.2f },
        { 80, -1, 150, -2, 400, 1, 2, 3000, 2, 11000, -20, 2.5f, 20, 150, 0, 0.1f, 0.3f, 0.15f, 0, 1.1f },
        { 120, 0, 100, -2, 500, 0.8f, 0, 3500, 3, 12000, -24, 2, 40, 300, -0.3f, 0.3f, 0.3f, 0.1f, 0, 1.5f },
        { 30, 2, 70, -2, 400, 1, 1, 4000, 2, 12000, -16, 4, 30, 120, 0.3f, 0, 0.4f, 0.1f, -2, 1 },
    };
    const auto& v = presets[juce::jlimit (0, (int) std::size (presets) - 1, index)];
    auto s = [this] (const char* id, float x) { tchk::setParam (apvts, id, x); };
    s ("lowcut", v.lowcut); s ("low_gain", v.lg); s ("low_freq", v.lf); s ("mid_gain", v.mg); s ("mid_freq", v.mf); s ("mid_q", v.mq);
    s ("hmid_gain", v.hmg); s ("hmid_freq", v.hmf); s ("high_gain", v.hg); s ("high_freq", v.hf);
    s ("threshold", v.th); s ("ratio", v.ra); s ("attack", v.at); s ("release", v.re); s ("comp_mix", 1.0f);
    s ("punch", v.punch); s ("sustain", v.sus); s ("exc_drive", v.exd); s ("exc_amount", v.exa); s ("clip", v.clip); s ("width", v.width);
    for (auto* id : { "eq_on", "comp_on", "trans_on", "exc_on", "clip_on" }) s (id, 1.0f);
}

std::array<BiquadCoeffs, 5> MixProcessor::makeEqCoeffs (const juce::AudioProcessorValueTreeState& s, double fs)
{
    auto v = [&s] (const char* id) { return (double) s.getRawParameterValue (id)->load(); };
    return { BiquadCoeffs::make (FilterType::highPass, fs, v ("lowcut"), 0.7071),
             BiquadCoeffs::make (FilterType::lowShelf, fs, v ("low_freq"), 0.7, v ("low_gain")),
             BiquadCoeffs::make (FilterType::peak, fs, v ("mid_freq"), v ("mid_q"), v ("mid_gain")),
             BiquadCoeffs::make (FilterType::peak, fs, v ("hmid_freq"), 1.0, v ("hmid_gain")),
             BiquadCoeffs::make (FilterType::highShelf, fs, v ("high_freq"), 0.7, v ("high_gain")) };
}

MixProcessor::MixProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

bool MixProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
        && layouts.getMainInputChannelSet() == out;
}

void MixProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlockSize = juce::jmax (1, samplesPerBlock);
    for (auto& f : eq) f.reset();
    lowCut2.reset();
    lastEqParams.fill (-12345.0f);
    comp.reset();
    fastAttEnv = slowAttEnv = fastRelEnv = slowRelEnv = 0.0f;
    kFastAtt = onePoleCoef (sr, 0.5); kSlowAtt = onePoleCoef (sr, 25.0); kRel = onePoleCoef (sr, 120.0);
    kRelAtt = onePoleCoef (sr, 0.5); kFastRel = onePoleCoef (sr, 25.0); kSlowRel = onePoleCoef (sr, 350.0);
    exciterHp.reset();
    lastExcFreq = -1.0f;

    const int numCh = juce::jlimit (1, 2, getTotalNumOutputChannels());
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numCh, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing ((size_t) maxBlockSize);
    oversampling->reset();
    setLatencySamples ((int) std::round (oversampling->getLatencyInSamples()));

    auto init = [] (juce::SmoothedValue<float>& s, double rate, float v) { s.reset (rate, 0.05); s.setCurrentAndTargetValue (v); };
    init (inGain, sr, dbToGain (param ("input")));
    init (outGain, sr, dbToGain (param ("output")));
    init (widthSm, sr, param ("width"));
    init (compMix, sr, param ("comp_mix"));
    init (excAmount, sr * 2.0, 0.0f);
    init (clipCeil, sr * 2.0, dbToGain (param ("clip")));
}

void MixProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n > maxBlockSize)
    {
        for (int start = 0; start < n; start += maxBlockSize)
        {
            juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, juce::jmin (maxBlockSize, n - start));
            processBlock (sub, midi);
        }
        return;
    }
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    const int numCh = juce::jmin (2, getTotalNumOutputChannels(), buffer.getNumChannels());
    if (numCh <= 0 || n <= 0 || oversampling == nullptr)
        return;

    float inPk = 0.0f;
    for (int ch = 0; ch < numCh; ++ch) inPk = juce::jmax (inPk, buffer.getMagnitude (ch, 0, n));
    inputPeakDb = gainToDb (inPk, -100.0f);

    // ---- EQ coefficients (only recomputed when a knob moved) ----
    const bool eqOn = param ("eq_on") > 0.5f;
    {
        const char* ids[] = { "lowcut", "low_gain", "low_freq", "mid_gain", "mid_freq", "mid_q", "hmid_gain", "hmid_freq", "high_gain", "high_freq", "eq_on" };
        bool changed = false;
        for (size_t k = 0; k < lastEqParams.size(); ++k)
        {
            const float v = param (ids[k]);
            if (v != lastEqParams[k]) { lastEqParams[k] = v; changed = true; }
        }
        if (changed)
        {
            const auto c = makeEqCoeffs (apvts, sr);
            for (size_t k = 0; k < eq.size(); ++k) eq[k].set (c[k]);
            lowCut2.set (c[0]);
        }
    }
    const bool lowCutOn = param ("lowcut") > 21.0f;

    const bool compOn = param ("comp_on") > 0.5f;
    comp.thresholdDb = param ("threshold");
    comp.ratio = param ("ratio");
    comp.kneeDb = 6.0f;
    comp.setTimes (sr, param ("attack"), param ("release"));
    const float makeupDb = -comp.thresholdDb * (1.0f - 1.0f / comp.ratio) * 0.5f;
    compMix.setTargetValue (compOn ? param ("comp_mix") : 0.0f);

    const bool transOn = param ("trans_on") > 0.5f;
    const float punch = param ("punch"), sustain = param ("sustain");

    inGain.setTargetValue (dbToGain (param ("input")));
    outGain.setTargetValue (dbToGain (param ("output")));
    widthSm.setTargetValue (param ("width"));

    float minGr = 0.0f, transMax = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float ig = inGain.getNextValue();
        float x[2] = { buffer.getSample (0, i) * ig, numCh > 1 ? buffer.getSample (1, i) * ig : 0.0f };

        for (int ch = 0; ch < numCh; ++ch)
        {
            if (lowCutOn) x[ch] = lowCut2.process (ch, eq[0].process (ch, x[ch]));
            if (eqOn)
                for (size_t k = 1; k < eq.size(); ++k)
                    x[ch] = eq[k].process (ch, x[ch]);
        }

        const float level = juce::jmax (std::abs (x[0]), std::abs (x[1]));

        // Compressor (parallel mix + auto makeup)
        const float cm = compMix.getNextValue();
        if (cm > 0.0001f)
        {
            const float gr = comp.process (gainToDb (level));
            minGr = juce::jmin (minGr, gr);
            const float g = dbToGain (gr + makeupDb);
            for (int ch = 0; ch < numCh; ++ch)
                x[ch] = x[ch] * (1.0f - cm) + x[ch] * g * cm;
        }

        // Transient shaper: difference between fast and slow envelopes
        if (transOn && (punch != 0.0f || sustain != 0.0f))
        {
            fastAttEnv = level > fastAttEnv ? level + kFastAtt * (fastAttEnv - level) : level + kRel * (fastAttEnv - level);
            slowAttEnv = level > slowAttEnv ? level + kSlowAtt * (slowAttEnv - level) : level + kRel * (slowAttEnv - level);
            fastRelEnv = level > fastRelEnv ? level + kRelAtt * (fastRelEnv - level) : level + kFastRel * (fastRelEnv - level);
            slowRelEnv = level > slowRelEnv ? level + kRelAtt * (slowRelEnv - level) : level + kSlowRel * (slowRelEnv - level);
            const float attackDiff = gainToDb (fastAttEnv, -90.0f) - gainToDb (slowAttEnv, -90.0f);
            const float sustainDiff = gainToDb (slowRelEnv, -90.0f) - gainToDb (fastRelEnv, -90.0f);
            const float gDb = juce::jlimit (-12.0f, 12.0f, punch * attackDiff * 1.2f + sustain * sustainDiff * 0.8f);
            transMax = std::abs (gDb) > std::abs (transMax) ? gDb : transMax;
            const float g = dbToGain (gDb);
            for (int ch = 0; ch < numCh; ++ch) x[ch] *= g;
        }

        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, i, x[ch]);
    }
    compGrDb = minGr;
    transientDb = transMax;

    // ---- Oversampled: exciter + clipper ----
    const bool excOn = param ("exc_on") > 0.5f;
    const float excFreq = param ("exc_freq");
    if (excFreq != lastExcFreq)
    {
        exciterHp.set (BiquadCoeffs::make (FilterType::highPass, sr * 2.0, excFreq, 0.7071));
        lastExcFreq = excFreq;
    }
    const float drive = 1.0f + 9.0f * param ("exc_drive");
    excAmount.setTargetValue (excOn ? param ("exc_amount") : 0.0f);
    const bool clipOn = param ("clip_on") > 0.5f;
    clipCeil.setTargetValue (dbToGain (param ("clip")));

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
    auto os = oversampling->processSamplesUp (block);
    float clipIn = 0.0f, clipOut = 0.0f;
    for (size_t i = 0; i < os.getNumSamples(); ++i)
    {
        const float amt = excAmount.getNextValue();
        const float ceil = clipCeil.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float v = os.getSample (ch, (int) i);
            const float hp = exciterHp.process (ch, v);
            if (amt > 0.0001f)
                v += amt * std::tanh (drive * hp) * (1.5f / std::sqrt (drive));
            if (clipOn && ceil < 0.999f)
            {
                clipIn = juce::jmax (clipIn, std::abs (v));
                v = softClip (v, ceil);
                clipOut = juce::jmax (clipOut, std::abs (v));
            }
            os.setSample (ch, (int) i, v);
        }
    }
    clipGrDb = clipIn > 1.0e-5f ? juce::jmin (0.0f, gainToDb (clipOut) - gainToDb (clipIn)) : 0.0f;
    oversampling->processSamplesDown (block);

    // ---- Width + output ----
    float outPk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float w = widthSm.getNextValue(), og = outGain.getNextValue();
        if (numCh == 2)
        {
            const float l = buffer.getSample (0, i), r = buffer.getSample (1, i);
            const float m = 0.5f * (l + r), s = 0.5f * (l - r) * w;
            buffer.setSample (0, i, (m + s) * og);
            buffer.setSample (1, i, (m - s) * og);
        }
        else
            buffer.setSample (0, i, buffer.getSample (0, i) * og);
    }
    for (int ch = 0; ch < numCh; ++ch) outPk = juce::jmax (outPk, buffer.getMagnitude (ch, 0, n));
    outputPeakDb = gainToDb (outPk, -100.0f);
}

juce::AudioProcessorEditor* MixProcessor::createEditor() { return new MixEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MixProcessor(); }
