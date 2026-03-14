#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <cmath>

// ─── WarpFieldVisualizer ──────────────────────────────────────────────────────
// Animated radial "warp field" drawn behind the knobs.
// Reacts to X (temporal displacement) parameter.
// All drawing is opaque to its own bounds; caller should ensure it sits behind
// the knob layer.
class WarpFieldVisualizer : public juce::Component,
                             private juce::Timer
{
public:
    WarpFieldVisualizer()
    {
        setInterceptsMouseClicks(false, false);
        startTimerHz(30);
    }

    ~WarpFieldVisualizer() override
    {
        stopTimer();
    }

    // Called from GUI thread (parameter change callback).
    void setXParam(float x)   { xParam.store(x,   std::memory_order_relaxed); }
    void setZParam(float z)   { zParam.store(z,   std::memory_order_relaxed); }
    void setMixParam(float m) { mixParam.store(m, std::memory_order_relaxed); }

    void paint(juce::Graphics& g) override
    {
        const float x   = xParam.load(std::memory_order_relaxed);
        const float z   = zParam.load(std::memory_order_relaxed);
        const float mix = mixParam.load(std::memory_order_relaxed);

        const float cx = (float)getWidth()  * 0.5f;
        const float cy = (float)getHeight() * 0.5f;
        const float maxR = std::sqrt(cx * cx + cy * cy) + 50.0f;

        // ── Radial warp lines ────────────────────────────────────────────────
        // Alpha is proportional to x and mix
        const uint8_t lineAlpha = (uint8_t)(juce::jlimit(0.0f, 1.0f, (x + mix) * 0.5f) * 28.0f);
        if (lineAlpha > 0)
        {
            g.setColour(juce::Colour((uint8_t)255, (uint8_t)255, (uint8_t)255, lineAlpha));

            const int numLines = 48;
            const float twoPi  = juce::MathConstants<float>::twoPi;

            for (int l = 0; l < numLines; ++l)
            {
                const float angle = (float)l / (float)numLines * twoPi;

                juce::Path line;
                line.startNewSubPath(cx, cy);

                const int numPts = 24;
                for (int p = 1; p <= numPts; ++p)
                {
                    const float t     = (float)p / (float)numPts;
                    const float r     = t * maxR;
                    const float warp  = x * 18.0f
                                      * std::sin(animPhase * 1.3f + r * 0.012f
                                                  + (float)l * 0.27f)
                                      + z * 8.0f
                                      * std::sin(animPhase * 2.7f + r * 0.025f);
                    const float px = cx + std::cos(angle) * (r + warp);
                    const float py = cy + std::sin(angle) * (r + warp);
                    line.lineTo(px, py);
                }
                g.strokePath(line, juce::PathStrokeType(0.4f));
            }
        }

        // ── Concentric rings ──────────────────────────────────────────────────
        const int numRings = 6;
        for (int ri = 1; ri <= numRings; ++ri)
        {
            const float t     = (float)ri / (float)numRings;
            const float baseR = t * std::min(cx, cy) * 0.88f;
            const float warp  = x * 6.0f * std::sin(animPhase * 0.9f + t * 4.0f);
            const uint8_t ringAlpha = (uint8_t)(juce::jlimit(0.0f, 1.0f,
                                         (x * 0.7f + mix * 0.3f) * (1.0f - t * 0.4f)) * 40.0f);

            if (ringAlpha < 2) continue;

            g.setColour(juce::Colour((uint8_t)255, (uint8_t)255, (uint8_t)255, ringAlpha));
            const float r = baseR + warp;
            g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 0.5f);
        }

        // ── Central pulse dot ─────────────────────────────────────────────────
        const float pulse     = 0.5f + 0.5f * std::sin(animPhase * 2.0f);
        const float dotRadius = (4.0f + x * 8.0f) * (0.7f + 0.3f * pulse);
        const uint8_t dotAlpha = (uint8_t)(juce::jlimit(0.0f, 1.0f,
                                              x * 0.6f + mix * 0.4f) * 180.0f);
        if (dotAlpha > 0)
        {
            // Outer glow
            g.setColour(juce::Colour((uint8_t)0, (uint8_t)255, (uint8_t)255, (uint8_t)(dotAlpha / 4)));
            g.fillEllipse(cx - dotRadius * 2.0f, cy - dotRadius * 2.0f,
                          dotRadius * 4.0f, dotRadius * 4.0f);
            // Core
            g.setColour(juce::Colour((uint8_t)0, (uint8_t)255, (uint8_t)255, dotAlpha));
            g.fillEllipse(cx - dotRadius, cy - dotRadius,
                          dotRadius * 2.0f, dotRadius * 2.0f);
        }
    }

private:
    void timerCallback() override
    {
        animPhase += 0.04f;
        if (animPhase > juce::MathConstants<float>::twoPi * 100.0f)
            animPhase -= juce::MathConstants<float>::twoPi * 100.0f;
        repaint();
    }

    std::atomic<float> xParam   { 0.0f };
    std::atomic<float> zParam   { 0.0f };
    std::atomic<float> mixParam { 0.5f };
    float animPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WarpFieldVisualizer)
};
