#include "PluginProcessor.h"
#include "TchiskibidiDSP.h"
#include "PluginEditor.h"

using namespace tchk;

//==============================================================================
// SpectralDenoiser
void SpectralDenoiser::prepare()
{
    window.resize ((size_t) size);
    for (int i = 0; i < size; ++i)
        window[(size_t) i] = std::sqrt (0.5f - 0.5f * std::cos (2.0f * kPi * (float) i / (float) size)); // sqrt-Hann
    inRing.assign ((size_t) size, 0.0f);
    outRing.assign ((size_t) size, 0.0f);
    frame.assign ((size_t) size * 2, 0.0f);
    adaptiveNoise.assign ((size_t) numBins, 1.0f);
    smoothGain.assign ((size_t) numBins, 1.0f);
    powerBuf.assign ((size_t) numBins, 0.0f);
    smoothPower.assign ((size_t) numBins, 0.0f);
    reset();
}

void SpectralDenoiser::reset()
{
    std::fill (inRing.begin(), inRing.end(), 0.0f);
    std::fill (outRing.begin(), outRing.end(), 0.0f);
    std::fill (adaptiveNoise.begin(), adaptiveNoise.end(), 1.0f);
    std::fill (smoothGain.begin(), smoothGain.end(), 1.0f);
    std::fill (smoothPower.begin(), smoothPower.end(), 0.0f);
    pos = hopCount = 0;
}

float SpectralDenoiser::process (float x) noexcept
{
    inRing[(size_t) pos] = x;
    const float y = outRing[(size_t) pos];
    outRing[(size_t) pos] = 0.0f;
    if (++pos >= size) pos = 0;
    if (++hopCount >= hop)
    {
        hopCount = 0;
        processFrame();
    }
    return y;
}

void SpectralDenoiser::processFrame() noexcept
{
    constexpr float olaScale = 0.5f; // sqrt-Hann analysis * synthesis at 75% overlap sums to 2
    if (! enabled && spectrumOut == nullptr && ! learning)
    {
        // nothing to analyse or remove: same result as the FFT path with unity gains, without the FFTs
        for (int i = 0; i < size; ++i)
        {
            const float w = window[(size_t) i];
            outRing[(size_t) ((pos + i) % size)] += inRing[(size_t) ((pos + i) % size)] * w * w * olaScale;
        }
        return;
    }

    for (int i = 0; i < size; ++i)
        frame[(size_t) i] = inRing[(size_t) ((pos + i) % size)] * window[(size_t) i];
    std::fill (frame.begin() + size, frame.end(), 0.0f);

    fft.performRealOnlyForwardTransform (frame.data(), true);

    for (int k = 0; k < numBins; ++k)
    {
        const float re = frame[(size_t) (2 * k)], im = frame[(size_t) (2 * k + 1)];
        const float p = re * re + im * im;
        powerBuf[(size_t) k] = p;

        // minimum-statistics style noise floor tracker: falls fast, rises slowly
        auto& ps = smoothPower[(size_t) k];
        ps = 0.7f * ps + 0.3f * p; // smoothed power: less biased minimum tracking
        auto& nz = adaptiveNoise[(size_t) k];
        nz = ps < nz ? nz * 0.8f + ps * 0.2f : nz * 1.003f;
        nz = juce::jmax (nz, 1.0e-12f);
    }

    if (spectrumOut != nullptr)
        std::copy (powerBuf.begin(), powerBuf.end(), spectrumOut->begin());

    if (learning && learnAccum != nullptr && learnFrames != nullptr)
    {
        for (int k = 0; k < numBins; ++k)
            (*learnAccum)[(size_t) k] += powerBuf[(size_t) k];
        ++(*learnFrames);
    }

    const bool useProfile = noiseProfile != nullptr && ! adaptive;
    std::array<float, numBins> gains;
    for (int k = 0; k < numBins; ++k)
    {
        if (! enabled) { gains[(size_t) k] = 1.0f; continue; }
        const float nz = useProfile ? (*noiseProfile)[(size_t) k] : adaptiveNoise[(size_t) k] * kAdaptiveBias;
        const float p = juce::jmax (powerBuf[(size_t) k], 1.0e-12f);
        gains[(size_t) k] = juce::jlimit (floorGain, 1.0f, 1.0f - sensitivity * nz / p);
    }

    for (int k = 0; k < numBins; ++k)
    {
        // smoothing across neighbouring bins and over time: avoids "musical noise" artefacts
        const float g = (gains[(size_t) juce::jmax (0, k - 1)] + 2.0f * gains[(size_t) k] + gains[(size_t) juce::jmin (numBins - 1, k + 1)]) * 0.25f;
        auto& s = smoothGain[(size_t) k];
        s = g > s ? s + (g - s) * 0.7f : s + (g - s) * 0.35f;
        frame[(size_t) (2 * k)] *= s;
        frame[(size_t) (2 * k + 1)] *= s;
    }

    fft.performRealOnlyInverseTransform (frame.data());

    for (int i = 0; i < size; ++i)
        outRing[(size_t) ((pos + i) % size)] += frame[(size_t) i] * window[(size_t) i] * olaScale;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout CleanProcessor::createParameterLayout()
{
    ParamList p;
    p.add ("lowcut", "Low Cut", 20.0f, 300.0f, 20.0f, Unit::hz, 0.5f);

    p.addBool ("hum_on", "De-hum On", false);
    p.addChoice ("hum_freq", "Hum Frequency", { "50 Hz", "60 Hz" }, 0);
    p.add ("hum_amt", "De-hum Amount", 0.0f, 1.0f, 1.0f, Unit::percent);
    p.addChoice ("hum_harm", "Harmonics", { "1", "2", "3", "4", "5", "6", "7", "8" }, 3);

    p.addBool ("click_on", "De-click On", false);
    p.add ("click_sens", "De-click Sensitivity", 0.0f, 1.0f, 0.5f, Unit::percent);

    p.addBool ("dn_on", "De-noise On", true);
    p.add ("dn_amount", "Reduction", 0.0f, 30.0f, 12.0f, Unit::dB);
    p.add ("dn_sens", "Sensitivity", 0.0f, 1.0f, 0.4f, Unit::percent);
    p.addBool ("learn", "Learn Noise", false);
    p.addBool ("adaptive", "Adaptive", true);

    p.addBool ("listen", "Listen Removed", false);
    p.add ("output", "Output", -12.0f, 12.0f, 0.0f, Unit::dB);
    return p.build();
}

CleanProcessor::CleanProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    noiseProfile.assign ((size_t) SpectralDenoiser::numBins, 0.0f);
    learnAccum.assign ((size_t) SpectralDenoiser::numBins, 0.0f);
    spectrum.assign ((size_t) SpectralDenoiser::numBins, 0.0f);
    for (auto& v : displayInput) v = -120.0f;
    for (auto& v : displayNoise) v = -120.0f;
    startTimerHz (10);
}

bool CleanProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
        && layouts.getMainInputChannelSet() == out;
}

void CleanProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlockSize = juce::jmax (1, samplesPerBlock);
    lowCut1.reset(); lowCut2.reset(); lastLowCut = -1.0f;
    for (auto& h : humNotches) h.reset();
    lastHumFreq = -1.0f; lastHarmonics = -1;

    for (auto& b : clickBuf) b.fill (0.0f);
    clickFlag.fill (false);
    clickPos = 0; prev1.fill (0); prev2.fill (0); lastOut.fill (0);
    errEnv = 1.0e-6f; kErr = onePoleCoef (sr, 20.0);
    flagUntil = -1; repair = {}; sampleCounter = 0;

    for (auto& d : denoisers) d.prepare();
    const int latency = clickDelay + SpectralDenoiser::size;
    setLatencySamples (latency);
    dryDelay.prepare (latency + 8);
    dryBuffer.setSize (2, maxBlockSize);
    outGain.reset (sr, 0.05);
    outGain.setCurrentAndTargetValue (dbToGain (param ("output")));
}

void CleanProcessor::updateHumFilters (float baseFreq, int harmonics)
{
    for (int k = 0; k < maxHarmonics; ++k)
    {
        const double f = baseFreq * (k + 1);
        humNotches[(size_t) k].set (BiquadCoeffs::make (FilterType::notch, sr, juce::jmin (f, sr * 0.45), 25.0));
    }
    lastHumFreq = baseFreq;
    lastHarmonics = harmonics;
}

void CleanProcessor::clearProfile()
{
    const juce::SpinLock::ScopedLockType lock (profileLock);
    profileReady = false;
    std::fill (noiseProfile.begin(), noiseProfile.end(), 0.0f);
}

void CleanProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const int nCh = juce::jmin (2, buffer.getNumChannels());
    float in = 0.0f, out = 0.0f;
    for (int ch = 0; ch < nCh; ++ch) in = juce::jmax (in, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
    processBlockInternal (buffer, midiMessages);
    for (int ch = 0; ch < nCh; ++ch) out = juce::jmax (out, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
    inputPeakDb = in > 1.0e-5f ? 20.0f * std::log10 (in) : -100.0f;
    outputPeakDb = out > 1.0e-5f ? 20.0f * std::log10 (out) : -100.0f;
    tchk::sanitize (buffer);
}

void CleanProcessor::processBlockInternal (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n > maxBlockSize)
    {
        for (int start = 0; start < n; start += maxBlockSize)
        {
            juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, juce::jmin (maxBlockSize, n - start));
            processBlockInternal (sub, midi);
        }
        return;
    }
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    const int numCh = juce::jmin (2, getTotalNumOutputChannels(), buffer.getNumChannels());
    if (numCh <= 0 || n <= 0)
        return;

    for (int ch = 0; ch < numCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

    // ---- settings ----
    const float lc = param ("lowcut");
    if (lc != lastLowCut)
    {
        const auto c = BiquadCoeffs::make (FilterType::highPass, sr, lc, 0.7071);
        lowCut1.set (c); lowCut2.set (c); lastLowCut = lc;
    }
    const bool lowCutOn = lc > 21.0f;

    const bool humOn = param ("hum_on") > 0.5f;
    const float humAmt = param ("hum_amt");
    const float humFreq = param ("hum_freq") > 0.5f ? 60.0f : 50.0f;
    const int harmonics = juce::jlimit (1, maxHarmonics, (int) param ("hum_harm") + 1);
    if (humFreq != lastHumFreq || harmonics != lastHarmonics)
        updateHumFilters (humFreq, harmonics);

    const bool clickOn = param ("click_on") > 0.5f;
    const float clickThreshold = 10.0f - 7.0f * param ("click_sens");

    const bool learning = param ("learn") > 0.5f;
    if (! learning)
        learnState = idle;
    else if (learnState == idle)
    {
        std::fill (learnAccum.begin(), learnAccum.end(), 0.0f);
        learnFrames = 0;
        learnState = learningNow;
    }
    const bool profileUsable = profileReady.load();
    const juce::SpinLock::ScopedTryLockType tryLock (profileLock);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto& d = denoisers[(size_t) ch];
        d.enabled = param ("dn_on") > 0.5f;
        d.floorGain = dbToGain (-param ("dn_amount"));
        d.sensitivity = 1.0f + 3.0f * param ("dn_sens");
        d.adaptive = param ("adaptive") > 0.5f || ! profileUsable;
        d.noiseProfile = (profileUsable && tryLock.isLocked()) ? &noiseProfile : nullptr;
        if (d.noiseProfile == nullptr) d.adaptive = true;
        d.learning = learnState == learningNow && ch == 0;
        d.learnAccum = ch == 0 ? &learnAccum : nullptr;
        d.learnFrames = ch == 0 ? &learnFrames : nullptr;
        d.spectrumOut = (ch == 0 && displayWanted.load (std::memory_order_relaxed)) ? &spectrum : nullptr;
    }
    outGain.setTargetValue (dbToGain (param ("output")));
    const bool listen = param ("listen") > 0.5f;

    // ---- per-sample chain ----
    for (int i = 0; i < n; ++i)
    {
        float x[2] = { buffer.getSample (0, i), numCh > 1 ? buffer.getSample (1, i) : 0.0f };

        for (int ch = 0; ch < numCh; ++ch)
        {
            if (lowCutOn) x[ch] = lowCut2.process (ch, lowCut1.process (ch, x[ch]));
            if (humOn)
            {
                float notched = x[ch];
                for (int k = 0; k < harmonics; ++k)
                    notched = humNotches[(size_t) k].process (ch, notched);
                x[ch] += humAmt * (notched - x[ch]);
            }
        }

        // ---- de-click: detect spikes in the 2nd-order prediction error, repair by interpolation ----
        float emax = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float e = x[ch] - 2.0f * prev1[(size_t) ch] + prev2[(size_t) ch];
            prev2[(size_t) ch] = prev1[(size_t) ch];
            prev1[(size_t) ch] = x[ch];
            emax = juce::jmax (emax, std::abs (e));
        }
        const bool isClick = clickOn && emax > 0.01f && emax > clickThreshold * std::sqrt (errEnv);
        if (isClick)
        {
            flagUntil = (int) (sampleCounter + 8);
            for (int back = 1; back <= 3; ++back)
                clickFlag[(size_t) ((clickPos - back + clickRing) % clickRing)] = true;
        }
        else
            errEnv = emax * emax + kErr * (errEnv - emax * emax);

        for (int ch = 0; ch < numCh; ++ch)
            clickBuf[(size_t) ch][(size_t) clickPos] = x[ch];
        clickFlag[(size_t) clickPos] = sampleCounter <= flagUntil;

        const int readIdx = (clickPos - clickDelay + clickRing) % clickRing;
        if (repair.remaining == 0 && clickFlag[(size_t) readIdx])
        {
            int len = 0;
            while (len < clickDelay - 4 && clickFlag[(size_t) ((readIdx + len) % clickRing)]) ++len;
            const int endIdx = (readIdx + len) % clickRing;
            repair.length = repair.remaining = len;
            for (int ch = 0; ch < 2; ++ch)
            {
                repair.start[ch] = lastOut[(size_t) ch];
                repair.end[ch] = clickBuf[(size_t) ch][(size_t) endIdx];
            }
            clicksRepaired.fetch_add (1);
        }
        clickFlag[(size_t) ((readIdx - 1 + clickRing) % clickRing)] = false;

        for (int ch = 0; ch < numCh; ++ch)
        {
            float y = clickBuf[(size_t) ch][(size_t) readIdx];
            if (repair.remaining > 0)
            {
                const float t = (float) (repair.length - repair.remaining + 1) / (float) (repair.length + 1);
                y = repair.start[ch] + (repair.end[ch] - repair.start[ch]) * t;
            }
            lastOut[(size_t) ch] = y;
            x[ch] = denoisers[(size_t) ch].process (y);
        }
        if (repair.remaining > 0) --repair.remaining;
        if (++clickPos >= clickRing) clickPos = 0;
        ++sampleCounter;

        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, i, x[ch]);
    }

    // ---- finish learning after ~3 seconds ----
    if (learnState == learningNow)
    {
        const int needed = (int) (3.0 * sr / SpectralDenoiser::hop);
        learnProgress = juce::jlimit (0.0f, 1.0f, (float) learnFrames / (float) needed);
        if (learnFrames >= needed && tryLock.isLocked())
        {
            for (size_t k = 0; k < noiseProfile.size(); ++k)
                noiseProfile[k] = learnAccum[k] / (float) learnFrames;
            profileReady = true;
            learnState = done; // the timer switches the Learn button off
        }
    }
    else if (learnState == idle)
        learnProgress = 0.0f;

    // ---- output: gain, or "listen to what is removed" ----
    const float latency = (float) (clickDelay + SpectralDenoiser::size);
    for (int i = 0; i < n; ++i)
    {
        const float og = outGain.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            dryDelay.write (ch, dryBuffer.getSample (ch, i));
            const float dry = dryDelay.read (ch, latency);
            const float wet = buffer.getSample (ch, i);
            buffer.setSample (ch, i, (listen ? dry - wet : wet) * og);
        }
        dryDelay.advance();
    }

    // ---- display data ----
    const double binHz = sr / SpectralDenoiser::size;
    const float norm = 20.0f * std::log10 ((float) SpectralDenoiser::size / 4.0f);
    const bool showProfile = profileUsable && tryLock.isLocked() && param ("adaptive") < 0.5f;
    const auto& noiseRef = showProfile ? noiseProfile : denoisers[0].getAdaptiveNoise();
    const float noiseScale = showProfile ? 1.0f : SpectralDenoiser::kAdaptiveBias;
    for (int b = 0; b < displayBins; ++b)
    {
        const double f0 = 30.0 * std::pow (20000.0 / 30.0, (double) b / displayBins);
        const double f1 = 30.0 * std::pow (20000.0 / 30.0, (double) (b + 1) / displayBins);
        const int k0 = juce::jlimit (1, SpectralDenoiser::numBins - 1, (int) (f0 / binHz));
        const int k1 = juce::jlimit (k0, SpectralDenoiser::numBins - 1, (int) (f1 / binHz));
        float pIn = 0.0f, pNoise = 0.0f;
        for (int k = k0; k <= k1; ++k)
        {
            pIn = juce::jmax (pIn, spectrum[(size_t) k]);
            pNoise = juce::jmax (pNoise, noiseRef[(size_t) k] * noiseScale);
        }
        displayInput[(size_t) b] = powerToDb (pIn) - norm;
        displayNoise[(size_t) b] = pNoise > 0.0f ? powerToDb (pNoise) - norm : -120.0f;
    }
}

void CleanProcessor::timerCallback()
{
    if (learnState == done && param ("learn") > 0.5f)
    {
        tchk::setParam (apvts, "learn", 0.0f);
        tchk::setParam (apvts, "adaptive", 0.0f); // use the freshly learned profile
    }
}

//==============================================================================
void CleanProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (profileReady.load())
    {
        const juce::SpinLock::ScopedLockType lock (profileLock);
        juce::MemoryBlock mb (noiseProfile.data(), noiseProfile.size() * sizeof (float));
        state.setProperty ("noiseProfile", mb.toBase64Encoding(), nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void CleanProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (! xml->hasTagName (apvts.state.getType()))
            return;
        auto state = juce::ValueTree::fromXml (*xml);
        juce::MemoryBlock mb;
        if (state.hasProperty ("noiseProfile") && mb.fromBase64Encoding (state["noiseProfile"].toString())
            && mb.getSize() == noiseProfile.size() * sizeof (float))
        {
            const juce::SpinLock::ScopedLockType lock (profileLock);
            std::memcpy (noiseProfile.data(), mb.getData(), mb.getSize());
            profileReady = true;
        }
        state.removeProperty ("noiseProfile", nullptr);
        apvts.replaceState (state);
    }
}

juce::AudioProcessorEditor* CleanProcessor::createEditor() { return new CleanEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CleanProcessor(); }
