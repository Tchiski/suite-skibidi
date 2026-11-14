#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tchk;

namespace
{
    constexpr double kDivisionBeats[] = { 1.0, 0.75, 0.5, 1.0 / 3.0, 0.25, 1.5 };
    constexpr float kLevelTargetDb = -20.0f; // RMS target of the auto-level
}

juce::StringArray VocalProcessor::getDivisionNames() { return { "1/4", "1/8 D", "1/8", "1/8 T", "1/16", "1/4 D" }; }

juce::AudioProcessorValueTreeState::ParameterLayout VocalProcessor::createParameterLayout()
{
    ParamList p;
    p.add ("input", "Input", -18.0f, 18.0f, 0.0f, Unit::dB);

    p.addBool ("tune_on", "Autotune On", false);
    p.addChoice ("tune_key", "Autotune Key", tchk::AutoTune::keyNames(), 0);
    p.addChoice ("tune_scale", "Autotune Scale", tchk::AutoTune::scaleNames(), 0);
    p.add ("tune_speed", "Autotune Speed", 0.0f, 400.0f, 20.0f, Unit::ms, 0.4f);
    p.add ("tune_amount", "Autotune Amount", 0.0f, 1.0f, 1.0f, Unit::percent);

    p.addBool ("level_on", "Auto-Level On", true);
    p.add ("level_amt", "Auto-Level Amount", 0.0f, 1.0f, 0.5f, Unit::percent);

    p.addBool ("deess_on", "De-esser On", true);
    p.add ("deess_freq", "De-esser Freq", 3000.0f, 11000.0f, 6000.0f, Unit::hz, 0.6f);
    p.add ("deess_amt", "De-esser Amount", 0.0f, 1.0f, 0.4f, Unit::percent);

    p.addBool ("eq_on", "EQ On", true);
    p.add ("lowcut", "Low Cut", 20.0f, 400.0f, 90.0f, Unit::hz, 0.5f);
    p.add ("mud", "Mud Cut", 0.0f, 1.0f, 0.3f, Unit::percent);
    p.add ("presence", "Presence", -6.0f, 9.0f, 2.0f, Unit::dB);
    p.add ("air", "Air", -6.0f, 9.0f, 2.5f, Unit::dB);

    p.addBool ("comp_on", "Comp On", true);
    p.add ("comp_amt", "Compression", 0.0f, 1.0f, 0.5f, Unit::percent);
    p.add ("comp_speed", "Comp Speed", 0.0f, 1.0f, 0.5f, Unit::percent);

    p.addBool ("warm_on", "Warmth On", true);
    p.add ("warmth", "Warmth", 0.0f, 1.0f, 0.2f, Unit::percent);

    p.addBool ("dbl_on", "Doubler On", false);
    p.add ("dbl_amt", "Doubler Amount", 0.0f, 1.0f, 0.4f, Unit::percent);
    p.add ("dbl_width", "Doubler Width", 0.0f, 1.0f, 0.8f, Unit::percent);

    p.addBool ("delay_on", "Delay On", false);
    p.add ("delay_mix", "Delay Mix", 0.0f, 1.0f, 0.2f, Unit::percent);
    p.addChoice ("delay_div", "Delay Time", getDivisionNames(), 1);
    p.add ("delay_fb", "Delay Feedback", 0.0f, 0.85f, 0.3f, Unit::percent);

    p.addBool ("verb_on", "Reverb On", true);
    p.add ("verb_mix", "Reverb Mix", 0.0f, 1.0f, 0.15f, Unit::percent);
    p.add ("verb_size", "Reverb Size", 0.0f, 1.0f, 0.5f, Unit::percent);

    p.add ("output", "Output", -18.0f, 12.0f, 0.0f, Unit::dB);
    return p.build();
}

juce::StringArray VocalProcessor::getPresetNames()
{
    return { "Rap lead", "Rap douce / melo", "Chant pop", "Backs / choeurs", "Adlibs", "Voix podcast", "Lo-fi / radio",
             // artist-style presets (inspired by their usual vocal sound)
             "Travis Scott", "Drake", "Kanye West (808s)", "Future", "The Weeknd", "PNL", "Jul", "Ninho", "Damso" };
}

void VocalProcessor::applyPreset (int index)
{
    struct P { float lvl, dsf, dsa, lc, mud, pres, air, comp, speed, warm; bool dbl; float dbla, dblw; bool dly; float dmix; int div; float fb; float vmix, vsize;
               bool tune; float tspeed, tamt; };
    static const P presets[] = {
        //  lvl   ds-freq ds-amt lowcut mud  pres  air   comp  speed warm  dbl    dbl-a dbl-w  delay  d-mix div fb     v-mix v-size tune   t-ms  t-amt
        { 0.6f, 6500, 0.5f,  100, 0.4f, 3.0f, 3.0f,  0.65f, 0.7f, 0.25f, false, 0.3f,  0.8f, false, 0.15f, 1, 0.30f, 0.10f, 0.40f, false, 20,  1.0f }, // Rap lead
        { 0.5f, 6000, 0.4f,   90, 0.3f, 2.0f, 3.5f,  0.50f, 0.5f, 0.20f, true,  0.25f, 0.7f, true,  0.18f, 1, 0.35f, 0.22f, 0.60f, false, 20,  1.0f }, // Rap douce / melo
        { 0.5f, 6500, 0.45f,  90, 0.3f, 2.5f, 4.0f,  0.50f, 0.5f, 0.15f, false, 0.3f,  0.8f, true,  0.12f, 0, 0.30f, 0.25f, 0.65f, false, 20,  1.0f }, // Chant pop
        { 0.7f, 6000, 0.6f,  150, 0.5f, 0.0f, 3.0f,  0.70f, 0.6f, 0.20f, true,  0.6f,  1.0f, false, 0.10f, 1, 0.30f, 0.30f, 0.70f, false, 20,  1.0f }, // Backs / choeurs
        { 0.6f, 6500, 0.5f,  200, 0.5f, 4.0f, 2.0f,  0.70f, 0.8f, 0.40f, true,  0.4f,  1.0f, true,  0.30f, 2, 0.45f, 0.25f, 0.50f, false, 20,  1.0f }, // Adlibs
        { 0.8f, 5500, 0.4f,   80, 0.4f, 2.0f, 1.0f,  0.60f, 0.4f, 0.10f, false, 0.3f,  0.8f, false, 0.10f, 1, 0.30f, 0.00f, 0.30f, false, 20,  1.0f }, // Voix podcast
        { 0.6f, 6000, 0.3f,  400, 0.6f, 6.0f, -6.0f, 0.75f, 0.7f, 0.70f, false, 0.3f,  0.8f, true,  0.20f, 1, 0.40f, 0.15f, 0.30f, false, 20,  1.0f }, // Lo-fi / radio
        // --- artist styles ---
        // Travis Scott: hard, instant autotune, dark and wide, drowned in delay + big reverb
        { 0.6f, 6000, 0.5f,  120, 0.45f, 2.0f, 1.5f, 0.70f, 0.7f, 0.35f, true,  0.35f, 1.0f, true,  0.26f, 1, 0.50f, 0.32f, 0.88f, true,  0,   1.0f },
        // Drake: intimate and smooth, subtle tuning, warm low-mids, short polished space
        { 0.6f, 6000, 0.45f,  80, 0.2f,  1.5f, 2.5f, 0.60f, 0.4f, 0.25f, false, 0.2f,  0.6f, true,  0.10f, 0, 0.25f, 0.16f, 0.55f, true,  70,  0.6f },
        // Kanye West (808s & Heartbreak era): robotic tuning, saturated, cold echoes
        { 0.6f, 6500, 0.4f,  110, 0.35f, 2.5f, 2.0f, 0.70f, 0.6f, 0.45f, true,  0.30f, 0.8f, true,  0.20f, 0, 0.40f, 0.30f, 0.78f, true,  0,   1.0f },
        // Future: fast tuning, dark and gritty, mid-forward, 1/8 echoes
        { 0.6f, 6000, 0.5f,  120, 0.4f,  3.0f, 0.5f, 0.70f, 0.7f, 0.50f, true,  0.35f, 0.9f, true,  0.22f, 2, 0.40f, 0.22f, 0.70f, true,  5,   1.0f },
        // The Weeknd: airy and bright, medium tuning, lush reverb, dotted echoes
        { 0.5f, 6500, 0.45f,  90, 0.25f, 2.0f, 4.5f, 0.55f, 0.5f, 0.15f, true,  0.25f, 0.8f, true,  0.18f, 1, 0.35f, 0.34f, 0.92f, true,  30,  0.8f },
        // PNL: dreamy, heavy tuning, wide doubler, huge reverb and long echoes
        { 0.6f, 6000, 0.5f,  110, 0.35f, 2.0f, 3.0f, 0.65f, 0.5f, 0.30f, true,  0.40f, 1.0f, true,  0.25f, 0, 0.50f, 0.42f, 0.95f, true,  10,  1.0f },
        // Jul: instant robotic tuning, very present and bright, tight compression, little space
        { 0.7f, 6500, 0.55f, 130, 0.4f,  4.0f, 3.0f, 0.75f, 0.8f, 0.25f, false, 0.3f,  0.8f, true,  0.14f, 2, 0.30f, 0.12f, 0.45f, true,  0,   1.0f },
        // Ninho: clean modern tuning, upfront and polished
        { 0.6f, 6500, 0.5f,  110, 0.35f, 3.0f, 2.5f, 0.70f, 0.6f, 0.25f, false, 0.3f,  0.8f, true,  0.12f, 2, 0.30f, 0.18f, 0.55f, true,  15,  0.9f },
        // Damso: deep, close and dark, light natural tuning, very little space
        { 0.6f, 6000, 0.45f,  70, 0.3f,  2.5f, 1.0f, 0.65f, 0.4f, 0.35f, false, 0.3f,  0.8f, false, 0.10f, 1, 0.30f, 0.12f, 0.50f, true,  90,  0.4f },
    };
    const auto& v = presets[juce::jlimit (0, (int) std::size (presets) - 1, index)];
    auto s = [this] (const char* id, float x) { tchk::setParam (apvts, id, x); };
    s ("level_amt", v.lvl); s ("deess_freq", v.dsf); s ("deess_amt", v.dsa); s ("lowcut", v.lc); s ("mud", v.mud);
    s ("presence", v.pres); s ("air", v.air); s ("comp_amt", v.comp); s ("comp_speed", v.speed); s ("warmth", v.warm);
    s ("dbl_on", v.dbl ? 1.0f : 0.0f); s ("dbl_amt", v.dbla); s ("dbl_width", v.dblw);
    s ("delay_on", v.dly ? 1.0f : 0.0f); s ("delay_mix", v.dmix); s ("delay_div", (float) v.div); s ("delay_fb", v.fb);
    s ("verb_on", v.vmix > 0.0f ? 1.0f : 0.0f); s ("verb_mix", v.vmix); s ("verb_size", v.vsize);
    // autotune: the key / scale stay as the user set them (they depend on the song)
    s ("tune_on", v.tune ? 1.0f : 0.0f); s ("tune_speed", v.tspeed); s ("tune_amount", v.tamt);
    for (auto* id : { "level_on", "deess_on", "eq_on", "comp_on", "warm_on" }) s (id, 1.0f);
}

std::vector<BiquadCoeffs> VocalProcessor::makeEqCoeffs (const juce::AudioProcessorValueTreeState& s, double fs)
{
    auto v = [&s] (const char* id) { return (double) s.getRawParameterValue (id)->load(); };
    const auto lc = BiquadCoeffs::make (FilterType::highPass, fs, v ("lowcut"), 0.7071);
    return { lc, lc,
             BiquadCoeffs::make (FilterType::peak, fs, 300.0, 1.0, -6.0 * v ("mud")),
             BiquadCoeffs::make (FilterType::peak, fs, 3500.0, 0.8, v ("presence")),
             BiquadCoeffs::make (FilterType::highShelf, fs, 12000.0, 0.7, v ("air")) };
}

VocalProcessor::VocalProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

bool VocalProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Mono or stereo in, stereo or mono out (a mono vocal track can still get stereo doubler/reverb)
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in = layouts.getMainInputChannelSet();
    const bool okOut = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    const bool okIn = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    return okOut && okIn && in.size() <= out.size();
}

double VocalProcessor::hostBpm() const
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 1.0) return *bpm;
    return 120.0;
}

void VocalProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlockSize = juce::jmax (1, samplesPerBlock);

    levelPow = 0.0f; levelGainDb = 0.0f;
    kLevelPow = onePoleCoef (sr, 400.0);
    kLevelGain = 1.0f - onePoleCoef (sr, 250.0);

    lastDeessFreq = -1.0f;
    sibEnv = fullEnv = deessGr = 0.0f;
    kEnvAtt = onePoleCoef (sr, 1.0);
    kEnvRel = onePoleCoef (sr, 60.0);
    sibDetect.reset();

    for (auto& f : eq) f.reset();
    lastEq.fill (-999.0f);

    comp.reset();
    compAvgPow = 1.0e-4f;
    kCompAvg = onePoleCoef (sr, 2000.0);
    comp.kneeDb = 8.0f;

    dc.prepare (sr);
    doublerLine.prepare ((int) (0.05 * sr));
    lfo1 = 0.0f; lfo2 = 1.3f;

    delayLine.prepare ((int) (3.0 * sr));
    delayHp.set (BiquadCoeffs::make (FilterType::highPass, sr, 300.0, 0.7071));
    delayLp.set (BiquadCoeffs::make (FilterType::lowPass, sr, 6000.0, 0.7071));
    delayHp.reset(); delayLp.reset();
    delaySamples.reset (sr, 0.3);
    delaySamples.setCurrentAndTargetValue ((float) (0.375 * sr));

    reverb.prepare ({ sr, (juce::uint32) maxBlockSize, 2 });
    reverb.reset();
    reverbHp.set (BiquadCoeffs::make (FilterType::highPass, sr, 250.0, 0.7071));
    reverbHp.reset();

    fxBuffer.setSize (2, maxBlockSize);
    reverbBuffer.setSize (2, maxBlockSize);

    autotune.prepare (sr);

    inGain.reset (sr, 0.05); inGain.setCurrentAndTargetValue (dbToGain (param ("input")));
    outGain.reset (sr, 0.05); outGain.setCurrentAndTargetValue (dbToGain (param ("output")));
}

void VocalProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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

    const int numIn = getTotalNumInputChannels();
    const int numCh = juce::jmin (2, getTotalNumOutputChannels(), buffer.getNumChannels());
    if (numCh <= 0 || n <= 0)
        return;
    // mono input on a stereo output: copy the voice to both sides
    if (numIn == 1 && numCh == 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    float inPk = 0.0f;
    for (int ch = 0; ch < numCh; ++ch) inPk = juce::jmax (inPk, buffer.getMagnitude (ch, 0, n));
    inputPeakDb = gainToDb (inPk, -100.0f);

    // ---- Autotune (first in the chain, on the raw voice) ----
    autotune.setVoiceRange (tuneRange.load());
    autotune.setParams (param ("tune_on") > 0.5f, (int) param ("tune_key"), (int) param ("tune_scale"),
                        param ("tune_speed"), param ("tune_amount"));
    autotune.process (buffer.getArrayOfWritePointers(), numCh, n);

    // ---- parameters ----
    const bool levelOn = param ("level_on") > 0.5f;
    const float levelAmt = param ("level_amt");
    const bool deessOn = param ("deess_on") > 0.5f;
    const float deessAmt = param ("deess_amt");
    const float deessFreq = param ("deess_freq");
    if (deessFreq != lastDeessFreq)
    {
        deessSplit.prepare (sr, deessFreq);
        sibDetect.set (BiquadCoeffs::make (FilterType::highPass, sr, deessFreq, 0.7071));
        lastDeessFreq = deessFreq;
    }
    const bool eqOn = param ("eq_on") > 0.5f;
    {
        const std::array<float, 4> now { param ("lowcut"), param ("mud"), param ("presence"), param ("air") };
        if (now != lastEq)
        {
            const auto c = makeEqCoeffs (apvts, sr);
            for (size_t k = 0; k < eq.size(); ++k) eq[k].set (c[k]);
            lastEq = now;
        }
    }
    const bool compOn = param ("comp_on") > 0.5f;
    const float compAmt = param ("comp_amt"), speed = param ("comp_speed");
    comp.ratio = 1.5f + 4.5f * compAmt;
    comp.setTimes (sr, 25.0f - 22.0f * speed, 250.0f - 180.0f * speed);
    const bool warmOn = param ("warm_on") > 0.5f;
    const float warmth = param ("warmth");
    const float drive = 1.0f + 5.0f * warmth;

    inGain.setTargetValue (dbToGain (param ("input")));
    outGain.setTargetValue (dbToGain (param ("output")));

    float minDeess = 0.0f, minComp = 0.0f;

    // ---- serial chain ----
    for (int i = 0; i < n; ++i)
    {
        const float ig = inGain.getNextValue();
        float x[2] = { buffer.getSample (0, i) * ig, numCh > 1 ? buffer.getSample (1, i) * ig : 0.0f };
        float level = juce::jmax (std::abs (x[0]), std::abs (x[1]));

        // Auto-level: rides the gain towards a constant RMS level (like an engineer on a fader)
        if (levelOn)
        {
            levelPow = level * level + kLevelPow * (levelPow - level * level);
            const float rmsDb = powerToDb (levelPow);
            const float target = rmsDb > -55.0f ? juce::jlimit (-12.0f, 12.0f, (kLevelTargetDb - rmsDb) * levelAmt) : levelGainDb;
            levelGainDb += (target - levelGainDb) * kLevelGain;
            const float g = dbToGain (levelGainDb);
            x[0] *= g; x[1] *= g;
        }

        for (int ch = 0; ch < numCh; ++ch)
            if (eqOn) x[ch] = eq[1].process (ch, eq[0].process (ch, x[ch])); // low cut first

        // De-esser: split-band, only the highs are turned down when "s" sounds dominate
        if (deessOn && deessAmt > 0.0f)
        {
            const float mono = numCh > 1 ? 0.5f * (x[0] + x[1]) : x[0];
            const float s = std::abs (sibDetect.process (0, mono));
            const float f = std::abs (mono);
            sibEnv = s + (s > sibEnv ? kEnvAtt : kEnvRel) * (sibEnv - s);
            fullEnv = f + (f > fullEnv ? kEnvAtt : kEnvRel) * (fullEnv - f);
            const float excess = gainToDb (sibEnv, -90.0f) - (gainToDb (fullEnv, -90.0f) - 4.0f);
            const float target = juce::jlimit (-18.0f, 0.0f, -deessAmt * juce::jmax (0.0f, excess) * 3.0f);
            deessGr += (target - deessGr) * (target < deessGr ? 0.5f : 0.002f);
            minDeess = juce::jmin (minDeess, deessGr);
            const float g = dbToGain (deessGr);
            for (int ch = 0; ch < numCh; ++ch)
            {
                float lo, hi;
                deessSplit.process (ch, x[ch], lo, hi);
                x[ch] = lo + hi * g;
            }
        }

        if (eqOn)
            for (int ch = 0; ch < numCh; ++ch)
                for (size_t k = 2; k < eq.size(); ++k)
                    x[ch] = eq[k].process (ch, x[ch]);

        // Compressor with a threshold that follows the voice's average level
        level = juce::jmax (std::abs (x[0]), std::abs (x[1]));
        if (compOn)
        {
            compAvgPow = level * level + kCompAvg * (compAvgPow - level * level);
            comp.thresholdDb = juce::jmax (-50.0f, powerToDb (compAvgPow)) + 6.0f - 16.0f * compAmt;
            const float gr = comp.process (gainToDb (level));
            minComp = juce::jmin (minComp, gr);
            const float g = dbToGain (gr + compAmt * 4.0f);
            x[0] *= g; x[1] *= g;
        }

        if (warmOn && warmth > 0.0f)
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float s = dc.process (ch, saturate (x[ch] * 0.7f, drive, SatMode::tube) / 0.7f);
                x[ch] += juce::jmin (1.0f, warmth * 2.0f) * (s - x[ch]);
            }

        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, i, x[ch]);
    }
    deesserGrDb = minDeess;
    compGrDb = minComp;
    levelerDb = levelOn ? levelGainDb : 0.0f;

    // ---- Doubler: two slightly detuned, delayed copies panned left/right ----
    if (param ("dbl_on") > 0.5f && numCh == 2)
    {
        const float amt = param ("dbl_amt") * 0.6f, wdt = param ("dbl_width");
        const float inc1 = juce::MathConstants<float>::twoPi * 0.31f / (float) sr;
        const float inc2 = juce::MathConstants<float>::twoPi * 0.43f / (float) sr;
        for (int i = 0; i < n; ++i)
        {
            const float mono = 0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i));
            doublerLine.write (0, mono);
            lfo1 += inc1; if (lfo1 > juce::MathConstants<float>::twoPi) lfo1 -= juce::MathConstants<float>::twoPi;
            lfo2 += inc2; if (lfo2 > juce::MathConstants<float>::twoPi) lfo2 -= juce::MathConstants<float>::twoPi;
            const float a = doublerLine.read (0, (float) sr * (0.013f + 0.0015f * std::sin (lfo1)));
            const float b = doublerLine.read (0, (float) sr * (0.021f + 0.0018f * std::sin (lfo2)));
            doublerLine.advance();
            const float l = a * (0.5f + 0.5f * wdt) + b * (0.5f - 0.5f * wdt);
            const float r = b * (0.5f + 0.5f * wdt) + a * (0.5f - 0.5f * wdt);
            buffer.addSample (0, i, amt * l);
            buffer.addSample (1, i, amt * r);
        }
    }

    // ---- Sends: delay + reverb (computed from the processed voice) ----
    const bool delayOn = param ("delay_on") > 0.5f;
    const bool verbOn = param ("verb_on") > 0.5f;
    const float delayMix = delayOn ? param ("delay_mix") : 0.0f;
    const float verbMix = verbOn ? param ("verb_mix") : 0.0f;

    if (delayMix > 0.0f || delayOn)
    {
        const int div = juce::jlimit (0, 5, (int) param ("delay_div"));
        const float target = (float) juce::jlimit (0.01, 2.9, 60.0 / hostBpm() * kDivisionBeats[div]) * (float) sr;
        delaySamples.setTargetValue (target);
        const float fb = param ("delay_fb");
        for (int i = 0; i < n; ++i)
        {
            const float d = delaySamples.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                // the right side is slightly later for a wide, natural echo
                const float tap = delayLine.read (ch, d * (ch == 1 ? 1.02f : 1.0f));
                const float filtered = delayLp.process (ch, delayHp.process (ch, tap));
                delayLine.write (ch, buffer.getSample (ch, i) + std::tanh (filtered * fb));
                fxBuffer.setSample (ch, i, filtered);
            }
            delayLine.advance();
        }
    }
    else
        fxBuffer.clear (0, n);

    if (verbMix > 0.0f)
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = 0.5f + 0.48f * param ("verb_size");
        rp.damping = 0.45f; rp.width = 1.0f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f;
        reverb.setParameters (rp);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const int src = juce::jmin (ch, numCh - 1);
                const float in = buffer.getSample (src, i) + fxBuffer.getSample (src, i) * 0.5f;
                reverbBuffer.setSample (ch, i, reverbHp.process (ch, in));
            }
        auto block = juce::dsp::AudioBlock<float> (reverbBuffer).getSubBlock (0, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
    }

    float outPk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float og = outGain.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float y = buffer.getSample (ch, i) + delayMix * fxBuffer.getSample (ch, i);
            if (verbMix > 0.0f) y += verbMix * reverbBuffer.getSample (numCh == 1 ? 0 : ch, i);
            buffer.setSample (ch, i, y * og);
        }
    }
    for (int ch = 0; ch < numCh; ++ch) outPk = juce::jmax (outPk, buffer.getMagnitude (ch, 0, n));
    outputPeakDb = gainToDb (outPk, -100.0f);
}

juce::AudioProcessorEditor* VocalProcessor::createEditor() { return new VocalEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalProcessor(); }
