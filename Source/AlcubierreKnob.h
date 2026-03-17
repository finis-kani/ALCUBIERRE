#pragma once

#include <JuceHeader.h>

// ─── AlcubierreKnob ───────────────────────────────────────────────────────────
// Rotary slider rendered as a glowing LED dot with subtle per-knob flicker.
// Inherits juce::Slider (for SliderAttachment) and juce::Timer (for flicker).
class AlcubierreKnob : public juce::Slider, private juce::Timer
{
public:
    AlcubierreKnob(const juce::String& labelText, juce::Colour colour)
        : ledLabel(labelText), ledColour(colour)
    {
        setSliderStyle(juce::Slider::Rotary);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setDoubleClickReturnValue(true, 0.5);

        // Randomise per-knob flicker phase so they drift apart visually
        flickerPhase = juce::Random::getSystemRandom().nextFloat()
                     * juce::MathConstants<float>::twoPi;
        flickerRate  = 0.18f
                     + (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 0.04f;

        startTimerHz(20);
    }

    ~AlcubierreKnob() override { stopTimer(); }

    // Kept for backward-compat with SliderAttachment construction.
    juce::Slider& getSlider() { return *this; }

    void paint(juce::Graphics& g) override
    {
        const float w  = (float)getWidth();
        const float h  = (float)getHeight();
        const float cx = w * 0.5f;
        const float cy = h * 0.5f;

        const float value = (float)juce::jmap(getValue(),
                                               getMinimum(), getMaximum(),
                                               0.0, 1.0);

        const float baseDotRadius = 8.0f;
        const float glowMaxRadius = 27.0f;
        const float wellRadius = w * 0.5f - 4.0f;

        // ── Recessed knob well ────────────────────────────────────────────────
        // Dark fill
        g.setColour(juce::Colour(0xff131313));
        g.fillEllipse(cx - wellRadius, cy - wellRadius,
                      wellRadius * 2.0f, wellRadius * 2.0f);
        // 3 inner concentric rings
        const float ringRadii[3]    = { wellRadius - 4.0f, wellRadius - 9.0f, wellRadius - 16.0f };
        const juce::Colour ringCols[3] = {
            juce::Colour(0xff1A1A1A),
            juce::Colour(0xff222222),
            juce::Colour(0xff2A2A2A)
        };
        for (int ri = 0; ri < 3; ++ri)
        {
            const float rr = ringRadii[ri];
            if (rr <= 0.0f) continue;
            g.setColour(ringCols[ri]);
            g.drawEllipse(cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 1.0f);
        }
        // 16 tick marks around outer rim
        {
            const float tickOuter = wellRadius + 3.0f;
            const float tickInner = tickOuter - 4.0f;
            g.setColour(juce::Colour(0xff3A3A3A));
            for (int ti = 0; ti < 16; ++ti)
            {
                const float angle = (float)ti / 16.0f * juce::MathConstants<float>::twoPi;
                const float cosA  = std::cos(angle);
                const float sinA  = std::sin(angle);
                g.drawLine(cx + cosA * tickInner, cy + sinA * tickInner,
                           cx + cosA * tickOuter, cy + sinA * tickOuter, 1.0f);
            }
        }

        // Flicker: gentle breath + harmonic shimmer
        const float flickerAlpha = 1.0f
                                 + 0.06f * std::sin(flickerPhase)
                                 + 0.03f * std::sin(flickerPhase * 2.73f);

        // ── Glow halo ─────────────────────────────────────────────────────────
        for (int i = 5; i >= 1; --i)
        {
            const float radius = baseDotRadius
                               + (float)i * (glowMaxRadius / 5.0f) * value;
            const float alpha  = juce::jlimit(0.0f, 1.0f,
                (0.15f * value) / (float)i * flickerAlpha);
            g.setColour(ledColour.withAlpha(alpha));
            g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        }

        // ── Core LED dot ──────────────────────────────────────────────────────
        const float coreAlpha = juce::jlimit(0.0f, 1.0f,
            (0.3f + 0.7f * value) * flickerAlpha);
        g.setColour(ledColour.withAlpha(coreAlpha));
        g.fillEllipse(cx - baseDotRadius, cy - baseDotRadius,
                      baseDotRadius * 2.0f, baseDotRadius * 2.0f);

        // ── Value arc (7 o'clock → 5 o'clock, 270° sweep) ────────────────────
        const float rotStart  = juce::MathConstants<float>::pi * 1.25f;
        const float rotEnd    = juce::MathConstants<float>::pi * 2.75f;
        const float arcAngle  = rotStart + value * (rotEnd - rotStart);
        const float arcRadius = baseDotRadius + 6.0f;

        {
            juce::Path track;
            track.addCentredArc(cx, cy, arcRadius, arcRadius,
                                0.0f, rotStart, rotEnd, true);
            g.setColour(ledColour.withAlpha(0.18f));
            g.strokePath(track, juce::PathStrokeType(1.5f));
        }

        if (value > 0.01f)
        {
            juce::Path arc;
            arc.addCentredArc(cx, cy, arcRadius, arcRadius,
                              0.0f, rotStart, arcAngle, true);
            g.setColour(ledColour.withAlpha(0.85f));
            g.strokePath(arc, juce::PathStrokeType(2.0f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // ── Label above LED ───────────────────────────────────────────────────
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                             9.0f, juce::Font::plain));
        g.setColour(ledColour.withAlpha(0.82f));
        g.drawText(ledLabel,
                   0, (int)(cy - baseDotRadius - 17.0f),
                   (int)w, 13,
                   juce::Justification::centred, false);

        // ── Value readout below LED ───────────────────────────────────────────
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                             8.0f, juce::Font::plain));
        g.setColour(ledColour.withAlpha(0.55f));
        g.drawText(juce::String(getValue(), 2),
                   0, (int)(cy + baseDotRadius + 3.0f),
                   (int)w, 12,
                   juce::Justification::centred, false);
    }

private:
    void timerCallback() override
    {
        flickerPhase += flickerRate;
        if (flickerPhase > juce::MathConstants<float>::twoPi * 100.0f)
            flickerPhase -= juce::MathConstants<float>::twoPi * 100.0f;
        repaint();
    }

    juce::String ledLabel;
    juce::Colour ledColour;
    float flickerPhase = 0.0f;
    float flickerRate  = 0.18f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlcubierreKnob)
};
