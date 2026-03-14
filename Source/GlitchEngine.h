#pragma once

#include <JuceHeader.h>

// ─── GlitchEngine ─────────────────────────────────────────────────────────────
// Applies probability-based stutter, pitch-jump read offsets, and bit-mangling
// to the granular wet signal. Intensity is driven by the Z parameter.
class GlitchEngine
{
public:
    GlitchEngine();

    void prepare(double sampleRate);

    // Process wet buffer in-place.
    // z  — dimensional scatter / glitch probability [0, 1]
    void process(juce::AudioBuffer<float>& buffer, float z);

private:
    // ── Stutter ──────────────────────────────────────────────────────────────
    void  tryStartStutter(const juce::AudioBuffer<float>& src, float z, int numSamples);
    void  applyStutter(juce::AudioBuffer<float>& buffer);

    bool  stuttering               = false;
    int   stutterWriteCount        = 0;  // how many captured samples
    int   stutterTotalLength       = 0;  // captured block length
    int   stutterRepeatRemaining   = 0;  // how many more output samples to stutter
    int   stutterReadPos           = 0;
    juce::AudioBuffer<float> stutterBuffer;

    // ── Bit mangle ───────────────────────────────────────────────────────────
    void applyBitMangle(float* data, int numSamples, float intensity);

    // ── State ─────────────────────────────────────────────────────────────────
    double sampleRate              = 44100.0;
    int    samplesSinceLastGlitch  = 0;
    juce::Random random;
};
