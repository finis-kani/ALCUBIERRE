#pragma once

#include <JuceHeader.h>

// ─── AlcubierreKnob ───────────────────────────────────────────────────────────
// Rotary slider rendered as a glowing LED dot overlaid on the background image.
// Inherits juce::Slider so it can be passed directly to SliderAttachment.
// paint() is fully overridden — no LookAndFeel drawing occurs.
class AlcubierreKnob : public juce::Slider
{
public:
    AlcubierreKnob(const juce::String& labelText, juce::Colour colour)
        : ledLabel(labelText), ledColour(colour)
    {
        setSliderStyle(juce::Slider::Rotary);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setDoubleClickReturnValue(true, 0.5);
    }

    // Kept for backward-compat with SliderAttachment construction in PluginEditor.
    juce::Slider& getSlider() { return *this; }

    void paint(juce::Graphics& g) override
    {
        const float w  = (float)getWidth();
        const float h  = (float)getHeight();
        const float cx = w * 0.5f;
        const float cy = h * 0.5f;

        // Normalise value 0..1
        const float value = (float)juce::jmap(getValue(),
                                               getMinimum(), getMaximum(),
                                               0.0, 1.0);

        const float baseDotRadius = 8.0f;
        const float glowMaxRadius = 27.0f; // full glow extent at value=1

        // ── Glow halo (outer to inner, semi-transparent rings) ────────────────
        for (int i = 5; i >= 1; --i)
        {
            const float radius = baseDotRadius
                               + (float)i * (glowMaxRadius / 5.0f) * value;
            const float alpha  = (0.15f * value) / (float)i;
            g.setColour(ledColour.withAlpha(juce::jlimit(0.0f, 1.0f, alpha)));
            g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        }

        // ── Core LED dot ──────────────────────────────────────────────────────
        g.setColour(ledColour.withAlpha(0.3f + 0.7f * value));
        g.fillEllipse(cx - baseDotRadius, cy - baseDotRadius,
                      baseDotRadius * 2.0f, baseDotRadius * 2.0f);

        // ── Value arc (7 o'clock → 5 o'clock, 270° sweep) ────────────────────
        const float rotStart  = juce::MathConstants<float>::pi * 1.25f;
        const float rotEnd    = juce::MathConstants<float>::pi * 2.75f;
        const float arcAngle  = rotStart + value * (rotEnd - rotStart);
        const float arcRadius = baseDotRadius + 6.0f;

        // Background track
        {
            juce::Path track;
            track.addCentredArc(cx, cy, arcRadius, arcRadius,
                                0.0f, rotStart, rotEnd, true);
            g.setColour(ledColour.withAlpha(0.18f));
            g.strokePath(track, juce::PathStrokeType(1.5f));
        }

        // Filled portion
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
    juce::String ledLabel;
    juce::Colour ledColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlcubierreKnob)
};
