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
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{}

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
    seekInputLength = stretcher.outputSeekLength(1.0f);
    startupBuffer.setSize(channels, seekInputLength, false, true, true);
    startupBuffer.clear();
    startupInputCount = 0;
    stretcherReady = false;

    const int latency = stretcher.inputLatency() + stretcher.outputLatency();
    setLatencySamples(latency);
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

float JerzyAutoTuneAudioProcessor::tunedRatio() const noexcept
{
    if (detectedMidi < 0.0f) return 1.0f;
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
    if (!any) return 1.0f;

    static constexpr std::array<int, 7> major { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr std::array<int, 7> minor { 0, 2, 3, 5, 7, 8, 10 };
    const int nearest = static_cast<int>(std::lround(detectedMidi));
    float best = static_cast<float>(nearest), bestDistance = 100.0f;

    for (int offset = -12; offset <= 12; ++offset)
    {
        const int candidate = nearest + offset;
        const int rel = ((candidate - key) % 12 + 12) % 12;
        bool allowed = scale == 0;
        if (scale == 1) allowed = std::find(major.begin(), major.end(), rel) != major.end();
        else if (scale == 2) allowed = std::find(minor.begin(), minor.end(), rel) != minor.end();
        allowed = allowed && enabled[static_cast<size_t>((candidate % 12 + 12) % 12)];
        const float d = std::abs(static_cast<float>(candidate) - detectedMidi);
        if (allowed && d < bestDistance) { bestDistance = d; best = static_cast<float>(candidate); }
    }

    const float corrected = detectedMidi + (best - detectedMidi) * amount;
    return std::pow(2.0f, (corrected - detectedMidi) / 12.0f);
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

    for (int i = 0; i < samples; ++i)
    {
        float linkedCompPeak = 0.0f;

        for (int ch = 0; ch < channels; ++ch)
        {
            const size_t ci = static_cast<size_t>(ch);
            float x = buffer.getSample(ch, i);

            if (gateOn)
            {
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
            }

            if (noiseOn)
            {
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
            }

            if (deEssOn)
            {
                const float low = (1.0f - deEssA) * x + deEssA * deEssSideLow[ci];
                deEssSideLow[ci] = low;
                const float high = x - low;
                const float s = std::abs(high);
                deEssEnvelope = juce::jmax(s, deEssEnvelope * deEssRelease);
                const float db = juce::Decibels::gainToDecibels(juce::jmax(deEssEnvelope, 1.0e-7f));
                const float reductionDb = juce::jlimit(0.0f, deEssRange, (db - deEssThresholdDb) * 0.75f);
                x = low + high * juce::Decibels::decibelsToGain(-reductionDb);
            }

            if (satOn)
            {
                satToneLow[ci] = (1.0f - toneA) * x + toneA * satToneLow[ci];
                const float low = satToneLow[ci];
                const float high = x - low;
                const float pre = low * (1.0f - 0.35f * satTone) + high * (1.0f + 0.35f * satTone);
                const float driven = std::tanh(pre * satDrive) / std::tanh(juce::jmax(1.0f, satDrive));
                x = (x + (driven - x) * satMix) * satOutput;
            }

            doublerBuffer.setSample(ch, doublerWrite, x);
            if (doublerOn)
            {
                const float phaseOffset = ch == 0 ? 0.0f : juce::MathConstants<float>::pi;
                const float modMs = modulationMs * std::sin(doublerPhase + phaseOffset);
                const int delaySamples = juce::jlimit(1, doublerBufferLength - 2,
                    static_cast<int>((doublerDelayMs + modMs) * 0.001f * static_cast<float>(currentSampleRate)));
                const int read = (doublerWrite - delaySamples + doublerBufferLength) % doublerBufferLength;
                const float d = doublerBuffer.getSample(ch, read);
                const float sideScale = channels > 1 ? (ch == 0 ? width : -width) : 1.0f;
                x = x * (1.0f - 0.35f * doublerMix) + d * doublerMix * 0.55f * sideScale;
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

        if (compOn)
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

        const float compGain = compOn ? juce::Decibels::decibelsToGain(reductionDb) * compMakeup : 1.0f;

        for (int ch = 0; ch < channels; ++ch)
        {
            float x = buffer.getSample(ch, i) * compGain;
            if (compOn)
                x = limiterCeiling * std::tanh(x / juce::jmax(0.02f, limiterCeiling));

            if (eqOn)
            {
                auto& e = vocalEq[static_cast<size_t>(ch)];
                for (auto& filter : e) x = filter.process(x, eqSmooth);
            }

            buffer.setSample(ch, i, x * outputGain);
        }
    }
}

void JerzyAutoTuneAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = juce::jmin(buffer.getNumChannels(), stretchedBuffer.getNumChannels());
    const int samples = buffer.getNumSamples();

    float blockInputPeak = 0.0f;
    for (int i = 0; i < samples; ++i)
    {
        float detectorSample = 0.0f;
        for (int ch = 0; ch < channels; ++ch) detectorSample += buffer.getSample(ch, i);
        detectorSample /= static_cast<float>(juce::jmax(1, channels));
        blockInputPeak = juce::jmax(blockInputPeak, std::abs(detectorSample));
        detector[static_cast<size_t>(detectorWrite)] = detectorSample;
        detectorWrite = (detectorWrite + 1) % detectorSize;
        if (++samplesSinceAnalysis >= 512) { analysePitch(); samplesSinceAnalysis = 0; }
    }

    mixSmoother.setTargetValue(parameters.getRawParameterValue("mix")->load() * 0.01f);
    speedSmoother.setTargetValue(parameters.getRawParameterValue("speed")->load());

    int offset = 0;
    while (offset < samples)
    {
        if (!stretcherReady)
        {
            const int count = juce::jmin(samples - offset, seekInputLength - startupInputCount);
            for (int ch = 0; ch < channels; ++ch)
                startupBuffer.copyFrom(ch, startupInputCount, buffer, ch, offset, count);

            for (int i = 0; i < count; ++i)
            {
                const int dryRead = (dryDelayWrite - getLatencySamples() + dryDelayLength) % dryDelayLength;
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
                smoothedRatio = targetPitchRatio;
                stretcher.setTransposeFactor(smoothedRatio);
                stretcher.outputSeek(startPointers.data(), seekInputLength);
                stretcherReady = true;
            }
            continue;
        }

        const int count = juce::jmin(maximumBlockSize, juce::jmin(64, samples - offset));
        const float speedMs = speedSmoother.skip(count);
        const float timeConstant = juce::jmax(0.002f, speedMs * 0.001f);
        const float smoothing = 1.0f - std::exp(-static_cast<float>(count) /
            (timeConstant * static_cast<float>(currentSampleRate)));
        smoothedRatio += (targetPitchRatio - smoothedRatio) * smoothing;
        stretcher.setTransposeFactor(smoothedRatio);

        std::array<const float*, 2> inputPointers {};
        std::array<float*, 2> outputPointers {};
        for (int ch = 0; ch < channels; ++ch)
        {
            inputPointers[static_cast<size_t>(ch)] = buffer.getReadPointer(ch, offset);
            outputPointers[static_cast<size_t>(ch)] = stretchedBuffer.getWritePointer(ch);
        }
        stretcher.process(inputPointers.data(), count, outputPointers.data(), count);

        for (int i = 0; i < count; ++i)
        {
            const int dryRead = (dryDelayWrite - getLatencySamples() + dryDelayLength) % dryDelayLength;
            const float mixValue = mixSmoother.getNextValue();
            for (int ch = 0; ch < channels; ++ch)
            {
                const float input = buffer.getSample(ch, offset + i);
                dryDelayBuffer.setSample(ch, dryDelayWrite, input);
                const float dry = dryDelayBuffer.getSample(ch, dryRead);
                const float wet = stretchedBuffer.getSample(ch, i);
                buffer.setSample(ch, offset + i, dry + (wet - dry) * mixValue);
            }
            dryDelayWrite = (dryDelayWrite + 1) % dryDelayLength;
        }
        offset += count;
    }

    processVocalChain(buffer);

    float blockOutputPeak = 0.0f;
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < samples; ++i)
            blockOutputPeak = juce::jmax(blockOutputPeak, std::abs(buffer.getSample(ch, i)));

    inputPeak.store(blockInputPeak, std::memory_order_relaxed);
    outputPeak.store(blockOutputPeak, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* JerzyAutoTuneAudioProcessor::createEditor()
{
    return new JerzyAutoTuneAudioProcessorEditor(*this);
}

void JerzyAutoTuneAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, destData);
}

void JerzyAutoTuneAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JerzyAutoTuneAudioProcessor();
}
