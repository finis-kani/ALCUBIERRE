#include "GlitchEngine.h"

GlitchEngine::GlitchEngine()  = default;

void GlitchEngine::prepare(double sr)
{
    sampleRate             = sr;
    stuttering             = false;
    stutterWriteCount      = 0;
    stutterTotalLength     = 0;
    stutterRepeatRemaining = 0;
    stutterReadPos         = 0;
    samplesSinceLastGlitch = 0;
}

// ─── Process ─────────────────────────────────────────────────────────────────
void GlitchEngine::process(juce::AudioBuffer<float>& buffer, float z)
{
    if (z < 0.001f) return;

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();

    samplesSinceLastGlitch += numSamples;

    // ── Bit mangle (continuous, subtle at low z, harsh at high z) ────────────
    // Only kick in above z=0.3
    const float mangleIntensity = juce::jmax(0.0f, z - 0.3f) / 0.7f;
    if (mangleIntensity > 0.0f)
    {
        for (int ch = 0; ch < numCh; ++ch)
            applyBitMangle(buffer.getWritePointer(ch), numSamples, mangleIntensity);
    }

    // ── Stutter logic ─────────────────────────────────────────────────────────
    if (!stuttering)
    {
        // Minimum gap between glitches: 150 ms
        const int minGap = (int)(0.15 * sampleRate);

        if (samplesSinceLastGlitch >= minGap)
        {
            // Per-block trigger probability scales with z²
            const float prob = z * z * 0.15f;
            if (random.nextFloat() < prob)
                tryStartStutter(buffer, z, numSamples);
        }
    }

    if (stuttering)
        applyStutter(buffer);
}

// ─── Start a stutter event ───────────────────────────────────────────────────
void GlitchEngine::tryStartStutter(const juce::AudioBuffer<float>& src,
                                    float z, int numSamples)
{
    // Capture length: 10 ms – 200 ms, inversely scaled so high-z = shorter (faster)
    const double maxCapMs  = 200.0 - (double)z * 160.0;   // 40..200 ms
    const double capMs     = 10.0 + random.nextFloat() * maxCapMs;
    const int    captureLen = juce::jlimit(64, (int)(0.3 * sampleRate),
                                           (int)(capMs * 0.001 * sampleRate));

    // Repeat duration: 1–6 repetitions
    const int repeats = 1 + random.nextInt(5);

    stutterBuffer.setSize(src.getNumChannels(), captureLen, false, true, true);
    const int copyLen = juce::jmin(captureLen, numSamples);
    for (int ch = 0; ch < src.getNumChannels(); ++ch)
        stutterBuffer.copyFrom(ch, 0, src, ch, 0, copyLen);

    stutterWriteCount      = copyLen;
    stutterTotalLength     = captureLen;
    stutterRepeatRemaining = captureLen * repeats;
    stutterReadPos         = 0;
    stuttering             = true;
    samplesSinceLastGlitch = 0;
}

// ─── Apply stutter to output buffer ─────────────────────────────────────────
void GlitchEngine::applyStutter(juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numCh       = buffer.getNumChannels();
    const int capturedLen = stutterBuffer.getNumSamples();

    if (capturedLen == 0)
    {
        stuttering = false;
        return;
    }

    for (int i = 0; i < numSamples && stutterRepeatRemaining > 0; ++i)
    {
        const int rp = stutterReadPos % capturedLen;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const int sch = ch % stutterBuffer.getNumChannels();
            buffer.setSample(ch, i, stutterBuffer.getSample(sch, rp));
        }
        ++stutterReadPos;
        --stutterRepeatRemaining;
    }

    if (stutterRepeatRemaining <= 0)
        stuttering = false;
}

// ─── Bit mangle ──────────────────────────────────────────────────────────────
void GlitchEngine::applyBitMangle(float* data, int numSamples, float intensity)
{
    // Reduce effective bit depth: 24 bits at intensity=0, down to 3 bits at intensity=1
    const int bits = juce::jlimit(3, 24, (int)(3.0f + (1.0f - intensity) * 21.0f));
    const float scale = std::ldexp(1.0f, bits - 1);  // 2^(bits-1)

    for (int i = 0; i < numSamples; ++i)
    {
        const float clamped = juce::jlimit(-1.0f, 1.0f, data[i]);
        data[i] = std::round(clamped * scale) / scale;
    }
}
