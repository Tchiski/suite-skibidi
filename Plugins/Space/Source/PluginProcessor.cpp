#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float kMaxDelayMs = 4000.0f;
    constexpr float kMaxWobbleMs = 4.0f;
    constexpr float kWobbleRateHz = 0.55f;

    // Length of each tempo division, in quarter notes (beats).
    constexpr double kDivisionBeats[] = { 4.0, 2.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25 };

    inline float softClip (float x) noexcept { return std::tanh (x); }
}

juce::StringArray TchiskibidiSpaceProcessor::getDivisionNames()
{
    return { "1/1", "1/2", "1/4", "1/4 D", "1/4 T", "1/8", "1/8 D", "1/8 T", "1/16" };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TchiskibidiSpaceProcessor::createParameterLayout()
{
    using namespace juce;

    const auto percent = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });

    const auto hertz = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz"
                                                : String (roundToInt (v)) + " Hz"; });

    const auto millis = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s"
                                                : String (roundToInt (v)) + " ms"; });

    const auto decibels = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (v, 1) + " dB"; });

    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // --- Echo ---
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "time", 1 }, "Echo Time",
        NormalisableRange<float> (1.0f, 2000.0f, 0.0f, 0.35f), 375.0f, millis));
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Tempo Sync", false));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Division",
        getDivisionNames(), 6));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "feedback", 1 }, "Feedback",
        NormalisableRange<float> (0.0f, 0.95f), 0.45f, percent));
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { "pingpong", 1 }, "Ping-Pong", false));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lowcut", 1 }, "Low Cut",
        NormalisableRange<float> (20.0f, 2000.0f, 0.0f, 0.3f), 120.0f, hertz));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "highcut", 1 }, "High Cut",
        NormalisableRange<float> (1000.0f, 20000.0f, 0.0f, 0.3f), 7000.0f, hertz));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "wobble", 1 }, "Wobble",
        NormalisableRange<float> (0.0f, 1.0f), 0.15f, percent));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "echolevel", 1 }, "Echo Level",
        NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent));

    // --- Reverb ---
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "size", 1 }, "Reverb Size",
        NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "damping", 1 }, "Reverb Damping",
        NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Reverb Width",
        NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "reverblevel", 1 }, "Reverb Level",
        NormalisableRange<float> (0.0f, 1.0f), 0.35f, percent));

    // --- Master ---
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f), 0.35f, percent));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f), 0.0f, decibels));

    return { params.begin(), params.end() };
}

//==============================================================================
TchiskibidiSpaceProcessor::TchiskibidiSpaceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    timeParam        = apvts.getRawParameterValue ("time");
    syncParam        = apvts.getRawParameterValue ("sync");
    divisionParam    = apvts.getRawParameterValue ("division");
    feedbackParam    = apvts.getRawParameterValue ("feedback");
    pingPongParam    = apvts.getRawParameterValue ("pingpong");
    lowCutParam      = apvts.getRawParameterValue ("lowcut");
    highCutParam     = apvts.getRawParameterValue ("highcut");
    wobbleParam      = apvts.getRawParameterValue ("wobble");
    echoLevelParam   = apvts.getRawParameterValue ("echolevel");
    sizeParam        = apvts.getRawParameterValue ("size");
    dampingParam     = apvts.getRawParameterValue ("damping");
    widthParam       = apvts.getRawParameterValue ("width");
    reverbLevelParam = apvts.getRawParameterValue ("reverblevel");
    mixParam         = apvts.getRawParameterValue ("mix");
    outputParam      = apvts.getRawParameterValue ("output");
}

bool TchiskibidiSpaceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
double TchiskibidiSpaceProcessor::getHostBpm() const
{
    if (auto* hostPlayHead = getPlayHead())
        if (auto position = hostPlayHead->getPosition())
            if (auto bpm = position->getBpm())
                if (*bpm > 1.0)
                    return *bpm;

    return 120.0;
}

float TchiskibidiSpaceProcessor::computeDelayMs() const
{
    if (syncParam->load() > 0.5f)
    {
        constexpr int numDivisions = (int) (sizeof (kDivisionBeats) / sizeof (kDivisionBeats[0]));
        const int index = juce::jlimit (0, numDivisions - 1, (int) divisionParam->load());
        const double ms = 60000.0 / getHostBpm() * kDivisionBeats[index];
        return juce::jlimit (1.0f, kMaxDelayMs, (float) ms);
    }

    return juce::jlimit (1.0f, kMaxDelayMs, timeParam->load());
}

//==============================================================================
void TchiskibidiSpaceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const double maxSeconds = (kMaxDelayMs + kMaxWobbleMs) / 1000.0;
    delayLineSize = (int) std::ceil (maxSeconds * sampleRate) + 8;
    maxDelaySamples = (float) (delayLineSize - 4);

    for (auto& line : delayLines)
        line.assign ((size_t) delayLineSize, 0.0f);

    writePosition = 0;
    lowpassState.fill (0.0f);
    highpassState.fill (0.0f);
    lfoPhase = 0.0f;

    const auto init = [sampleRate] (juce::SmoothedValue<float>& s, double rampSeconds, float value)
    {
        s.reset (sampleRate, rampSeconds);
        s.setCurrentAndTargetValue (value);
    };

    init (delaySamplesSmoothed, 0.25, computeDelayMs() * 0.001f * (float) sampleRate);
    init (feedbackSmoothed, 0.05, feedbackParam->load());
    init (echoLevelSmoothed, 0.05, echoLevelParam->load());
    init (reverbLevelSmoothed, 0.05, reverbLevelParam->load());
    init (mixSmoothed, 0.05, mixParam->load());
    init (outputGainSmoothed, 0.05, juce::Decibels::decibelsToGain (outputParam->load()));

    const auto numChannels = (juce::uint32) juce::jlimit (1, 2, getTotalNumOutputChannels());
    reverb.prepare ({ sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), numChannels });
    reverb.reset();

    echoBuffer.setSize (2, juce::jmax (1, samplesPerBlock));
    reverbBuffer.setSize (2, juce::jmax (1, samplesPerBlock));
}

float TchiskibidiSpaceProcessor::readDelay (int channel, float delayInSamples) const noexcept
{
    const auto& line = delayLines[(size_t) channel];

    float readPos = (float) writePosition - delayInSamples;
    while (readPos < 0.0f)
        readPos += (float) delayLineSize;

    const int i1 = (int) readPos % delayLineSize;
    const float frac = readPos - std::floor (readPos);
    const int i0 = (i1 - 1 + delayLineSize) % delayLineSize;
    const int i2 = (i1 + 1) % delayLineSize;
    const int i3 = (i1 + 2) % delayLineSize;

    const float y0 = line[(size_t) i0], y1 = line[(size_t) i1], y2 = line[(size_t) i2], y3 = line[(size_t) i3];

    // 4-point Hermite interpolation: smooth when the delay time moves (tape-like pitch bend)
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + y1;
}

void TchiskibidiSpaceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const int nCh = juce::jmin (2, buffer.getNumChannels());
    float in = 0.0f, out = 0.0f;
    for (int ch = 0; ch < nCh; ++ch) in = juce::jmax (in, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
    processBlockInternal (buffer, midiMessages);
    for (int ch = 0; ch < nCh; ++ch) out = juce::jmax (out, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
    inputPeakDb = in > 1.0e-5f ? 20.0f * std::log10 (in) : -100.0f;
    outputPeakDb = out > 1.0e-5f ? 20.0f * std::log10 (out) : -100.0f;
}

void TchiskibidiSpaceProcessor::processBlockInternal (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalIn = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int numChannels = juce::jmin (2, totalOut, buffer.getNumChannels());
    if (numChannels <= 0 || numSamples <= 0 || delayLineSize == 0)
        return;

    if (echoBuffer.getNumSamples() < numSamples)
    {
        echoBuffer.setSize (2, numSamples, false, false, true);
        reverbBuffer.setSize (2, numSamples, false, false, true);
    }

    // --- Read parameters once per block ---
    const float sr = (float) currentSampleRate;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    constexpr float halfPi = juce::MathConstants<float>::halfPi;

    delaySamplesSmoothed.setTargetValue (computeDelayMs() * 0.001f * sr);
    feedbackSmoothed.setTargetValue (feedbackParam->load());
    echoLevelSmoothed.setTargetValue (echoLevelParam->load());
    reverbLevelSmoothed.setTargetValue (reverbLevelParam->load());
    mixSmoothed.setTargetValue (mixParam->load());
    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    const bool pingPong = pingPongParam->load() > 0.5f && numChannels == 2;
    const float highCut = juce::jmin (highCutParam->load(), sr * 0.45f);
    const float lowCut = juce::jmin (lowCutParam->load(), highCut * 0.9f);
    const float lpCoef = std::exp (-twoPi * highCut / sr);
    const float hpCoef = std::exp (-twoPi * lowCut / sr);
    const float lfoIncrement = twoPi * kWobbleRateHz / sr;
    const float wobbleSamples = wobbleParam->load() * kMaxWobbleMs * 0.001f * sr;

    // --- Echo: delay line with filtered, saturated feedback ---
    for (int i = 0; i < numSamples; ++i)
    {
        const float baseDelay = delaySamplesSmoothed.getNextValue();
        const float feedback = feedbackSmoothed.getNextValue();

        lfoPhase += lfoIncrement;
        if (lfoPhase >= twoPi)
            lfoPhase -= twoPi;

        float echo[2] = { 0.0f, 0.0f };

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float phase = lfoPhase + (ch == 1 ? halfPi : 0.0f);
            const float modulation = wobbleSamples * 0.5f * (1.0f + std::sin (phase));
            const float delayed = readDelay (ch, juce::jlimit (2.0f, maxDelaySamples, baseDelay + modulation));

            // Band-pass (low cut + high cut) so repeats get darker and thinner, like tape
            lowpassState[(size_t) ch] = (1.0f - lpCoef) * delayed + lpCoef * lowpassState[(size_t) ch];
            highpassState[(size_t) ch] = (1.0f - hpCoef) * lowpassState[(size_t) ch] + hpCoef * highpassState[(size_t) ch];
            echo[ch] = lowpassState[(size_t) ch] - highpassState[(size_t) ch];

            echoBuffer.setSample (ch, i, echo[ch]);
        }

        if (pingPong)
        {
            const float monoIn = 0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i));
            delayLines[0][(size_t) writePosition] = monoIn + softClip (feedback * echo[1]);
            delayLines[1][(size_t) writePosition] = softClip (feedback * echo[0]);
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
                delayLines[(size_t) ch][(size_t) writePosition] = buffer.getSample (ch, i) + softClip (feedback * echo[ch]);
        }

        if (++writePosition >= delayLineSize)
            writePosition = 0;
    }

    // --- Reverb: fed with the dry signal + the echoes ---
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = sizeParam->load();
    reverbParams.damping = dampingParam->load();
    reverbParams.width = widthParam->load();
    reverbParams.wetLevel = 1.0f;
    reverbParams.dryLevel = 0.0f;
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters (reverbParams);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        reverbBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        reverbBuffer.addFrom (ch, 0, echoBuffer, ch, 0, numSamples);
    }

    auto reverbBlock = juce::dsp::AudioBlock<float> (reverbBuffer)
                           .getSubsetChannelBlock (0, (size_t) numChannels)
                           .getSubBlock (0, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> reverbContext (reverbBlock);
    reverb.process (reverbContext);

    // --- Final mix ---
    for (int i = 0; i < numSamples; ++i)
    {
        const float echoLevel = echoLevelSmoothed.getNextValue();
        const float reverbLevel = reverbLevelSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float gain = outputGainSmoothed.getNextValue();

        // Dry stays at full level until 50 %, wet reaches full level at 50 %
        const float dryGain = juce::jmin (1.0f, 2.0f * (1.0f - mix));
        const float wetGain = juce::jmin (1.0f, 2.0f * mix);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dry = buffer.getSample (ch, i);
            const float wet = echoLevel * echoBuffer.getSample (ch, i)
                            + reverbLevel * reverbBuffer.getSample (ch, i);
            buffer.setSample (ch, i, (dry * dryGain + wet * wetGain) * gain);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* TchiskibidiSpaceProcessor::createEditor()
{
    return new TchiskibidiSpaceEditor (*this);
}

void TchiskibidiSpaceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TchiskibidiSpaceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TchiskibidiSpaceProcessor();
}
