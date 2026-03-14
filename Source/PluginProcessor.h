#pragma once

#include <JuceHeader.h>
#include "RingBuffer.h"
#include "GranularEngine.h"
#include "GlitchEngine.h"
#include <array>
#include <vector>

// ─── Linear Predictive Coding helper ─────────────────────────────────────────
class LinearPredictor
{
public:
    static constexpr int ORDER = 8;

    void train(const float* data, int length);
    void generateFuture(const float* history, int histLen,
                        float* future, int futureLength) const;

private:
    float coeffs[ORDER] = {};
    bool  trained       = false;
};

// ─── AlcubierreProcessor ─────────────────────────────────────────────────────
class AlcubierreProcessor : public juce::AudioProcessor
{
public:
    AlcubierreProcessor();
    ~AlcubierreProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ALCUBIERRE"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int  getNumPrograms()                               override { return 1; }
    int  getCurrentProgram()                            override { return 0; }
    void setCurrentProgram(int)                         override {}
    const juce::String getProgramName(int)              override { return "Default"; }
    void changeProgramName(int, const juce::String&)    override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    const RingBuffer<float>& getRingBuffer() const { return ringBuffer; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateFutureBuffer(int numChannels);

    // ── DSP components ────────────────────────────────────────────────────────
    RingBuffer<float>  ringBuffer;
    GranularEngine     granularEngine;
    GlitchEngine       glitchEngine;

    juce::AudioBuffer<float> writeInputBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> feedbackBuffer;

    // ── Future buffer ─────────────────────────────────────────────────────────
    static constexpr int MAX_CHANNELS           = 2;
    static constexpr int FUTURE_BUFFER_LENGTH   = 4096;
    static constexpr int FUTURE_UPDATE_INTERVAL = 2048;
    static constexpr int LPC_TRAIN_LENGTH       = 2048;

    std::array<std::vector<float>, MAX_CHANNELS> futureData;
    std::array<const float*, MAX_CHANNELS>       futurePtrs;
    LinearPredictor predictors[MAX_CHANNELS];
    int samplesSinceLastFutureUpdate = 0;

    std::array<std::vector<float>, MAX_CHANNELS> lpcTrainBuf;

    double currentSampleRate = 44100.0;

    // ── Parameter smoothing (~30 ms one-pole) ─────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> timeSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> ySmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> zSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> grainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedbackSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> deltaSmoothed;

    // ── Vinyl / Doppler pitch modulation ──────────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> vinylPitchSmoothed;

    // ── RMS-based output gain compensation ────────────────────────────────────
    float dryRmsEnv  = 0.0f;
    float wetRmsEnv  = 0.0f;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainCompSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlcubierreProcessor)
};
