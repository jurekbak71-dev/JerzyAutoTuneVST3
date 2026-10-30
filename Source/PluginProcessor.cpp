#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const std::array<const char*, 12> noteParameterIds {
    "noteC", "noteCs", "noteD", "noteDs", "noteE", "noteF",
    "noteFs", "noteG", "noteGs", "noteA", "noteAs", "noteB"
};
const juce::StringArray scaleNames { "Chromatic", "Major", "Minor" };

enum FilterType { lowShelf = 0, peak = 1, highShelf = 2, highPass = 3, lowPass = 4 };

std::array<float, 5> makeBiquad(double sampleRate, float frequency, float gainDb, float q, int type)
{
    const float fs = static_cast<float>(sampleRate);
    const float f = juce::jlimit(10.0f, fs * 0.45f, frequency);
    const float omega = juce::MathConstants<float>::twoPi * f / fs;
    const float c = std::cos(omega);
    const float s = std::sin(omega);
    const float Q = juce::jmax(0.15f, q);
    const float alpha = s / (2.0f * Q);
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float beta = 2.0f * std::sqrt(A) * alpha;

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;

    switch (type)
    {
        case lowShelf:
            b0 = A * ((A + 1.0f) - (A - 1.0f) * c + beta);
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * c);
            b2 = A * ((A + 1.0f) - (A - 1.0f) * c - beta);
            a0 = (A + 1.0f) + (A - 1.0f) * c + beta;
            a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * c);
            a2 = (A + 1.0f) + (A - 1.0f) * c - beta;
            break;
        case peak:
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * c;
            b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A;
            a1 = -2.0f * c;
            a2 = 1.0f - alpha / A;
            break;
        case highShelf:
            b0 = A * ((A + 1.0f) + (A - 1.0f) * c + beta);
            b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * c);
            b2 = A * ((A + 1.0f) + (A - 1.0f) * c - beta);
            a0 = (A + 1.0f) - (A - 1.0f) * c + beta;
            a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * c);
            a2 = (A + 1.0f) - (A - 1.0f) * c - beta;
            break;
        case highPass:
            b0 = (1.0f + c) * 0.5f;
            b1 = -(1.0f + c);
            b2 = (1.0f + c) * 0.5f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * c;
            a2 = 1.0f - alpha;
            break;
        case lowPass:
            b0 = (1.0f - c) * 0.5f;
            b1 = 1.0f - c;
            b2 = (1.0f - c) * 0.5f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * c;
            a2 = 1.0f - alpha;
            break;
        default: break;
    }

    const float inv = 1.0f / a0;
    return { b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv };
}

float coeffFromMs(float ms, double sampleRate)
{
    return std::exp(-1.0f / (juce::jmax(0.05f, ms) * 0.001f * static_cast<float>(sampleRate)));
}
}

JerzyAutoTuneAudioProcessor::JerzyAutoTuneAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    const char* suffix[]={"Interval","Pan","Formant","Enabled","Level"};
    for(int v=0;v<4;++v)for(int k=0;k<5;++k)
        harmonyParameters[size_t(v)][size_t(k)]=parameters.getRawParameterValue("harmony"+juce::String(v+1)+suffix[k]);
}

JerzyAutoTuneAudioProcessor::APVTS::ParameterLayout JerzyAutoTuneAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterChoice>("key", "Key", noteNames, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("scale", "Scale", scaleNames, 1));
    for (int i = 0; i < noteNames.size(); ++i)
        layout.add(std::make_unique<juce::AudioParameterBool>(noteParameterIds[static_cast<size_t>(i)], noteNames[i], true));

    layout.add(std::make_unique<juce::AudioParameterFloat>("speed", "Speed", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 20.0f, "ms"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("amount", "Amount", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterBool>("gateEnabled", "Gate", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("gateThreshold", "Gate Threshold", juce::NormalisableRange<float>(-70.0f, -10.0f, 0.1f), -45.0f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("gateRange", "Gate Range", juce::NormalisableRange<float>(0.0f, 80.0f, 0.1f), 40.0f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("gateAttack", "Gate Attack", juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.4f), 5.0f, "ms"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("gateHold", "Gate Hold", juce::NormalisableRange<float>(0.0f, 250.0f, 1.0f), 40.0f, "ms"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("gateRelease", "Gate Release", juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f, 0.45f), 140.0f, "ms"));

    layout.add(std::make_unique<juce::AudioParameterBool>("noiseEnabled", "Noise Filter", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("noiseThreshold", "Noise Threshold", juce::NormalisableRange<float>(-80.0f, -25.0f, 0.1f), -58.0f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("noiseReduction", "Noise Reduction", juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 12.0f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("noiseRelease", "Noise Release", juce::NormalisableRange<float>(20.0f, 1000.0f, 1.0f, 0.5f), 180.0f, "ms"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("noiseHighPass", "Noise High Pass", juce::NormalisableRange<float>(20.0f, 300.0f, 1.0f, 0.5f), 70.0f, "Hz"));

    layout.add(std::make_unique<juce::AudioParameterBool>("deEsserEnabled", "De-Esser", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("deEsserThreshold", "De-Esser Threshold", juce::NormalisableRange<float>(-50.0f, 0.0f, 0.1f), -24.0f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("deEsserFreq", "De-Esser Frequency", juce::NormalisableRange<float>(3000.0f, 12000.0f, 10.0f, 0.5f), 6500.0f, "Hz"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("deEsserAmount", "De-Esser Range", juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 7.0f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("deEsserRelease", "De-Esser Release", juce::NormalisableRange<float>(10.0f, 300.0f, 1.0f, 0.5f), 70.0f, "ms"));

    layout.add(std::make_unique<juce::AudioParameterBool>("satEnabled", "Saturation", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("satDrive", "Saturation Drive", juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 4.0f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("satTone", "Saturation Tone", juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("satMix", "Saturation Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 30.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("satOutput", "Saturation Output", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f, "dB"));

    layout.add(std::make_unique<juce::AudioParameterBool>("doublerEnabled", "Doubler", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("doublerAmount", "Doubler Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 25.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("doublerDelay", "Doubler Delay", juce::NormalisableRange<float>(5.0f, 40.0f, 0.1f), 18.0f, "ms"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("doublerDetune", "Doubler Detune", juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 10.0f, "ct"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("doublerWidth", "Doubler Width", juce::NormalisableRange<float>(0.0f, 200.0f, 0.1f), 120.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterBool>("compEnabled", "Compressor Limiter", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("compThreshold", "Comp Threshold", juce::NormalisableRange<float>(-48.0f, 0.0f, 0.1f), -18.0f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("compRatio", "Comp Ratio", juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 3.0f, ":1"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("compAttack", "Comp Attack", juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.45f), 10.0f, "ms"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("compRelease", "Comp Release", juce::NormalisableRange<float>(20.0f, 1000.0f, 1.0f, 0.5f), 120.0f, "ms"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("compKnee", "Comp Knee", juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 6.0f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("compMakeup", "Comp Makeup", juce::NormalisableRange<float>(-6.0f, 18.0f, 0.1f), 1.5f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("limiterCeiling", "Limiter Ceiling", juce::NormalisableRange<float>(-6.0f, -0.1f, 0.1f), -1.0f, "dB"));

    layout.add(std::make_unique<juce::AudioParameterBool>("eqEnabled", "Vocal EQ", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eqLowCut", "EQ Low Cut", juce::NormalisableRange<float>(20.0f, 300.0f, 1.0f, 0.5f), 70.0f, "Hz"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eqLow", "EQ Body", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), -1.0f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eqMidFreq", "EQ Mid Frequency", juce::NormalisableRange<float>(250.0f, 6000.0f, 1.0f, 0.45f), 2200.0f, "Hz"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eqMid", "EQ Presence", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 1.5f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eqMidQ", "EQ Mid Q", juce::NormalisableRange<float>(0.3f, 6.0f, 0.01f, 0.5f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eqHigh", "EQ Air", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 1.0f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eqHighCut", "EQ High Cut", juce::NormalisableRange<float>(6000.0f, 22000.0f, 10.0f, 0.5f), 19000.0f, "Hz"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("outputGain", "Output", juce::NormalisableRange<float>(-18.0f, 12.0f, 0.1f), 0.0f, "dB"));
    // Append parameters: never change the original host parameter order or IDs.
    layout.add(std::make_unique<juce::AudioParameterChoice>("engineMode", "Pitch Engine", juce::StringArray { "Legacy 1.5", "Advanced" }, 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>("vibratoPreserve", "Vibrato Preserve", juce::NormalisableRange<float>(0, 100, .1f), 50, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("noteStability", "Note Stability", juce::NormalisableRange<float>(0, 100, .1f), 40, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("referenceHz", "Reference A4", juce::NormalisableRange<float>(400, 480, .1f), 440, "Hz"));
    layout.add(std::make_unique<juce::AudioParameterBool>("formantEnabled", "Preserve Formants", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("formantShift", "Formant Shift", juce::NormalisableRange<float>(-12, 12, .01f), 0, "st"));
    layout.add(std::make_unique<juce::AudioParameterFloat>("transpose", "Transpose", juce::NormalisableRange<float>(-12, 12, .01f), 0, "st"));
    layout.add(std::make_unique<juce::AudioParameterBool>("limiterEnabled", "Final Limiter", true));
    jerzy::studio::addParameters(layout);
    return layout;
}

float JerzyAutoTuneAudioProcessor::Biquad::process(float input, float smoothing) noexcept
{
    for (size_t i = 0; i < coefficients.size(); ++i)
        coefficients[i] += (targetCoefficients[i] - coefficients[i]) * smoothing;
    const float output = coefficients[0] * input + coefficients[1] * x1 + coefficients[2] * x2
                       - coefficients[3] * y1 - coefficients[4] * y2;
    x2 = x1; x1 = input; y2 = y1; y1 = output;
    return output;
}

void JerzyAutoTuneAudioProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    currentSampleRate = sampleRate;
    graphRate.store(float(sampleRate));
    spectralPrep->prepare(sampleRate); space.prepare(sampleRate); choir.prepare(sampleRate);
    stageA.prepare(sampleRate); stageB.prepare(sampleRate);
    keyDetector.reset(); suggestedKey.store(-1); suggestedConfidence.store(0);
    heldMidi.fill(false); sustainedMidi.fill(false); sustainPedal=false;sampleClock=0;
    hardLow={};hardHigh={};hardEnvelope={};satPrevious={};satDc={};
    stageBFade.reset(sampleRate,.010);stageBFade.setCurrentAndTargetValue(parameters.getRawParameterValue("comp2Enabled")->load());
    masterFade.reset(sampleRate,.010);masterFade.setCurrentAndTargetValue(parameters.getRawParameterValue("masterEnabled")->load());
    harmonyFade.reset(sampleRate,.020);harmonyFade.setCurrentAndTargetValue(0);
    harmonyWarm=0; harmonyWasActive=false;
    for(size_t v=0;v<4;++v)
    {
        harmonyEngines[v]=std::make_unique<signalsmith::stretch::SignalsmithStretch<float>>(71+long(v));
        harmonyEngines[v]->presetDefault(juce::jlimit(1,2,getTotalNumOutputChannels()),float(sampleRate),true);
        harmonyOutput[v].setSize(juce::jlimit(1,2,getTotalNumOutputChannels()),juce::jmax(64,maximumExpectedSamplesPerBlock));
        harmonyLevels[v].reset(sampleRate,.020);harmonyLevels[v].setCurrentAndTargetValue(0);
    }
    advanced = parameters.getRawParameterValue("engineMode")->load() >= .5f;
    pitchTracker.prepare(sampleRate);
    finalLimiter.prepare(sampleRate);
    noteCentre = -1; heldNote = -1000;
    detectorChannel=0; detectorChannelEnergy.fill(0); alignedPitchRatio=1;
    displayConfidence.store(0); displayTarget.store(-1);
    const char* bypassIds[] = { "gateEnabled", "noiseEnabled", "deEsserEnabled", "satEnabled", "doublerEnabled", "compEnabled", "eqEnabled" };
    for (size_t i=0; i<bypassMix.size(); ++i)
    {
        bypassMix[i].reset(sampleRate, .010);
        bypassMix[i].setCurrentAndTargetValue(parameters.getRawParameterValue(bypassIds[i])->load());
    }
    outputSmoother.reset(sampleRate, .020);
    outputSmoother.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(parameters.getRawParameterValue("outputGain")->load()));
    formantSmoother.reset(sampleRate, .030);
    formantSmoother.setCurrentAndTargetValue(advanced ? parameters.getRawParameterValue("formantShift")->load() : 0);
    detector.fill(0.0f);
    detectorWrite = samplesSinceAnalysis = 0;
    detectedMidi = -1.0f;
    targetPitchRatio = smoothedRatio = 1.0f;

    mixSmoother.reset(sampleRate, 0.01);
    mixSmoother.setCurrentAndTargetValue(parameters.getRawParameterValue("mix")->load() * 0.01f);
    speedSmoother.reset(sampleRate, 0.01);
    speedSmoother.setCurrentAndTargetValue(parameters.getRawParameterValue("speed")->load());

    gateEnvelope.fill(0.0f); gateGain.fill(1.0f); gateHoldSamples.fill(0);
    noiseEnvelope.fill(0.0f); noiseHpState.fill(0.0f); noisePrevInput.fill(0.0f);
    deEssSideLow.fill(0.0f); satToneLow.fill(0.0f);
    deEssEnvelope = compressorEnvelope = 0.0f;
    doublerPhase = 0.0f; doublerWrite = 0;

    maximumBlockSize = juce::jmax(1, maximumExpectedSamplesPerBlock);
    const int channels = juce::jlimit(1, 2, getTotalNumOutputChannels());
    stretchedBuffer.setSize(channels, maximumBlockSize, false, true, true);
    stretcher.presetDefault(channels, static_cast<float>(sampleRate), true);
    stretcher.setTransposeFactor(1.f);
    stretcher.setFormantSemitones(advanced ? parameters.getRawParameterValue("formantShift")->load() : 0,
                                 advanced && parameters.getRawParameterValue("formantEnabled")->load()>=.5f);
    stretcher.setFormantBase(0);
    // Align the centre of the YIN observation with the spectral processing clock.
    // Split computation distributes mapping over one interval; centre that interval.
    const int controlDelay=juce::jmax(0,stretcher.inputLatency()+stretcher.intervalSamples()/2-static_cast<int>(sampleRate*.040));
    ratioDelay.assign(static_cast<size_t>(controlDelay+1),1.f); ratioDelayWrite=0;
    seekInputLength = stretcher.outputSeekLength(1.0f);
    startupBuffer.setSize(channels, seekInputLength, false, true, true);
    startupBuffer.clear();
    startupInputCount = 0;
    stretcherReady = false;

    const int latency = stretcher.inputLatency() + stretcher.outputLatency();
    pitchLatency = latency;
    setLatencySamples(latency + finalLimiter.latency() + spectralPrep->latency());
    masterRingLength=getLatencySamples()+262144;
    masterDelay.setSize(channels,masterRingLength); masterDelay.clear();masterWrite=0;
    dryDelayLength = latency + maximumBlockSize + 1;
    dryDelayBuffer.setSize(channels, dryDelayLength, false, true, true);
    dryDelayBuffer.clear();
    dryDelayWrite = 0;

    doublerBufferLength = juce::jmax(128, static_cast<int>(sampleRate * 0.10));
    doublerBuffer.setSize(channels, doublerBufferLength, false, true, true);
    doublerBuffer.clear();

    for (auto& channel : vocalEq)
        for (auto& filter : channel)
            filter.reset();

    inputPeak.store(0.0f, std::memory_order_relaxed);
    outputPeak.store(0.0f, std::memory_order_relaxed);
    displayPitch.store(-1.0f, std::memory_order_relaxed);
    displayCorrection.store(0.0f, std::memory_order_relaxed);
    compressorReduction.store(0.0f, std::memory_order_relaxed);
}

void JerzyAutoTuneAudioProcessor::releaseResources() {}

bool JerzyAutoTuneAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo()) && input == output;
}

void JerzyAutoTuneAudioProcessor::analysePitch() noexcept
{
    constexpr int minLag = 44, maxLag = 850;
    std::array<float, maxLag + 1> scores {};
    scores.fill(1.0f);
    double energy = 0.0;
    for (int i = 0; i < detectorSize; ++i)
        energy += detector[static_cast<size_t>(i)] * detector[static_cast<size_t>(i)];

    if (energy / detectorSize < 0.000002)
    {
        detectedMidi = -1.0f; targetPitchRatio = 1.0f; return;
    }

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double difference = 0.0, normA = 0.0, normB = 0.0;
        for (int i = 0; i < detectorSize - maxLag; i += 8)
        {
            const float a = detector[static_cast<size_t>((detectorWrite + i) % detectorSize)];
            const float b = detector[static_cast<size_t>((detectorWrite + i + lag) % detectorSize)];
            difference += (a - b) * (a - b); normA += a * a; normB += b * b;
        }
        scores[static_cast<size_t>(lag)] = static_cast<float>(difference / (normA + normB + 1.0e-12));
    }

    int bestLag = 0; float bestScore = 1.0f;
    for (int lag = minLag + 1; lag < maxLag; ++lag)
    {
        const float score = scores[static_cast<size_t>(lag)];
        if (score <= scores[static_cast<size_t>(lag - 1)] && score < scores[static_cast<size_t>(lag + 1)] && score < 0.24f)
        { bestLag = lag; bestScore = score; break; }
        if (score < bestScore) { bestScore = score; bestLag = lag; }
    }

    if (bestLag == 0 || bestScore > 0.32f)
    {
        detectedMidi = -1.0f; targetPitchRatio = 1.0f; return;
    }

    const float left = scores[static_cast<size_t>(bestLag - 1)];
    const float centre = scores[static_cast<size_t>(bestLag)];
    const float right = scores[static_cast<size_t>(bestLag + 1)];
    const float curvature = left - 2.0f * centre + right;
    const float frac = std::abs(curvature) > 1.0e-9f
        ? juce::jlimit(-0.5f, 0.5f, 0.5f * (left - right) / curvature) : 0.0f;
    const float hz = static_cast<float>(currentSampleRate) / (static_cast<float>(bestLag) + frac);
    const float midi = 69.0f + 12.0f * std::log2(hz / 440.0f);
    if (detectedMidi < 0.0f || std::abs(midi - detectedMidi) > 7.0f) detectedMidi = midi;
    else detectedMidi += (midi - detectedMidi) * 0.35f;
    targetPitchRatio = tunedRatio();
}

float JerzyAutoTuneAudioProcessor::tunedRatio() noexcept
{
    if (detectedMidi < 0.0f) { noteCentre=-1; heldNote = -1000; displayTarget.store(-1); return 1.0f; }
    const float referenceOffset = advanced ? 12.0f*std::log2(parameters.getRawParameterValue("referenceHz")->load()/440.0f) : 0;
    const float preserve = advanced ? parameters.getRawParameterValue("vibratoPreserve")->load()*.01f : 0;
    const float pitch = detectedMidi - referenceOffset;
    if (noteCentre < 0 || std::abs(pitch-noteCentre)>.75f) noteCentre=pitch;
    else noteCentre += (pitch-noteCentre)*.015f;
    const float tuningPitch = pitch + (noteCentre-pitch)*preserve;
    const int key = static_cast<int>(parameters.getRawParameterValue("key")->load());
    const int scale = static_cast<int>(parameters.getRawParameterValue("scale")->load());
    const float amount = parameters.getRawParameterValue("amount")->load() * 0.01f;

    std::array<bool, 12> enabled {};
    bool any = false;
    for (size_t i = 0; i < enabled.size(); ++i)
    {
        enabled[i] = parameters.getRawParameterValue(noteParameterIds[i])->load() >= 0.5f;
        any = any || enabled[i];
    }
    if (!any) { heldNote=-1000; displayTarget.store(-1); return 1.0f; }

    static constexpr std::array<int, 7> major { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr std::array<int, 7> minor { 0, 2, 3, 5, 7, 8, 10 };
    const int nearest = static_cast<int>(std::lround(tuningPitch));
    float best = static_cast<float>(nearest), bestDistance = 100.0f;

    for (int offset = -12; offset <= 12; ++offset)
    {
        const int candidate = nearest + offset;
        const int rel = ((candidate - key) % 12 + 12) % 12;
        bool allowed = scale == 0;
        if (scale == 1) allowed = std::find(major.begin(), major.end(), rel) != major.end();
        else if (scale == 2) allowed = std::find(minor.begin(), minor.end(), rel) != minor.end();
        allowed = allowed && enabled[static_cast<size_t>((candidate % 12 + 12) % 12)];
        const float d = std::abs(static_cast<float>(candidate) - tuningPitch);
        if (allowed && d < bestDistance) { bestDistance = d; best = static_cast<float>(candidate); }
    }

    // The note mask may have no intersection with the selected scale.
    // Never silently quantise to a disallowed chromatic fallback.
    if (bestDistance >= 99.0f) { heldNote=-1000; displayTarget.store(-1); return 1.0f; }
    if (advanced && heldNote > -999)
    {
        const int old = static_cast<int>(heldNote);
        const int rel = ((old-key)%12+12)%12;
        const bool oldAllowed = enabled[static_cast<size_t>((old%12+12)%12)] &&
            (scale==0 || (scale==1 ? std::find(major.begin(),major.end(),rel)!=major.end() : std::find(minor.begin(),minor.end(),rel)!=minor.end()));
        const float margin=parameters.getRawParameterValue("noteStability")->load()*.004f;
        if(oldAllowed && std::abs(heldNote-tuningPitch) <= bestDistance+margin) best=heldNote;
    }
    heldNote=best; displayTarget.store(best);
    const float transpose=advanced ? parameters.getRawParameterValue("transpose")->load() : 0;
    return std::pow(2.0f, ((best-tuningPitch)*amount+transpose) / 12.0f);
}

void JerzyAutoTuneAudioProcessor::processVocalChain(juce::AudioBuffer<float>& buffer) noexcept
{
    const int channels = juce::jmin(2, buffer.getNumChannels());
    const int samples = buffer.getNumSamples();
    if (channels <= 0 || samples <= 0) return;

    const bool gateOn = parameters.getRawParameterValue("gateEnabled")->load() >= 0.5f;
    const float gateThreshold = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("gateThreshold")->load());
    const float gateRange = juce::Decibels::decibelsToGain(-parameters.getRawParameterValue("gateRange")->load());
    const float gateAttack = coeffFromMs(parameters.getRawParameterValue("gateAttack")->load(), currentSampleRate);
    const float gateRelease = coeffFromMs(parameters.getRawParameterValue("gateRelease")->load(), currentSampleRate);
    const int gateHold = static_cast<int>(parameters.getRawParameterValue("gateHold")->load() * 0.001f * static_cast<float>(currentSampleRate));

    const bool noiseOn = parameters.getRawParameterValue("noiseEnabled")->load() >= 0.5f;
    const float noiseThreshold = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("noiseThreshold")->load());
    const float noiseReduction = juce::Decibels::decibelsToGain(-parameters.getRawParameterValue("noiseReduction")->load());
    const float noiseRelease = coeffFromMs(parameters.getRawParameterValue("noiseRelease")->load(), currentSampleRate);
    const float hpHz = parameters.getRawParameterValue("noiseHighPass")->load();
    const float hpA = std::exp(-juce::MathConstants<float>::twoPi * hpHz / static_cast<float>(currentSampleRate));

    const bool deEssOn = parameters.getRawParameterValue("deEsserEnabled")->load() >= 0.5f;
    const float deEssThresholdDb = parameters.getRawParameterValue("deEsserThreshold")->load();
    const float deEssFreq = parameters.getRawParameterValue("deEsserFreq")->load();
    const float deEssRange = parameters.getRawParameterValue("deEsserAmount")->load();
    const float deEssRelease = coeffFromMs(parameters.getRawParameterValue("deEsserRelease")->load(), currentSampleRate);
    const float deEssA = std::exp(-juce::MathConstants<float>::twoPi * deEssFreq / static_cast<float>(currentSampleRate));

    const bool satOn = parameters.getRawParameterValue("satEnabled")->load() >= 0.5f;
    const float satDrive = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("satDrive")->load());
    const float satTone = parameters.getRawParameterValue("satTone")->load() * 0.01f;
    const float satMix = parameters.getRawParameterValue("satMix")->load() * 0.01f;
    const float satOutput = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("satOutput")->load());
    const float toneA = std::exp(-juce::MathConstants<float>::twoPi * 1200.0f / static_cast<float>(currentSampleRate));

    const bool doublerOn = parameters.getRawParameterValue("doublerEnabled")->load() >= 0.5f;
    const float doublerMix = parameters.getRawParameterValue("doublerAmount")->load() * 0.01f;
    const float doublerDelayMs = parameters.getRawParameterValue("doublerDelay")->load();
    const float detune = parameters.getRawParameterValue("doublerDetune")->load();
    const float width = parameters.getRawParameterValue("doublerWidth")->load() * 0.01f;
    const float phaseInc = juce::MathConstants<float>::twoPi * 0.24f / static_cast<float>(currentSampleRate);
    const float modulationMs = juce::jmap(detune, 0.0f, 30.0f, 0.0f, 4.5f);

    const bool compOn = parameters.getRawParameterValue("compEnabled")->load() >= 0.5f;
    const float compThresholdDb = parameters.getRawParameterValue("compThreshold")->load();
    const float compRatio = juce::jmax(1.0f, parameters.getRawParameterValue("compRatio")->load());
    const float compAttack = coeffFromMs(parameters.getRawParameterValue("compAttack")->load(), currentSampleRate);
    const float compRelease = coeffFromMs(parameters.getRawParameterValue("compRelease")->load(), currentSampleRate);
    const float compKnee = parameters.getRawParameterValue("compKnee")->load();
    const float compMakeup = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("compMakeup")->load());
    const float limiterCeiling = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("limiterCeiling")->load());

    const bool eqOn = parameters.getRawParameterValue("eqEnabled")->load() >= 0.5f;
    const float lowCut = parameters.getRawParameterValue("eqLowCut")->load();
    const float lowGain = parameters.getRawParameterValue("eqLow")->load();
    const float midFreq = parameters.getRawParameterValue("eqMidFreq")->load();
    const float midGain = parameters.getRawParameterValue("eqMid")->load();
    const float midQ = parameters.getRawParameterValue("eqMidQ")->load();
    const float highGain = parameters.getRawParameterValue("eqHigh")->load();
    const float highCut = parameters.getRawParameterValue("eqHighCut")->load();
    const float outputGain = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("outputGain")->load());
    const auto value=[this](const char* id){return parameters.getRawParameterValue(id)->load();};
    stageA.settings={compThresholdDb,compRatio,value("compAttack"),value("compRelease"),compKnee,value("compMakeup"),value("compSidechain"),value("compWarm"),int(value("compStyle"))};
    if(value("compPitchFilter")>.5f && detectedMidi>=0) stageA.settings.sidechain=juce::jlimit(50.f,500.f,440.f*std::pow(2.f,(detectedMidi-69)/12.f));
    stageB.settings={value("comp2Threshold"),value("comp2Ratio"),value("comp2Attack"),value("comp2Release"),value("comp2Knee"),value("comp2Makeup"),value("comp2Sidechain"),value("comp2Warm"),int(value("comp2Style"))};
    stageA.configure();stageB.configure();
    stageBFade.setTargetValue(advanced?value("comp2Enabled"):0.f);
    space.settings={advanced&&value("reverbEnabled")>.5f,advanced&&value("delayEnabled")>.5f,value("delayPingPong")>.5f,value("delaySync")>.5f,int(value("reverbType")),int(value("delayDivision")),value("reverbDecay"),value("reverbPreDelay"),value("reverbDamping"),value("reverbWidth"),value("reverbMix"),value("spaceDucking"),value("delayTime"),value("delayFeedback"),value("delayMix")};
    space.configure(tempo.load());
    choir.enabled=advanced&&value("choirEnabled")>.5f;choir.voices=4<<juce::jlimit(0,3,int(value("choirVoices")));
    choir.pitch=value("choirPitch");choir.timing=value("choirTiming");choir.vibrato=value("choirVibrato");choir.width=value("choirWidth");choir.mix=value("choirMix");choir.configure();
    const float hardRange=value("deEsserHard"),hardThreshold=value("deEsserHardThreshold");
    const float hardA=std::exp(-2*juce::MathConstants<float>::pi*2800.f/float(currentSampleRate));
    const float hardB=std::exp(-2*juce::MathConstants<float>::pi*9000.f/float(currentSampleRate));
    const float hardRelease=coeffFromMs(40,currentSampleRate),satDcCoeff=std::exp(-2*juce::MathConstants<float>::pi*12.f/float(currentSampleRate));
    const int saturationModel=advanced?int(value("satModel")):0;

    for (int ch = 0; ch < channels; ++ch)
    {
        auto& e = vocalEq[static_cast<size_t>(ch)];
        e[0].setTarget(makeBiquad(currentSampleRate, lowCut, 0.0f, 0.707f, highPass));
        e[1].setTarget(makeBiquad(currentSampleRate, 160.0f, lowGain, 0.707f, lowShelf));
        e[2].setTarget(makeBiquad(currentSampleRate, midFreq, midGain, midQ, peak));
        e[3].setTarget(makeBiquad(currentSampleRate, 10000.0f, highGain, 0.707f, highShelf));
        e[4].setTarget(makeBiquad(currentSampleRate, highCut, 0.0f, 0.707f, lowPass));
    }
    const float eqSmooth = 1.0f - std::exp(-1.0f / (0.020f * static_cast<float>(currentSampleRate)));

    const bool enabled[] = {gateOn,noiseOn,deEssOn,satOn,doublerOn,compOn,eqOn};
    for(size_t m=0;m<bypassMix.size();++m) bypassMix[m].setTargetValue(enabled[m] ? 1.f : 0.f);
    outputSmoother.setTargetValue(outputGain);
    const bool limitOn=advanced && parameters.getRawParameterValue("limiterEnabled")->load()>=.5f;
    float blockReduction = 0.0f, blockBReduction=0.0f;
    for (int i = 0; i < samples; ++i)
    {
        std::array<float,7> wet {};
        for(size_t m=0;m<wet.size();++m) wet[m]=advanced ? bypassMix[m].getNextValue() : (enabled[m] ? 1.f : 0.f);
        const float outputTrim=outputSmoother.getNextValue();
        float linkedCompPeak = 0.0f;

        for (int ch = 0; ch < channels; ++ch)
        {
            const size_t ci = static_cast<size_t>(ch);
            float x = buffer.getSample(ch, i);

            if (advanced || gateOn)
            {
                const float dry = x;
                const float level = std::abs(x);
                const float envCoeff = level > gateEnvelope[ci] ? coeffFromMs(2.0f, currentSampleRate) : gateRelease;
                gateEnvelope[ci] = envCoeff * gateEnvelope[ci] + (1.0f - envCoeff) * level;
                if (gateEnvelope[ci] >= gateThreshold)
                    gateHoldSamples[ci] = gateHold;
                else if (gateHoldSamples[ci] > 0)
                    --gateHoldSamples[ci];

                const float target = (gateEnvelope[ci] >= gateThreshold || gateHoldSamples[ci] > 0) ? 1.0f : gateRange;
                const float gainCoeff = target > gateGain[ci] ? gateAttack : gateRelease;
                gateGain[ci] = gainCoeff * gateGain[ci] + (1.0f - gainCoeff) * target;
                x *= gateGain[ci];
                x = dry + (x-dry)*wet[0];
            }

            if (advanced || noiseOn)
            {
                const float dry = x;
                const float hp = hpA * (noiseHpState[ci] + x - noisePrevInput[ci]);
                noisePrevInput[ci] = x;
                noiseHpState[ci] = hp;
                const float e = std::abs(hp);
                const float attack = coeffFromMs(5.0f, currentSampleRate);
                const float ec = e > noiseEnvelope[ci] ? attack : noiseRelease;
                noiseEnvelope[ci] = ec * noiseEnvelope[ci] + (1.0f - ec) * e;
                const float t = juce::jlimit(0.0f, 1.0f, noiseEnvelope[ci] / juce::jmax(1.0e-7f, noiseThreshold));
                const float suppress = noiseReduction + (1.0f - noiseReduction) * t;
                x = hp * suppress;
                x = dry + (x-dry)*wet[1];
            }

            if (advanced || deEssOn)
            {
                const float dry = x;
                const float low = (1.0f - deEssA) * x + deEssA * deEssSideLow[ci];
                deEssSideLow[ci] = low;
                const float high = x - low;
                const float s = std::abs(high);
                deEssEnvelope = juce::jmax(s, deEssEnvelope * deEssRelease);
                const float db = juce::Decibels::gainToDecibels(juce::jmax(deEssEnvelope, 1.0e-7f));
                const float reductionDb = juce::jlimit(0.0f, deEssRange, (db - deEssThresholdDb) * 0.75f);
                x = low + high * juce::Decibels::decibelsToGain(-reductionDb);
                if(advanced && hardRange>0)
                {
                    hardLow[ci]=(1-hardA)*x+hardA*hardLow[ci];hardHigh[ci]=(1-hardB)*x+hardB*hardHigh[ci];
                    const float band=hardHigh[ci]-hardLow[ci];
                    hardEnvelope[ci]=juce::jmax(std::abs(band),hardEnvelope[ci]*hardRelease);
                    const float transient=juce::jlimit(0.f,hardRange,(juce::Decibels::gainToDecibels(hardEnvelope[ci],-100.f)-hardThreshold)*.8f);
                    x+=band*(juce::Decibels::decibelsToGain(-transient)-1);
                }
                x = dry + (x-dry)*wet[2];
            }

            if (advanced || satOn)
            {
                const float dry = x;
                satToneLow[ci] = (1.0f - toneA) * x + toneA * satToneLow[ci];
                const float low = satToneLow[ci];
                const float high = x - low;
                const float pre = low * (1.0f - 0.35f * satTone) + high * (1.0f + 0.35f * satTone);
                float driven = std::tanh(pre * satDrive) / std::tanh(juce::jmax(1.0f, satDrive));
                if(saturationModel>0)
                {
                    const float shape=saturationModel==1?.15f:.38f;
                    const float coloured=(std::tanh(pre*satDrive+shape)-std::tanh(shape))/std::max(1.f,std::sqrt(satDrive));
                    driven=coloured-satPrevious[ci]+satDcCoeff*satDc[ci];satPrevious[ci]=coloured;satDc[ci]=driven;
                }
                x = (x + (driven - x) * satMix) * satOutput;
                x = dry + (x-dry)*wet[3];
            }

            doublerBuffer.setSample(ch, doublerWrite, x);
            if (advanced || doublerOn)
            {
                const float dry = x;
                const float phaseOffset = ch == 0 ? 0.0f : juce::MathConstants<float>::pi;
                const float modMs = modulationMs * std::sin(doublerPhase + phaseOffset);
                const float delaySamples = juce::jlimit(1.f, static_cast<float>(doublerBufferLength-2),
                    (doublerDelayMs+modMs)*.001f*static_cast<float>(currentSampleRate));
                const int integral=static_cast<int>(delaySamples);
                const float fraction=advanced ? delaySamples-static_cast<float>(integral) : 0;
                const int read=(doublerWrite-integral+doublerBufferLength)%doublerBufferLength;
                const int previous=(read+doublerBufferLength-1)%doublerBufferLength;
                const float d=doublerBuffer.getSample(ch,read)*(1-fraction)+doublerBuffer.getSample(ch,previous)*fraction;
                const float sideScale = channels > 1 ? (ch == 0 ? width : -width) : 1.0f;
                x = x * (1.0f - 0.35f * doublerMix) + d * doublerMix * 0.55f * sideScale;
                x = dry + (x-dry)*wet[4];
            }

            buffer.setSample(ch, i, x);
            linkedCompPeak = juce::jmax(linkedCompPeak, std::abs(x));
        }

        doublerWrite = (doublerWrite + 1) % doublerBufferLength;
        doublerPhase += phaseInc;
        if (doublerPhase > juce::MathConstants<float>::twoPi)
            doublerPhase -= juce::MathConstants<float>::twoPi;

        const float c = linkedCompPeak > compressorEnvelope ? compAttack : compRelease;
        compressorEnvelope = c * compressorEnvelope + (1.0f - c) * linkedCompPeak;
        const float envDb = juce::Decibels::gainToDecibels(juce::jmax(compressorEnvelope, 1.0e-7f));
        float reductionDb = 0.0f;

        if (advanced || compOn)
        {
            const float over = envDb - compThresholdDb;
            if (compKnee <= 0.001f)
            {
                if (over > 0.0f) reductionDb = (1.0f / compRatio - 1.0f) * over;
            }
            else if (over >= compKnee * 0.5f)
                reductionDb = (1.0f / compRatio - 1.0f) * over;
            else if (over > -compKnee * 0.5f)
            {
                const float kp = over + compKnee * 0.5f;
                reductionDb = (1.0f / compRatio - 1.0f) * kp * kp / (2.0f * compKnee);
            }
        }

        if(advanced)
        {
            float l=buffer.getSample(0,i),r=channels>1?buffer.getSample(1,i):l;
            stageA.process(l,r,wet[5]);stageB.process(l,r,stageBFade.getNextValue());
            blockReduction=juce::jmax(blockReduction,stageA.reduction);blockBReduction=juce::jmax(blockBReduction,stageB.reduction);
            buffer.setSample(0,i,l);if(channels>1)buffer.setSample(1,i,r);
        }
        else
        {
            blockReduction=juce::jmax(blockReduction,-reductionDb*wet[5]);
            const float compGain=1.f+(juce::Decibels::decibelsToGain(reductionDb)*compMakeup-1.f)*wet[5];
            for(int ch=0;ch<channels;++ch)
            {
                float x=buffer.getSample(ch,i)*compGain;
                if(compOn)x=limiterCeiling*std::tanh(x/juce::jmax(.02f,limiterCeiling));
                buffer.setSample(ch,i,x);
            }
        }
        for(int ch=0;ch<channels;++ch)
        {
            float x=buffer.getSample(ch,i);
            if (advanced || eqOn)
            {
                const float dry = x;
                auto& e = vocalEq[static_cast<size_t>(ch)];
                for (auto& filter : e) x = filter.process(x, eqSmooth);
                x = dry + (x-dry)*wet[6];
            }

            buffer.setSample(ch, i, x * (advanced ? outputTrim : outputGain));
        }
    }
    for(int i=0;i<samples;++i)
    {
        float l=buffer.getSample(0,i), r=channels>1 ? buffer.getSample(1,i) : l;
        choir.process(l,r);space.process(l,r);
        finalLimiter.process(l,r,limiterCeiling,limitOn);
        buffer.setSample(0,i,l); if(channels>1) buffer.setSample(1,i,r);
    }
    compressorReduction.store(blockReduction, std::memory_order_relaxed);
    stageBReduction.store(blockBReduction,std::memory_order_relaxed);
}

void JerzyAutoTuneAudioProcessor::collectPitch(const juce::AudioBuffer<float>& buffer,int offset,int count) noexcept
{
    const int channels=juce::jmin(2,buffer.getNumChannels());
    const float energySmoothing=1.f-std::exp(-1.f/(.010f*static_cast<float>(currentSampleRate)));
    for(int i=0;i<count;++i)
    {
        for(int ch=0;ch<channels;++ch)
        {
            const float x=buffer.getSample(ch,offset+i);
            auto& energy=detectorChannelEnergy[static_cast<size_t>(ch)];
            energy+=(x*x-energy)*energySmoothing;
        }
        if(channels>1 && detectorChannelEnergy[static_cast<size_t>(1-detectorChannel)]>
                        4.f*detectorChannelEnergy[static_cast<size_t>(detectorChannel)]+1.e-8f)
            detectorChannel=1-detectorChannel;
        const float sample=buffer.getSample(detectorChannel,offset+i);
        if(pitchTracker.push(sample) && advanced)
        {
            detectedMidi=pitchTracker.pitch();
            displayConfidence.store(pitchTracker.certainty(),std::memory_order_relaxed);
            targetPitchRatio=tunedRatio();
            if(resetKeyAnalysis.exchange(false))keyDetector.reset();
            keyDetector.add(detectedMidi,pitchTracker.certainty());
            suggestedKey.store(keyDetector.key);suggestedMinor.store(keyDetector.minor);suggestedConfidence.store(keyDetector.confidence);
        }
        ratioDelay[static_cast<size_t>(ratioDelayWrite)]=targetPitchRatio;
        ratioDelayWrite=(ratioDelayWrite+1)%static_cast<int>(ratioDelay.size());
        alignedPitchRatio=advanced ? ratioDelay[static_cast<size_t>(ratioDelayWrite)] : targetPitchRatio;
        {
            float mono=sample;
            if(channels>1) mono=.5f*(buffer.getSample(0,offset+i)+buffer.getSample(1,offset+i));
            detector[static_cast<size_t>(detectorWrite)]=mono;
            detectorWrite=(detectorWrite+1)%detectorSize;
            if(++samplesSinceAnalysis>=512) { if(!advanced) analysePitch(); samplesSinceAnalysis=0; }
        }
    }
}

void JerzyAutoTuneAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = juce::jmin(buffer.getNumChannels(), stretchedBuffer.getNumChannels());
    const int samples = buffer.getNumSamples();

    advanced=parameters.getRawParameterValue("engineMode")->load()>=.5f;
    if(!advanced) displayConfidence.store(0,std::memory_order_relaxed);
    if(auto* head=getPlayHead())if(auto position=head->getPosition())if(auto bpm=position->getBpm())tempo.store(*bpm);
    const auto value=[this](const char* id){return parameters.getRawParameterValue(id)->load();};
    spectralPrep->settings={advanced&&value("cleanEnabled")>.5f,advanced&&value("voiceEnabled")>.5f,value("cleanAudition")>.5f,value("cleanReduction"),value("cleanSmooth"),value("cleanMix")*.01f,value("voiceAir"),value("voiceColour"),value("voiceCharacter"),value("voiceMix")*.01f};
    const int masterStart=masterWrite;
    float blockInputPeak=0;
    const float fundamental=detectedMidi>=0?440.f*std::pow(2.f,(detectedMidi-69.f)/12.f):0;
    for(int i=0;i<samples;++i)
    {
        std::array<float,2> input{};
        for(int ch=0;ch<channels;++ch)
        {
            const float raw=buffer.getSample(ch,i);
            input[size_t(ch)]=std::isfinite(raw)?juce::jlimit(-32.f,32.f,raw):0;
            masterDelay.setSample(ch,masterWrite,input[size_t(ch)]);
            blockInputPeak=juce::jmax(blockInputPeak,std::abs(input[size_t(ch)]));
        }
        if(channels==1)input[1]=input[0];
        const float raw=input[0];spectralPrep->process(input[0],input[1],fundamental);analysis.push(raw,input[0]);
        for(int ch=0;ch<channels;++ch)buffer.setSample(ch,i,input[size_t(ch)]);
        masterWrite=(masterWrite+1)%masterRingLength;
    }
    analysis.publish();
    if(blockInputPeak>=1.f)inputOverload.store(juce::jmax(inputOverload.load(),blockInputPeak));
    mixSmoother.setTargetValue(value("tuneEnabled")>.5f?value("mix")*.01f:0.f);
    const bool harmonyActive=advanced&&value("harmonyEnabled")>.5f;
    if(harmonyActive&&!harmonyWasActive)harmonyWarm=stretcher.inputLatency()+stretcher.outputLatency();
    harmonyWasActive=harmonyActive;
    const int harmonyMode=int(value("harmonyMode"));
    const float harmonyMix=value("harmonyMix")*.01f,human=value("harmonyHumanize")*.01f;
    std::array<float,4> harmonyInterval{},harmonyPan{},harmonyFormant{};
    std::array<bool,4> harmonyEnabled{};
    for(int v=0;v<4;++v)
    {
        const auto& p=harmonyParameters[size_t(v)];
        harmonyInterval[size_t(v)]=p[0]->load();harmonyPan[size_t(v)]=p[1]->load()*.01f;
        harmonyFormant[size_t(v)]=p[2]->load();harmonyEnabled[size_t(v)]=p[3]->load()>.5f;
        harmonyLevels[size_t(v)].setTargetValue(jerzy::studio::db(p[4]->load()));
    }
    auto midiIt=midi.cbegin();
    const auto applyMidi=[&](int at)
    {
        while(midiIt!=midi.cend()&&(*midiIt).samplePosition<=at)
        {
            if((*midiIt).numBytes>3){++midiIt;continue;}
            const auto message=(*midiIt).getMessage();++midiIt;
            if(message.isNoteOn()){heldMidi[size_t(message.getNoteNumber())]=true;sustainedMidi[size_t(message.getNoteNumber())]=false;}
            if(message.isNoteOff()){const auto n=size_t(message.getNoteNumber());heldMidi[n]=false;sustainedMidi[n]=sustainPedal;}
            if(message.isSustainPedalOn())sustainPedal=true;
            if(message.isSustainPedalOff()){sustainPedal=false;sustainedMidi.fill(false);}
            if(message.isAllNotesOff()||message.isAllSoundOff()){heldMidi.fill(false);sustainedMidi.fill(false);}
        }
    };
    const auto harmonyRatio=[&](size_t v,const std::array<int,4>& notes)
    {
        float shift=harmonyInterval[v];
        if(harmonyMode==2)shift=notes[v]>=0&&detectedMidi>=0?float(notes[v])-detectedMidi:0;
        else if(harmonyMode==1&&detectedMidi>=0)
        {
            const int root=int(value("key")),scale=int(value("scale"));
            const int candidate=int(std::lround(detectedMidi+shift));int best=candidate;float distance=100;
            constexpr int major[]={0,2,4,5,7,9,11},minor[]={0,2,3,5,7,8,10};
            for(int n=candidate-2;n<=candidate+2;++n)
            {
                const int rel=((n-root)%12+12)%12;const auto* allowed=scale==2?minor:major;
                const bool member=scale==0||std::find(allowed,allowed+7,rel)!=allowed+7;
                if(member&&std::abs(float(n)-detectedMidi-shift)<distance){distance=std::abs(float(n)-detectedMidi-shift);best=n;}
            }
            shift=float(best)-detectedMidi;
        }
        const float drift=human*.06f*std::sin(float(double(sampleClock)/currentSampleRate)*(.77f+float(v)*.17f)+float(v)*2.4f);
        const float base=harmonyMode==0&&value("tuneEnabled")>.5f?smoothedRatio:1.f;
        return base*std::pow(2.f,(shift+drift)/12.f);
    };
    speedSmoother.setTargetValue(parameters.getRawParameterValue("speed")->load());

    int offset = 0;
    while (offset < samples)
    {
        applyMidi(offset);
        int eventLimit=samples-offset;
        if(midiIt!=midi.cend())eventLimit=juce::jmin(eventLimit,juce::jmax(1,(*midiIt).samplePosition-offset));
        if (!stretcherReady)
        {
            const int count = juce::jmin(eventLimit,juce::jmin(64,seekInputLength-startupInputCount));
            collectPitch(buffer,offset,count);
            for (int ch = 0; ch < channels; ++ch)
                startupBuffer.copyFrom(ch, startupInputCount, buffer, ch, offset, count);
            if(harmonyActive)
            {
                std::array<const float*,2> in{};for(int ch=0;ch<channels;++ch)in[size_t(ch)]=buffer.getReadPointer(ch,offset);
                std::array<int,4> notes{-1,-1,-1,-1};int n=0;for(int k=0;k<128&&n<4;++k)if(heldMidi[size_t(k)]||sustainedMidi[size_t(k)])notes[size_t(n++)]=k;
                for(size_t v=0;v<4;++v)
                {
                    harmonyEngines[v]->setTransposeFactor(harmonyRatio(v,notes));
                    harmonyEngines[v]->setFormantSemitones(harmonyFormant[v],value("formantEnabled")>.5f);
                    std::array<float*,2> out{};for(int ch=0;ch<channels;++ch)out[size_t(ch)]=harmonyOutput[v].getWritePointer(ch);
                    harmonyEngines[v]->process(in.data(),count,out.data(),count);
                }
                harmonyWarm=juce::jmax(0,harmonyWarm-count);
            }

            for (int i = 0; i < count; ++i)
            {
                const int dryRead = (dryDelayWrite - pitchLatency + dryDelayLength) % dryDelayLength;
                const float mixValue = mixSmoother.getNextValue();
                for (int ch = 0; ch < channels; ++ch)
                {
                    const float input = buffer.getSample(ch, offset + i);
                    dryDelayBuffer.setSample(ch, dryDelayWrite, input);
                    buffer.setSample(ch, offset + i, dryDelayBuffer.getSample(ch, dryRead) * (1.0f - mixValue));
                }
                dryDelayWrite = (dryDelayWrite + 1) % dryDelayLength;
            }

            startupInputCount += count; offset += count;
            if (startupInputCount == seekInputLength)
            {
                std::array<const float*, 2> startPointers {};
                for (int ch = 0; ch < channels; ++ch)
                    startPointers[static_cast<size_t>(ch)] = startupBuffer.getReadPointer(ch);
                smoothedRatio = advanced ? alignedPitchRatio : targetPitchRatio;
                stretcher.setTransposeFactor(smoothedRatio);
                stretcher.outputSeek(startPointers.data(), seekInputLength);
                stretcherReady = true;
            }
            continue;
        }

        const int count = juce::jmin(maximumBlockSize, juce::jmin(64,eventLimit));
        collectPitch(buffer,offset,count);
        const float speedMs = speedSmoother.skip(count);
        const float timeConstant = juce::jmax(0.002f, speedMs * 0.001f);
        const float smoothing = 1.0f - std::exp(-static_cast<float>(count) /
            (timeConstant * static_cast<float>(currentSampleRate)));
        smoothedRatio += ((advanced ? alignedPitchRatio : targetPitchRatio) - smoothedRatio) * smoothing;
        stretcher.setTransposeFactor(smoothedRatio);
        formantSmoother.setTargetValue(advanced ? parameters.getRawParameterValue("formantShift")->load() : 0);
        stretcher.setFormantSemitones(formantSmoother.skip(count), advanced && parameters.getRawParameterValue("formantEnabled")->load()>=.5f);
        stretcher.setFormantBase(detectedMidi>=0 ? 440.f*std::pow(2.f,(detectedMidi-69.f)/12.f)/static_cast<float>(currentSampleRate) : 0);

        std::array<const float*, 2> inputPointers {};
        std::array<float*, 2> outputPointers {};
        for (int ch = 0; ch < channels; ++ch)
        {
            inputPointers[static_cast<size_t>(ch)] = buffer.getReadPointer(ch, offset);
            outputPointers[static_cast<size_t>(ch)] = stretchedBuffer.getWritePointer(ch);
        }
        stretcher.process(inputPointers.data(), count, outputPointers.data(), count);
        std::array<int,4> midiNotes{-1,-1,-1,-1};int noteCount=0;
        for(int n=0;n<128&&noteCount<4;++n)if(heldMidi[size_t(n)]||sustainedMidi[size_t(n)])midiNotes[size_t(noteCount++)]=n;
        if(harmonyActive || harmonyFade.getCurrentValue()>1.e-5f)
        {
            for(size_t v=0;v<4;++v)
            {
                const float factor=harmonyRatio(v,midiNotes);
                harmonyEngines[v]->setTransposeFactor(factor);
                harmonyEngines[v]->setFormantSemitones(harmonyFormant[v],value("formantEnabled")>.5f);
                harmonyEngines[v]->setFormantBase(detectedMidi>=0?440.f*std::pow(2.f,(detectedMidi-69.f)/12.f)/float(currentSampleRate):0);
                std::array<float*,2> out{};for(int ch=0;ch<channels;++ch)out[size_t(ch)]=harmonyOutput[v].getWritePointer(ch);
                harmonyEngines[v]->process(inputPointers.data(),count,out.data(),count);
            }
        }
        harmonyWarm=juce::jmax(0,harmonyWarm-count);
        harmonyFade.setTargetValue(harmonyActive&&harmonyWarm==0?harmonyMix:0.f);
        sampleClock+=uint64_t(count);

        for (int i = 0; i < count; ++i)
        {
            const int dryRead = (dryDelayWrite - pitchLatency + dryDelayLength) % dryDelayLength;
            const float mixValue = mixSmoother.getNextValue();
            const float harmonyWet=harmonyFade.getNextValue();
            std::array<float,4> voiceGain{};
            for(size_t v=0;v<4;++v)voiceGain[v]=harmonyLevels[v].getNextValue()*float(harmonyEnabled[v]&&(harmonyMode!=2||midiNotes[v]>=0));
            for (int ch = 0; ch < channels; ++ch)
            {
                const float input = buffer.getSample(ch, offset + i);
                dryDelayBuffer.setSample(ch, dryDelayWrite, input);
                const float dry = dryDelayBuffer.getSample(ch, dryRead);
                const float wet = stretchedBuffer.getSample(ch, i);
                float harmony=0;
                if(harmonyWet>1.e-6f)for(size_t v=0;v<4;++v)
                {
                    const float pan=channels>1?std::sqrt(juce::jmax(0.f,1.f+(ch==0?-harmonyPan[v]:harmonyPan[v]))):1.f;
                    harmony+=harmonyOutput[v].getSample(ch,i)*voiceGain[v]*pan;
                }
                buffer.setSample(ch, offset + i, dry + (wet - dry) * mixValue + harmony*harmonyWet);
            }
            dryDelayWrite = (dryDelayWrite + 1) % dryDelayLength;
        }
        offset += count;
    }

    processVocalChain(buffer);

    masterFade.setTargetValue(value("masterEnabled"));
    float blockOutputPeak=0;
    for(int i=0;i<samples;++i)
    {
        const float wet=masterFade.getNextValue();
        const int read=(masterStart+i-getLatencySamples()+masterRingLength)%masterRingLength;
        for(int ch=0;ch<channels;++ch)
        {
            const float dry=masterDelay.getSample(ch,read),processed=buffer.getSample(ch,i);
            const float output=dry+(processed-dry)*wet;buffer.setSample(ch,i,output);
            blockOutputPeak=juce::jmax(blockOutputPeak,std::abs(output));
        }
    }
    if(blockOutputPeak>=1.f)outputOverload.store(juce::jmax(outputOverload.load(),blockOutputPeak));

    inputPeak.store(blockInputPeak, std::memory_order_relaxed);
    outputPeak.store(blockOutputPeak, std::memory_order_relaxed);
    displayPitch.store(detectedMidi, std::memory_order_relaxed);
    displayCorrection.store(detectedMidi < 0.0f ? 0.0f : 1200.0f * std::log2(smoothedRatio), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* JerzyAutoTuneAudioProcessor::createEditor()
{
    return new JerzyAutoTuneAudioProcessorEditor(*this);
}

void JerzyAutoTuneAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty("engineSchema", 3, nullptr);
    if(spectralPrep->profileReady.load())
    {
        std::array<float,jerzy::studio::SpectralPrep::bins> values{};
        for(size_t i=0;i<values.size();++i)values[i]=spectralPrep->savedProfile[i].load();
        const juce::MemoryBlock block(values.data(),values.size()*sizeof(float));
        state.setProperty("noiseProfile",block.toBase64Encoding(),nullptr);
        state.setProperty("noiseProfileRate",spectralPrep->profileRate.load(),nullptr);
    }
    state.setProperty("editorWidth", editorWidth.load(), nullptr);
    state.setProperty("editorHeight", editorHeight.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void JerzyAutoTuneAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
        {
            auto state = juce::ValueTree::fromXml(*xml);
            editorWidth.store(juce::jlimit(960, 1920, static_cast<int>(state.getProperty("editorWidth", 1280))));
            editorHeight.store(juce::jlimit(600, 1200, static_cast<int>(state.getProperty("editorHeight", 800))));
            const bool legacy=!state.hasProperty("engineSchema");
            if(legacy)
            {
                // Old XML has no appended parameters: insert explicit legacy defaults.
                for(const auto& item : std::array<std::pair<const char*,float>,8>{{
                    {"engineMode",0},{"vibratoPreserve",50},{"noteStability",40},{"referenceHz",440},
                    {"formantEnabled",1},{"formantShift",0},{"transpose",0},{"limiterEnabled",1}}})
                {
                    auto node=state.getChildWithProperty("id",item.first);
                    if(!node.isValid()) { node=juce::ValueTree("PARAM"); node.setProperty("id",item.first,nullptr); state.addChild(node,-1,nullptr); }
                    node.setProperty("value",item.second,nullptr);
                }
            }
            for(const auto& spec:jerzy::studio::parameterSpecs())
            {
                if(!state.getChildWithProperty("id",spec.id).isValid())
                {
                    juce::ValueTree node("PARAM");node.setProperty("id",spec.id,nullptr);node.setProperty("value",spec.initial,nullptr);state.addChild(node,-1,nullptr);
                }
            }
            juce::MemoryBlock profile;
            if(profile.fromBase64Encoding(state.getProperty("noiseProfile").toString())&&profile.getSize()==size_t(jerzy::studio::SpectralPrep::bins)*sizeof(float))
            {
                bool valid=true;const auto* data=static_cast<const float*>(profile.getData());
                for(int k=0;k<jerzy::studio::SpectralPrep::bins;++k)valid=valid&&std::isfinite(data[k])&&data[k]>=0;
                if(valid){for(int k=0;k<jerzy::studio::SpectralPrep::bins;++k)spectralPrep->savedProfile[size_t(k)].store(data[k]);spectralPrep->profileRate.store(double(state.getProperty("noiseProfileRate",48000)));spectralPrep->restoreProfile.store(true);spectralPrep->profileReady.store(true);}
            }
            if(!state.hasProperty("noiseProfile"))
            {
                for(auto& bin:spectralPrep->savedProfile)bin.store(0);spectralPrep->profileReady.store(false);spectralPrep->profileRate.store(0);spectralPrep->restoreProfile.store(true);
            }
            parameters.replaceState(state);undoManager.clearUndoHistory();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JerzyAutoTuneAudioProcessor();
}

void JerzyAutoTuneAudioProcessor::applySuggestedKey()
{
    const int key=suggestedKey.load();if(key<0)return;
    undoManager.beginNewTransaction("Apply detected key");
    for(const auto& item:std::array<std::pair<const char*,float>,2>{{{"key",float(key)},{"scale",suggestedMinor.load()?2.f:1.f}}})
    {auto* p=parameters.getParameter(item.first);p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(item.second));p->endChangeGesture();}
}
void JerzyAutoTuneAudioProcessor::applyFactoryPreset(int index)
{
    undoManager.beginNewTransaction("Load preset");
    for(auto* parameter:getParameters())parameter->setValueNotifyingHost(parameter->getDefaultValue());
    const auto set=[this](const char* id,float real){auto* p=parameters.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(real));};
    if(index==1){set("speed",5);set("vibratoPreserve",15);set("compStyle",1);set("compRatio",4);set("comp2Enabled",1);set("comp2Style",2);set("deEsserHard",4);}
    if(index==2){set("mix",0);set("compStyle",2);set("compThreshold",-24);set("compRatio",2);set("deEsserHard",3);}
    if(index==3){set("choirEnabled",1);set("choirVoices",2);set("choirMix",35);set("reverbEnabled",1);set("reverbType",2);}
    if(index==4){set("eqLowCut",250);set("eqHighCut",3500);set("satDrive",12);set("satModel",1);set("satMix",50);}
    if(index==5){set("reverbEnabled",1);set("reverbType",1);set("reverbMix",24);set("spaceDucking",9);set("delayEnabled",1);}
}
double JerzyAutoTuneAudioProcessor::getTailLengthSeconds() const
{
    const auto value=[this](const char* id){return parameters.getRawParameterValue(id)->load();};
    double tail=.5;
    if(value("reverbEnabled")>.5f)tail=std::max(tail,double(value("reverbDecay"))*2.0+.25);
    if(value("delayEnabled")>.5f)
    {
        constexpr double beats[]={1,.5,.75,.25,1.5,2};
        const double period=value("delaySync")>.5f?std::min(6.0,60.0/std::max(20.0,tempo.load())*beats[juce::jlimit(0,5,int(value("delayDivision")))]):value("delayTime")*.001;
        const double feedback=juce::jlimit(.001,.75,double(value("delayFeedback"))*.01);
        tail=std::max(tail,period*std::ceil(std::log(1.e-5)/std::log(feedback)));
    }
    return tail;
}
float JerzyAutoTuneAudioProcessor::eqResponse(float frequency) const noexcept
{
    if(parameters.getRawParameterValue("eqEnabled")->load()<.5f)return 0;
    const auto value=[this](const char* id){return parameters.getRawParameterValue(id)->load();};
    const double fs=graphRate.load();
    const std::array<std::array<float,5>,5> filters{{makeBiquad(fs,value("eqLowCut"),0,.707f,highPass),makeBiquad(fs,160,value("eqLow"),.707f,lowShelf),makeBiquad(fs,value("eqMidFreq"),value("eqMid"),value("eqMidQ"),peak),makeBiquad(fs,10000,value("eqHigh"),.707f,highShelf),makeBiquad(fs,value("eqHighCut"),0,.707f,lowPass)}};
    const auto z=std::polar(1.0,-2.0*juce::MathConstants<double>::pi*frequency/fs);double gain=1;
    for(const auto& c:filters)gain*=std::abs((double(c[0])+double(c[1])*z+double(c[2])*z*z)/(1.0+double(c[3])*z+double(c[4])*z*z));
    return float(20*std::log10(std::max(1.e-6,gain)));
}
