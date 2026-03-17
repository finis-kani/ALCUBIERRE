#pragma once

#include <JuceHeader.h>
#include "RingBuffer.h"

// ─── Single grain ─────────────────────────────────────────────────────────────
struct Grain
{
    double absoluteReadPos = 0.0;  // absolute position in ring buffer (fractional)
    double playRate        = 1.0;  // effective playback speed (includes vinyl mod)
    double basePlayRate    = 1.0;  // playback speed without vinyl modulation
    double sizeInSamples   = 1024.0;
    double phase           = 0.0;  // 0 → 1 lifetime
    float  amplitude       = 1.0f;
    bool   active          = false;

    // Future-mode grains read from the LPC-predicted buffer instead of ring buffer
    bool   isFuture        = false;
    double futureReadPos   = 0.0;  // position in future buffer (fractional)
};

// ─── GranularEngine ───────────────────────────────────────────────────────────
class GranularEngine
{
public:
    static constexpr int MAX_GRAINS = 32;

    GranularEngine();

    void prepare(double sampleRate, int maxBlockSize);

    // Process one block.
    //   output                  — cleared buffer to write wet signal into
    //   ringBuffer              — source of past audio
    //   futureData              — LPC-predicted samples, [channel][sample]
    //   futureLength            — number of samples in each future channel
    //   temporalDisplacement    — read offset behind write head, in samples (≥ 0)
    //   y                       — warp density (0=1 grain, 1=16 grains)
    //   z                       — scatter / randomness
    //   futureBlend             — future vs past blend (0=past only, 1=future only)
    //   grainSizeMs             — grain size in milliseconds
    //   tapePlayRate            — playback rate from TIME position (1.0 = normal speed)
    void processBlock(juce::AudioBuffer<float>&  output,
                      const RingBuffer<float>&   ringBuffer,
                      const float* const*        futureData,
                      int                        futureLength,
                      float temporalDisplacement,
                      float y, float z,
                      float futureBlend,
                      float grainSizeMs,
                      float tapePlayRate = 1.0f);

private:
    void spawnGrain(const RingBuffer<float>& ringBuffer,
                    int    futureLength,
                    float  temporalDisplacement,
                    float  z, float futureBlend,
                    double grainSizeSamples,
                    float  tapePlayRate);

    Grain  grains[MAX_GRAINS];
    double sampleRate         = 44100.0;
    double timeSinceLastGrain = 0.0;
    int    numChannels        = 2;

    juce::Random random;
};
