#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tchk;

namespace
{
    // Target tonal balance (dB relative to the band average) of a modern, well-balanced master.
    constexpr std::array<float, MasterProcessor::numSpectralBands> kTargetCurve { 4.5f, 2.5f, 0.0f, -1.5f, -3.5f, -7.0f };
    constexpr float kBassMonoFreq = 120.0f;
    constexpr float kLookaheadMs = 1.5f;

    juce::AudioParameterFloatAttributes percentAttr()
    {
        return juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });
    }
    juce::AudioParameterFloatAttributes dbAttr (int decimals = 1)
    {
        return juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [decimals] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, decimals) + " dB"; });
    }
}

//==============================================================================
juce::StringArray MasterProcessor::getTargetNames()
{
    return { "Streaming -14", "Apple Music -16", "YouTube -13", "Fort -10", "Club -8", "Tres fort -6" };
}

float MasterProcessor::getTargetLufs (int index)
{
    static const float values[] = { -14.0f, -16.0f, -13.0f, -10.0f, -8.0f, -6.0f };
    return values[juce::jlimit (0, 5, index)];
}

juce::StringArray MasterProcessor::getPresetNames()
{
    return { "Defaut", "Rap / Trap", "Pop / Voix", "Club / EDM", "Lo-fi / Chill", "Doux & naturel", "Instru 808" };
}

juce::AudioProcessorValueTreeState::ParameterLayout MasterProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto addF = [&p] (const char* id, const char* name, float lo, float hi, float def, AudioParameterFloatAttributes a)
    { p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi), def, a)); };
    auto addB = [&p] (const char* id, const char* name, bool def)
    { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    addF ("input", "Input", -12.0f, 12.0f, 0.0f, dbAttr());

    addB ("spec_on", "Spectral On", true);
    addF ("spec_amt", "Spectral Amount", 0.0f, 1.0f, 0.5f, percentAttr());

    addB ("comp_on", "Comp On", true);
    addF ("glue", "Glue", 0.0f, 1.0f, 0.4f, percentAttr());
    addF ("detail", "Detail", 0.0f, 1.0f, 0.3f, percentAttr());

    addB ("eq_on", "EQ On", true);
    addF ("bass", "Bass", -6.0f, 6.0f, 0.0f, dbAttr());
    addF ("air", "Air", -6.0f, 6.0f, 0.0f, dbAttr());

    addB ("stereo_on", "Stereo On", true);
    addF ("width", "Width", 0.0f, 2.0f, 1.0f, percentAttr());
    addB ("bass_mono", "Bass Mono", true);

    addB ("analog_on", "Analog On", true);
    addF ("color", "Color", 0.0f, 1.0f, 0.3f, percentAttr());
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "analog_mode", 1 }, "Analog Mode",
                                                         StringArray { "Tape", "Tube", "Crunch" }, 0));

    addB ("clip_on", "Clip On", true);
    addF ("clip_gain", "Clip Gain", 0.0f, 12.0f, 1.0f, dbAttr());

    addB ("lim_on", "Limiter On", true);
    addF ("lim_gain", "Limiter Gain", 0.0f, 20.0f, 4.0f, dbAttr());
    addF ("ceiling", "Ceiling", -3.0f, 0.0f, -1.0f, dbAttr());
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release",
        NormalisableRange<float> (10.0f, 500.0f, 0.0f, 0.4f), 80.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));
    addF ("output", "Output", -12.0f, 0.0f, 0.0f, dbAttr());

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "target", 1 }, "Target", getTargetNames(), 3));
    addB ("auto", "Auto", false);
    addB ("bypass", "Bypass", false);
    addB ("match", "Match", true);
    addF ("spice", "Spice", 0.0f, 1.0f, 0.35f, percentAttr());

    return { p.begin(), p.end() };
}

//==============================================================================
MasterProcessor::MasterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    for (auto& g : bandGrDb) g = 0.0f;
    for (auto& g : spectralGainDb) g = 0.0f;
    startTimerHz (2);
}

MasterProcessor::~MasterProcessor()
{
    stopTimer();
}

bool MasterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void MasterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlockSize = juce::jmax (1, samplesPerBlock);
    const int numCh = juce::jlimit (1, 2, getTotalNumOutputChannels());
    const int osFactor = 1 << osFactorLog2;
    const double osRate = sr * osFactor;

    // Spectral
    for (int b = 0; b < numSpectralBands; ++b)
    {
        spectralAnalysis[(size_t) b].set (BiquadCoeffs::make (FilterType::bandPass, sr, spectralFreqs[(size_t) b], 0.8));
        spectralAnalysis[(size_t) b].reset();
        spectralFilters[(size_t) b].reset();
        bandEnergy[(size_t) b] = 0.0f;
        spectralCorr[(size_t) b] = 0.0f;
    }
    setSpectralFilters();
    spectralEnergyCoef = onePoleCoef (sr, 400.0);
    spectralSmoothK = 1.0f - std::exp (-64.0f / (float) (sr * 1.2));
    spectralCounter = 0;

    // Comp
    crossover.prepare (sr, 120.0f, 2500.0f);
    const float att[3] = { 30.0f, 20.0f, 10.0f }, rel[3] = { 160.0f, 120.0f, 90.0f };
    for (int b = 0; b < 3; ++b)
    {
        compressors[(size_t) b].setTimes (sr, att[b], rel[b]);
        compressors[(size_t) b].kneeDb = 6.0f;
        compressors[(size_t) b].reset();
        bandAvgPow[(size_t) b] = bandFastPow[(size_t) b] = 1.0e-6f;
        upwardDb[(size_t) b] = 0.0f;
    }
    avgCoef = onePoleCoef (sr, 3000.0);
    fastCoef = onePoleCoef (sr, 40.0);
    upAttK = 1.0f - onePoleCoef (sr, 60.0);
    upRelK = 1.0f - onePoleCoef (sr, 250.0);

    // EQ + stereo
    bassShelf.reset(); airShelf.reset();
    lastBass = lastAir = 99.0f;
    monoSplit.prepare (sr, kBassMonoFreq);

    // Oversampled section
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, (size_t) osFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing ((size_t) juce::jmax (1, samplesPerBlock));
    oversampling->reset();
    dcBlocker.prepare (osRate);

    const int lookaheadBase = juce::jmax (1, (int) std::round (kLookaheadMs * 0.001 * sr));
    limiter.prepare (osRate, lookaheadBase * osFactor);

    latencySamples = (int) std::round (oversampling->getLatencyInSamples()) + lookaheadBase;
    setLatencySamples (latencySamples);

    dryDelay.prepare (latencySamples + 8);
    dryBuffer.setSize (2, juce::jmax (1, samplesPerBlock));

    // Smoothers
    auto init = [] (juce::SmoothedValue<float>& s, double rate, double secs, float v) { s.reset (rate, secs); s.setCurrentAndTargetValue (v); };
    init (inputGain, sr, 0.05, dbToGain (param ("input")));
    init (outputGain, sr, 0.05, dbToGain (param ("output")));
    init (widthSmoothed, sr, 0.05, param ("width"));
    init (bypassMix, sr, 0.03, param ("bypass") > 0.5f ? 1.0f : 0.0f);
    init (matchGain, sr, 0.3, 1.0f);
    init (driveSmoothed, osRate, 0.05, param ("color"));
    init (clipGainSmoothed, osRate, 0.05, dbToGain (param ("clip_gain")));
    init (limGainSmoothed, osRate, 0.1, dbToGain (param ("lim_gain")));

    inputLoudness.prepare (sr);
    outputLoudness.prepare (sr);

    corrLR = corrLL = corrRR = 0.0f;
    corrCoef = onePoleCoef (sr, 300.0);
    peakHold = -100.0f;
    peakHoldSamples = 0;
}

void MasterProcessor::setSpectralFilters()
{
    for (int b = 0; b < numSpectralBands; ++b)
    {
        const float g = spectralCorr[(size_t) b];
        BiquadCoeffs c;
        if (b == 0)                         c = BiquadCoeffs::make (FilterType::lowShelf, sr, 100.0, 0.7, g);
        else if (b == numSpectralBands - 1) c = BiquadCoeffs::make (FilterType::highShelf, sr, 9000.0, 0.7, g);
        else                                c = BiquadCoeffs::make (FilterType::peak, sr, spectralFreqs[(size_t) b], 0.9, g);
        spectralFilters[(size_t) b].set (c);
        spectralGainDb[(size_t) b] = g;
    }
}

void MasterProcessor::updateSpectral (float amount)
{
    std::array<float, numSpectralBands> db {};
    float mean = 0.0f, maxDb = -200.0f;
    for (int b = 0; b < numSpectralBands; ++b)
    {
        db[(size_t) b] = powerToDb (bandEnergy[(size_t) b]);
        mean += db[(size_t) b];
        maxDb = juce::jmax (maxDb, db[(size_t) b]);
    }
    mean /= (float) numSpectralBands;

    const bool hasSignal = maxDb > -65.0f;
    for (int b = 0; b < numSpectralBands; ++b)
    {
        float target = spectralCorr[(size_t) b];
        if (hasSignal)
        {
            const float error = (db[(size_t) b] - mean) - kTargetCurve[(size_t) b];
            target = juce::jlimit (-6.0f, 6.0f, -0.8f * amount * error);
        }
        spectralCorr[(size_t) b] += (target - spectralCorr[(size_t) b]) * spectralSmoothK;
    }
    setSpectralFilters();
}

//==============================================================================
void MasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    if (n > maxBlockSize) // some hosts send bigger blocks than announced: process in chunks
    {
        for (int start = 0; start < n; start += maxBlockSize)
        {
            juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, juce::jmin (maxBlockSize, n - start));
            processBlock (sub, midi);
        }
        return;
    }

    const int totalIn = getTotalNumInputChannels(), totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, n);

    const int numCh = juce::jmin (2, totalOut, buffer.getNumChannels());
    if (numCh <= 0 || n <= 0 || oversampling == nullptr)
        return;

    if (dryBuffer.getNumSamples() < n)
        dryBuffer.setSize (2, n, false, false, true);

    // ---- Input metering + keep the raw signal for bypass ----
    float inPeak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
    {
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, n);
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (ch, 0, n));
    }
    inputPeakDb = gainToDb (inPeak, -100.0f);
    inputLoudness.process (dryBuffer.getArrayOfReadPointers(), numCh, n);

    // ---- Parameters ----
    const bool specOn = param ("spec_on") > 0.5f;
    const float specAmt = param ("spec_amt");
    const bool compOn = param ("comp_on") > 0.5f;
    const float glue = param ("glue"), detail = param ("detail");
    const bool eqOn = param ("eq_on") > 0.5f;
    const bool stereoOn = param ("stereo_on") > 0.5f && numCh == 2;
    const bool bassMono = param ("bass_mono") > 0.5f;
    const bool analogOn = param ("analog_on") > 0.5f;
    const auto satMode = (SatMode) juce::jlimit (0, 2, (int) param ("analog_mode"));
    const bool clipOn = param ("clip_on") > 0.5f;
    const bool limOn = param ("lim_on") > 0.5f;
    const float ceilingLin = dbToGain (param ("ceiling"));

    inputGain.setTargetValue (dbToGain (param ("input")));
    outputGain.setTargetValue (dbToGain (param ("output")));
    widthSmoothed.setTargetValue (param ("width"));
    bypassMix.setTargetValue (param ("bypass") > 0.5f ? 1.0f : 0.0f);
    driveSmoothed.setTargetValue (analogOn ? param ("color") : 0.0f);
    clipGainSmoothed.setTargetValue (clipOn ? dbToGain (param ("clip_gain")) : 1.0f);
    limGainSmoothed.setTargetValue (limOn ? dbToGain (param ("lim_gain")) : 1.0f);

    {
        const float bass = eqOn ? param ("bass") : 0.0f, air = eqOn ? param ("air") : 0.0f;
        if (bass != lastBass) { bassShelf.set (BiquadCoeffs::make (FilterType::lowShelf, sr, 80.0, 0.7, bass)); lastBass = bass; }
        if (air != lastAir)   { airShelf.set (BiquadCoeffs::make (FilterType::highShelf, sr, 12000.0, 0.7, air)); lastAir = air; }
    }

    if (! specOn && std::abs (spectralCorr[0]) + std::abs (spectralCorr[5]) > 0.0f)
    {
        spectralCorr.fill (0.0f);
        setSpectralFilters();
    }

    std::array<float, 3> blockGr { 0.0f, 0.0f, 0.0f };

    // ---- Base-rate section: input, spectral, comp, EQ, stereo ----
    for (int i = 0; i < n; ++i)
    {
        const float ig = inputGain.getNextValue();
        float x[2] = { buffer.getSample (0, i) * ig, numCh > 1 ? buffer.getSample (1, i) * ig : 0.0f };

        if (specOn)
        {
            const float mono = numCh > 1 ? 0.5f * (x[0] + x[1]) : x[0];
            for (int b = 0; b < numSpectralBands; ++b)
            {
                const float y = spectralAnalysis[(size_t) b].process (0, mono);
                bandEnergy[(size_t) b] = y * y + spectralEnergyCoef * (bandEnergy[(size_t) b] - y * y);
            }
            if (++spectralCounter >= 64)
            {
                spectralCounter = 0;
                updateSpectral (specAmt);
            }
            for (int ch = 0; ch < numCh; ++ch)
                for (auto& f : spectralFilters)
                    x[ch] = f.process (ch, x[ch]);
        }

        if (compOn)
        {
            float bands[3][2] {};
            for (int ch = 0; ch < numCh; ++ch)
                crossover.process (ch, x[ch], bands[0][ch], bands[1][ch], bands[2][ch]);

            for (int b = 0; b < 3; ++b)
            {
                const float pk = juce::jmax (std::abs (bands[b][0]), std::abs (bands[b][1]));
                const float pw = pk * pk;
                bandAvgPow[(size_t) b] = pw + avgCoef * (bandAvgPow[(size_t) b] - pw);
                bandFastPow[(size_t) b] = pw + fastCoef * (bandFastPow[(size_t) b] - pw);
                const float avgDb = powerToDb (bandAvgPow[(size_t) b]);
                const float fastDb = powerToDb (bandFastPow[(size_t) b]);

                auto& c = compressors[(size_t) b];
                c.thresholdDb = avgDb + 8.0f - 10.0f * glue;
                c.ratio = 1.0f + 3.0f * glue;
                const float gr = glue > 0.001f ? c.process (gainToDb (pk)) : 0.0f;
                blockGr[(size_t) b] = juce::jmin (blockGr[(size_t) b], gr);

                // upward compression: lift quiet details back towards the average
                float upTarget = 0.0f;
                if (fastDb > -70.0f && b > 0)
                    upTarget = detail * juce::jlimit (0.0f, 12.0f, (avgDb - 6.0f) - fastDb) * 0.5f;
                auto& up = upwardDb[(size_t) b];
                up += (upTarget - up) * (upTarget > up ? upAttK : upRelK);

                const float g = dbToGain (gr + up + glue * 2.0f);
                bands[b][0] *= g;
                bands[b][1] *= g;
            }
            for (int ch = 0; ch < numCh; ++ch)
                x[ch] = bands[0][ch] + bands[1][ch] + bands[2][ch];
        }

        if (eqOn)
            for (int ch = 0; ch < numCh; ++ch)
                x[ch] = airShelf.process (ch, bassShelf.process (ch, x[ch]));

        const float width = widthSmoothed.getNextValue();
        if (stereoOn)
        {
            float lowMono = 0.0f, hi[2] = { x[0], x[1] };
            if (bassMono)
            {
                float lo[2];
                monoSplit.process (0, x[0], lo[0], hi[0]);
                monoSplit.process (1, x[1], lo[1], hi[1]);
                lowMono = 0.5f * (lo[0] + lo[1]);
            }
            const float mid = 0.5f * (hi[0] + hi[1]);
            const float side = 0.5f * (hi[0] - hi[1]) * width;
            x[0] = lowMono + mid + side;
            x[1] = lowMono + mid - side;
        }

        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, i, x[ch]);
    }

    for (int b = 0; b < 3; ++b)
        bandGrDb[(size_t) b] = compOn ? blockGr[(size_t) b] : 0.0f;

    // ---- Oversampled section: analog, clip, limiter ----
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
    auto osBlock = oversampling->processSamplesUp (block);
    const int osN = (int) osBlock.getNumSamples();
    float* osData[2] = { osBlock.getChannelPointer (0), numCh > 1 ? osBlock.getChannelPointer (1) : nullptr };

    float clipIn = 0.0f, clipOut = 0.0f;
    constexpr float headroom = 0.5f;

    for (int i = 0; i < osN; ++i)
    {
        const float color = driveSmoothed.getNextValue();
        const float drive = 1.0f + 4.0f * color;
        const float wet = juce::jmin (1.0f, 2.0f * color);
        const float cg = clipGainSmoothed.getNextValue();
        const float lg = limGainSmoothed.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            float v = osData[ch][i];
            if (wet > 0.0001f)
            {
                const float s = dcBlocker.process (ch, saturate (v * headroom, drive, satMode) / headroom);
                v += wet * (s - v);
            }
            if (clipOn)
            {
                v *= cg;
                clipIn = juce::jmax (clipIn, std::abs (v));
                v = softClip (v, 1.0f);
                clipOut = juce::jmax (clipOut, std::abs (v));
            }
            osData[ch][i] = v * lg;
        }
    }
    clipGrDb = clipIn > 1.0e-5f ? juce::jmin (0.0f, gainToDb (clipOut) - gainToDb (clipIn)) : 0.0f;

    limiter.setCeiling (limOn ? ceilingLin * 0.995f : 1000.0f);
    limiter.setRelease (param ("release"));
    const float minGain = limiter.process (osData, numCh, osN);
    limiterGrDb = limOn ? gainToDb (minGain) : 0.0f;

    float tp = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        for (int i = 0; i < osN; ++i)
            tp = juce::jmax (tp, std::abs (osData[ch][i]));

    oversampling->processSamplesDown (block);

    // ---- Output: safety clip, output gain, meters, bypass crossfade ----
    for (int i = 0; i < n; ++i)
    {
        const float og = outputGain.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float y = buffer.getSample (ch, i);
            if (limOn) y = juce::jlimit (-ceilingLin, ceilingLin, y);
            buffer.setSample (ch, i, y * og);
        }
        if (numCh == 2)
        {
            const float l = buffer.getSample (0, i), r = buffer.getSample (1, i);
            corrLR = l * r + corrCoef * (corrLR - l * r);
            corrLL = l * l + corrCoef * (corrLL - l * l);
            corrRR = r * r + corrCoef * (corrRR - r * r);
        }
    }
    correlation = numCh == 2 && corrLL * corrRR > 1.0e-10f ? juce::jlimit (-1.0f, 1.0f, corrLR / std::sqrt (corrLL * corrRR)) : 1.0f;

    outputLoudness.process (buffer.getArrayOfReadPointers(), numCh, n);

    if (scopeWanted.load (std::memory_order_relaxed))
    {
        // one pair every 4 samples is plenty for the display
        const int pairs = n / 4;
        int s1, n1, s2, n2;
        scopeFifo.prepareToWrite (pairs, s1, n1, s2, n2);
        auto put = [&] (int start, int count, int offset)
        {
            for (int k = 0; k < count; ++k)
            {
                const int i = (offset + k) * 4;
                scopeL[(size_t) (start + k)] = buffer.getSample (0, i);
                scopeR[(size_t) (start + k)] = numCh > 1 ? buffer.getSample (1, i) : buffer.getSample (0, i);
            }
        };
        put (s1, n1, 0);
        put (s2, n2, n1);
        scopeFifo.finishedWrite (n1 + n2);
    }

    const float tpDb = gainToDb (tp * outputGain.getCurrentValue(), -100.0f);
    truePeakDb = tpDb;
    if (tpDb >= peakHold || (peakHoldSamples -= n) <= 0)
    {
        peakHold = tpDb;
        peakHoldSamples = (int) (3.0 * sr);
    }
    truePeakHoldDb = peakHold;

    // Loudness-matched bypass: the original is played at the processed loudness
    const bool match = param ("match") > 0.5f;
    const float inS = inputLoudness.shortTerm.load(), outS = outputLoudness.shortTerm.load();
    const float matchDb = (match && inS > -60.0f && outS > -60.0f) ? juce::jlimit (-24.0f, 24.0f, outS - inS) : 0.0f;
    matchGain.setTargetValue (dbToGain (matchDb));

    const float delaySamples = (float) latencySamples;
    for (int i = 0; i < n; ++i)
    {
        const float bp = bypassMix.getNextValue();
        const float mg = matchGain.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            dryDelay.write (ch, dryBuffer.getSample (ch, i));
            const float dry = dryDelay.read (ch, delaySamples);
            if (bp > 0.0f)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * (1.0f - bp) + dry * mg * bp);
        }
        dryDelay.advance();
    }
}

//==============================================================================
void MasterProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void MasterProcessor::timerCallback()
{
    if (param ("auto") < 0.5f)
        return;

    const float outS = outputLoudness.shortTerm.load();
    const float inM = inputLoudness.momentary.load();
    if (inM < -50.0f || outS < -60.0f)
        return; // silence: nothing to measure

    const float target = getTargetLufs ((int) param ("target"));
    const float error = target - outS;
    if (std::abs (error) < 0.3f)
        return;

    if (param ("lim_on") < 0.5f)
        setParam ("lim_on", 1.0f);

    const float step = juce::jlimit (-1.5f, 1.5f, error * 0.35f);
    setParam ("lim_gain", juce::jlimit (0.0f, 20.0f, param ("lim_gain") + step));
}

void MasterProcessor::applyPreset (int index)
{
    struct P { float spec, glue, detail, bass, air, width, color; int mode; float clip, lim; int target; };
    static const P presets[] = {
        { 0.50f, 0.40f, 0.30f, 0.0f, 0.0f, 1.00f, 0.30f, 0, 1.0f, 4.0f, 3 }, // Defaut
        { 0.50f, 0.45f, 0.35f, 1.5f, 1.0f, 1.10f, 0.30f, 0, 2.0f, 6.0f, 3 }, // Rap / Trap
        { 0.60f, 0.35f, 0.40f, 0.5f, 2.0f, 1.15f, 0.20f, 1, 1.0f, 5.0f, 3 }, // Pop / Voix
        { 0.50f, 0.55f, 0.30f, 2.0f, 1.5f, 1.20f, 0.35f, 0, 3.5f, 8.0f, 4 }, // Club / EDM
        { 0.30f, 0.50f, 0.20f, 1.0f, -2.0f, 0.90f, 0.60f, 1, 0.0f, 4.0f, 0 }, // Lo-fi / Chill
        { 0.35f, 0.20f, 0.20f, 0.0f, 0.5f, 1.00f, 0.10f, 1, 0.0f, 3.0f, 0 }, // Doux & naturel
        { 0.40f, 0.50f, 0.25f, 2.5f, 1.0f, 1.10f, 0.40f, 2, 3.0f, 7.0f, 4 }, // Instru 808
    };
    const auto& p = presets[juce::jlimit (0, (int) std::size (presets) - 1, index)];

    undoManager.beginNewTransaction();
    for (auto* id : { "spec_on", "comp_on", "eq_on", "stereo_on", "analog_on", "clip_on", "lim_on", "bass_mono" })
        setParam (id, 1.0f);
    setParam ("spec_amt", p.spec);   setParam ("glue", p.glue);   setParam ("detail", p.detail);
    setParam ("bass", p.bass);       setParam ("air", p.air);     setParam ("width", p.width);
    setParam ("color", p.color);     setParam ("analog_mode", (float) p.mode);
    setParam ("clip_gain", p.clip);  setParam ("lim_gain", p.lim); setParam ("target", (float) p.target);
    setParam ("ceiling", -1.0f);     setParam ("release", 80.0f); setParam ("input", 0.0f); setParam ("output", 0.0f);
}

void MasterProcessor::switchAB()
{
    abSlots[(size_t) currentSlot] = apvts.copyState();
    currentSlot ^= 1;
    if (abSlots[(size_t) currentSlot].isValid())
        apvts.replaceState (abSlots[(size_t) currentSlot].createCopy());
}

void MasterProcessor::copyToOtherSlot()
{
    abSlots[(size_t) (currentSlot ^ 1)] = apvts.copyState();
}

//==============================================================================
juce::AudioProcessorEditor* MasterProcessor::createEditor()
{
    return new MasterEditor (*this);
}

void MasterProcessor::setOversamplingHQ (bool hq)
{
    apvts.state.setProperty ("os_hq", hq, nullptr);
    const int wanted = hq ? 2 : 1;
    if (wanted == osFactorLog2)
        return;
    suspendProcessing (true);
    osFactorLog2 = wanted;
    if (getSampleRate() > 0.0)
        prepareToPlay (getSampleRate(), juce::jmax (1, getBlockSize()));
    suspendProcessing (false);
}

void MasterProcessor::applySpice (float a)
{
    a = juce::jlimit (0.0f, 1.0f, a);
    auto mix = [a] (float lo, float hi) { return lo + (hi - lo) * a; };
    setParam ("spec_amt", mix (0.20f, 0.80f));
    setParam ("glue", mix (0.15f, 0.75f));
    setParam ("detail", mix (0.10f, 0.60f));
    setParam ("bass", mix (0.0f, 2.5f));
    setParam ("air", mix (0.0f, 3.0f));
    setParam ("width", mix (1.0f, 1.3f));
    setParam ("color", mix (0.10f, 0.70f));
    setParam ("clip_gain", mix (0.0f, 4.0f));
    setParam ("lim_gain", mix (1.0f, 10.0f));
}

void MasterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MasterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            setOversamplingHQ ((bool) apvts.state.getProperty ("os_hq", true));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MasterProcessor();
}
