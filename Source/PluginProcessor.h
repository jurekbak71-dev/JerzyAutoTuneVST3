#pragma once

#include <JuceHeader.h>
#include <signalsmith-stretch/signalsmith-stretch.h>
#include "VocalDSP.h"
#include <array>
#include <atomic>

class JerzyAutoTuneAudioProcessorEditor;

class JerzyAutoTuneAudioProcessor final : public juce::AudioProcessor
{
public:
    JerzyAutoTuneAudioProcessor();
    ~JerzyAutoTuneAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "JERZY AUTO TUNE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.50; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS parameters;
    float getOutputPeak() const noexcept { return outputPeak.load(std::memory_order_relaxed); }
    float getInputPeak() const noexcept { return inputPeak.load(std::memory_order_relaxed); }

    float getPitchConfidence() const noexcept { return displayConfidence.load(std::memory_order_relaxed); }
    float getTargetNote() const noexcept { return displayTarget.load(std::memory_order_relaxed); }
    float getDetectedPitch() const noexcept { return displayPitch.load(std::memory_order_relaxed); }
    float getCorrectionCents() const noexcept { return displayCorrection.load(std::memory_order_relaxed); }
    float getCompressorReduction() const noexcept { return compressorReduction.load(std::memory_order_relaxed); }

private:
    friend class JerzyAutoTuneAudioProcessorEditor;
    static APVTS::ParameterLayout createParameterLayout();
    void analysePitch() noexcept;
    float tunedRatio() noexcept;
    void collectPitch(const juce::AudioBuffer<float>&, int, int) noexcept;
    void processVocalChain(juce::AudioBuffer<float>&) noexcept;

    struct Biquad
    {
        std::array<float, 5> coefficients { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        std::array<float, 5> targetCoefficients { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
        void setTarget(const std::array<float, 5>& c) noexcept { targetCoefficients = c; }
        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }
        float process(float input, float smoothing) noexcept;
    };

    jerzy::PitchTracker pitchTracker;
    jerzy::OutputLimiter finalLimiter;
    std::array<juce::SmoothedValue<float>, 7> bypassMix;
    juce::SmoothedValue<float> outputSmoother, formantSmoother;
    std::atomic<float> displayConfidence { 0 }, displayTarget { -1 };
    float noteCentre = -1.0f, heldNote = -1000.0f;
    int pitchLatency = 0, ratioDelayWrite = 0, detectorChannel = 0;
    std::vector<float> ratioDelay;
    std::array<float, 2> detectorChannelEnergy {};
    float alignedPitchRatio = 1.0f;
    bool advanced = true;

    static constexpr int detectorSize = 4096;
    std::array<float, detectorSize> detector {};
    int detectorWrite = 0, samplesSinceAnalysis = 0;
    float detectedMidi = -1.0f, targetPitchRatio = 1.0f, smoothedRatio = 1.0f;
    double currentSampleRate = 44100.0;

    int maximumBlockSize = 512, dryDelayLength = 1, dryDelayWrite = 0;
    int seekInputLength = 0, startupInputCount = 0;
    bool stretcherReady = false;
    juce::AudioBuffer<float> stretchedBuffer, dryDelayBuffer, startupBuffer;
    signalsmith::stretch::SignalsmithStretch<float> stretcher;
    juce::SmoothedValue<float> mixSmoother, speedSmoother;

    std::array<float, 2> gateEnvelope {}, gateGain {};
    std::array<int, 2> gateHoldSamples {};
    std::array<float, 2> noiseEnvelope {}, noiseHpState {}, noisePrevInput {};
    std::array<float, 2> deEssSideLow {}, satToneLow {};
    float deEssEnvelope = 0.0f, compressorEnvelope = 0.0f;
    float doublerPhase = 0.0f;
    int doublerWrite = 0, doublerBufferLength = 1;
    juce::AudioBuffer<float> doublerBuffer;
    std::array<std::array<Biquad, 5>, 2> vocalEq;

    std::atomic<int> editorWidth { 1008 }, editorHeight { 864 };
    std::atomic<float> displayPitch { -1.0f }, displayCorrection { 0.0f }, compressorReduction { 0.0f };
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyAutoTuneAudioProcessor)
};
