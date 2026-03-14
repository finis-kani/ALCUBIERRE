#pragma once

#include <JuceHeader.h>
#include "RingBuffer.h"

// ─── Single grain ─────────────────────────────────────────────────────────────
struct Grain
{
    double absoluteReadPos = 0.0;  // absolute position in ring buffer (fractional)
    double playRate        = 1.0;  // playback speed (1.0 = natural, affects pitch)
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
    //   output         — cleared buffer to write wet signal into
    //   ringBuffer     — source of past audio
    //   futureData     — LPC-predicted samples, [channel][sample], may be nullptr
    //   futureLength   — number of samples in each future channel
    //   x              — temporal displacement (0=present, 1=10 s back)
    //   y              — warp density (0=1 grain, 1=16 grains)
    //   z              — scatter / randomness
    //   alphaOmega     — future blend (0=past only, 1=future only)
    //   grainSizeMs    — grain size in milliseconds
    void processBlock(juce::AudioBuffer<float>&  output,
                      const RingBuffer<float>&   ringBuffer,
                      const float* const*        futureData,
                      int                        futureLength,
                      float x, float y, float z,
                      float alphaOmega, float grainSizeMs);

private:
    void spawnGrain(const RingBuffer<float>& ringBuffer,
                    int   futureLength,
                    float x, float z, float alphaOmega,
                    double grainSizeSamples);

    Grain  grains[MAX_GRAINS];
    double sampleRate         = 44100.0;
    double timeSinceLastGrain = 0.0;
    int    numChannels        = 2;

    juce::Random random;
};
