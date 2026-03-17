#include "GranularEngine.h"

GranularEngine::GranularEngine()
{
    for (auto& g : grains)
        g.active = false;
}

void GranularEngine::prepare(double sr, int /*maxBlockSize*/)
{
    sampleRate = sr;
    timeSinceLastGrain = 0.0;
    for (auto& g : grains)
        g.active = false;
}

// ─── Spawn one grain ──────────────────────────────────────────────────────────
void GranularEngine::spawnGrain(const RingBuffer<float>& ringBuffer,
                                 int    futureLength,
                                 float  temporalDisplacement,
                                 float  z, float futureBlend,
                                 double grainSizeSamples,
                                 float  tapePlayRate)
{
    // Find an inactive slot
    Grain* slot = nullptr;
    for (auto& g : grains)
    {
        if (!g.active) { slot = &g; break; }
    }
    if (slot == nullptr) return;  // all slots busy

    const double ringSize = (double)ringBuffer.getSize();

    // Decide whether this grain reads from future or past
    const bool useFuture = (futureLength > 0) && (random.nextFloat() < futureBlend);

    // Base temporal displacement already in samples
    const double baseOffset = (double)temporalDisplacement;

    // Scatter: random offset proportional to z and grain size
    const double scatter = ((double)random.nextFloat() - 0.5) * 2.0
                         * (double)z * grainSizeSamples * 4.0;

    // Slight playback rate variation per grain (pitch shimmer from z)
    const double rateVariation = 1.0 + ((double)random.nextFloat() - 0.5) * (double)z * 0.2;

    slot->active        = true;
    slot->phase         = 0.0;
    slot->sizeInSamples = grainSizeSamples;
    slot->amplitude     = 1.0f;
    slot->basePlayRate  = rateVariation;
    slot->playRate      = rateVariation * (double)tapePlayRate;

    if (useFuture)
    {
        slot->isFuture     = true;
        // Start somewhere in the future buffer
        double startPos = random.nextFloat() * (double)(futureLength - 1);
        slot->futureReadPos = startPos;
        slot->absoluteReadPos = 0.0;
    }
    else
    {
        slot->isFuture = false;
        // Absolute read position = writePos - baseOffset + scatter
        const int wp = ringBuffer.getWritePosition();
        double readPos = (double)wp - baseOffset + scatter;
        // Wrap into [0, ringSize)
        while (readPos < 0.0)          readPos += ringSize;
        while (readPos >= ringSize)    readPos -= ringSize;
        slot->absoluteReadPos = readPos;
        slot->futureReadPos   = 0.0;
    }
}

// ─── Process block ────────────────────────────────────────────────────────────
void GranularEngine::processBlock(juce::AudioBuffer<float>&  output,
                                   const RingBuffer<float>&   ringBuffer,
                                   const float* const*        futureData,
                                   int                        futureLength,
                                   float temporalDisplacement,
                                   float y, float z,
                                   float futureBlend,
                                   float grainSizeMs,
                                   float tapePlayRate)
{
    output.clear();

    const int numSamples = output.getNumSamples();
    numChannels          = output.getNumChannels();

    const double grainSizeSamples = juce::jmax(64.0, (grainSizeMs / 1000.0) * sampleRate);

    // Target simultaneous grain count: 1..16
    const double targetOverlap    = 1.0 + (double)y * 15.0;
    const double interGrainInterval = grainSizeSamples / targetOverlap;

    // Overall output gain: normalise so we don't clip with many overlapping grains
    const float outputGain = 1.0f / (float)juce::jmax(1.0, targetOverlap * 0.5);

    const double ringSize = (double)ringBuffer.getSize();

    // ── Apply vinyl pitch mod to all currently active grains ──────────────────
    for (auto& grain : grains)
    {
        if (grain.active)
            grain.playRate = grain.basePlayRate * (double)tapePlayRate;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        // ── Grain scheduling ────────────────────────────────────────────────
        timeSinceLastGrain += 1.0;
        if (timeSinceLastGrain >= interGrainInterval)
        {
            timeSinceLastGrain -= interGrainInterval;
            spawnGrain(ringBuffer, futureLength, temporalDisplacement,
                       z, futureBlend, grainSizeSamples, tapePlayRate);
        }

        // ── Sum active grains ────────────────────────────────────────────────
        for (auto& grain : grains)
        {
            if (!grain.active) continue;

            // Hanning window
            const float window = 0.5f * (1.0f - std::cos(
                juce::MathConstants<float>::twoPi * (float)grain.phase));
            const float amp = grain.amplitude * window * outputGain;

            if (grain.isFuture && futureData != nullptr && futureLength > 0)
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const int fch = ch % 2;
                    int   fi0 = (int)grain.futureReadPos;
                    int   fi1 = fi0 + 1;
                    fi0 = juce::jlimit(0, futureLength - 1, fi0);
                    fi1 = juce::jlimit(0, futureLength - 1, fi1);
                    const float frac = (float)(grain.futureReadPos - (int)grain.futureReadPos);
                    const float s = futureData[fch][fi0] * (1.0f - frac)
                                  + futureData[fch][fi1] * frac;
                    output.addSample(ch, i, s * amp);
                }
                grain.futureReadPos += grain.playRate;
                if (grain.futureReadPos >= (double)(futureLength - 1))
                    grain.futureReadPos = 0.0;
            }
            else
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const float s = (float)ringBuffer.readAtAbsolutePosition(ch, grain.absoluteReadPos);
                    output.addSample(ch, i, s * amp);
                }
                grain.absoluteReadPos += grain.playRate;
                if (grain.absoluteReadPos >= ringSize) grain.absoluteReadPos -= ringSize;
                if (grain.absoluteReadPos < 0.0)       grain.absoluteReadPos += ringSize;
            }

            // Advance phase; deactivate at end of grain
            grain.phase += 1.0 / grain.sizeInSamples;
            if (grain.phase >= 1.0)
                grain.active = false;
        }
    }
}
