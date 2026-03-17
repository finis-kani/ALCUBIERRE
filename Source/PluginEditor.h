#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "AlcubierreKnob.h"
#include "WarpFieldVisualizer.h"

// ─── AlcubierreTimeSlider ─────────────────────────────────────────────────────
// Horizontal TIME slider with frosted track, tick marks, and glowing cyan thumb.
class AlcubierreTimeSlider : public juce::Slider
{
public:
    AlcubierreTimeSlider()
    {
        setSliderStyle(juce::Slider::LinearHorizontal);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setDoubleClickReturnValue(true, 0.0);
    }

    void paint(juce::Graphics& g) override
    {
        const float w  = (float)getWidth();
        const float h  = (float)getHeight();
        const float cy = h * 0.5f - 6.0f;  // track center (leave room for labels below)

        const float margin    = 18.0f;
        const float trackLen  = w - margin * 2.0f;

        // Normalise value from [-1, 1] to [0, 1]
        const float norm     = (float)juce::jmap(getValue(), getMinimum(), getMaximum(), 0.0, 1.0);
        const float thumbX   = margin + norm * trackLen;

        // ── Track line ────────────────────────────────────────────────────────
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.20f));
        g.drawLine(margin, cy, margin + trackLen, cy, 1.5f);

        // ── Tick marks: PAST (left), NOW (center), FUTURE (right) ─────────────
        const float ticks[3] = { margin,
                                  margin + trackLen * 0.5f,
                                  margin + trackLen };
        for (float tx : ticks)
        {
            g.setColour(juce::Colour(0xff00C8F0).withAlpha(0.45f));
            g.drawLine(tx, cy - 5.0f, tx, cy + 5.0f, 1.0f);
        }

        // ── Glowing cyan LED thumb dot ─────────────────────────────────────────
        const float thumbR = 6.0f;
        for (int i = 4; i >= 1; --i)
        {
            const float gr = thumbR + (float)i * 4.5f;
            const float ga = 0.08f / (float)i;
            g.setColour(juce::Colour(0xff00C8F0).withAlpha(ga));
            g.fillEllipse(thumbX - gr, cy - gr, gr * 2.0f, gr * 2.0f);
        }
        g.setColour(juce::Colour(0xff00C8F0).withAlpha(0.95f));
        g.fillEllipse(thumbX - thumbR, cy - thumbR, thumbR * 2.0f, thumbR * 2.0f);

        // ── Labels ────────────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                             8.0f, juce::Font::plain));
        g.setColour(juce::Colour(0xff00C8F0).withAlpha(0.65f));

        const float labelY = cy + thumbR + 4.0f;
        g.drawText("PAST",    (int)(ticks[0] - 20), (int)labelY, 40, 12, juce::Justification::centred, false);
        g.drawText("PRESENT", (int)(ticks[1] - 28), (int)labelY, 56, 12, juce::Justification::centred, false);
        g.drawText("FUTURE",  (int)(ticks[2] - 20), (int)labelY, 40, 12, juce::Justification::centred, false);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlcubierreTimeSlider)
};

// ─── NowButton ────────────────────────────────────────────────────────────────
// Small glowing pill button labelled "NOW". When clicked, ramps all warped
// parameters back to neutral over 1.5 seconds.
class NowButton : public juce::Button
{
public:
    NowButton() : juce::Button("NOW") {}

    void paintButton(juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        const float w = (float)getWidth();
        const float h = (float)getHeight();
        const float a = isDown ? 1.0f : (isHighlighted ? 0.85f : 0.6f);

        // Pill fill
        g.setColour(juce::Colours::white.withAlpha(0.10f * a));
        g.fillRoundedRectangle(0.0f, 0.0f, w, h, h * 0.5f);

        // Pill border glow
        g.setColour(juce::Colours::white.withAlpha(0.45f * a));
        g.drawRoundedRectangle(0.5f, 0.5f, w - 1.0f, h - 1.0f, h * 0.5f - 0.5f, 1.0f);

        // Outer glow halo
        g.setColour(juce::Colours::white.withAlpha(0.07f * a));
        g.drawRoundedRectangle(-2.0f, -2.0f, w + 4.0f, h + 4.0f, h * 0.5f + 2.0f, 2.5f);

        // Label
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                             10.0f, juce::Font::plain));
        g.setColour(juce::Colours::white.withAlpha(0.90f * a));
        g.drawText("NOW", 0, 0, (int)w, (int)h, juce::Justification::centred, false);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NowButton)
};

// ─── AlcubierreEditor ─────────────────────────────────────────────────────────
// 700 × 790 px portrait window.
class AlcubierreEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit AlcubierreEditor(AlcubierreProcessor&);
    ~AlcubierreEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;  // drives NOW ramp

    AlcubierreProcessor& processor;

    // ── Warp-field overlay ────────────────────────────────────────────────────
    WarpFieldVisualizer warpField;

    // ── TIME slider (replaces X + Ω knobs) ───────────────────────────────────
    AlcubierreTimeSlider timeSlider;

    // ── LED knobs ─────────────────────────────────────────────────────────────
    AlcubierreKnob knobY         { "Y",     juce::Colour(0xff00C8F0) }; // cyan
    AlcubierreKnob knobZ         { "Z",     juce::Colour(0xff00C8F0) }; // cyan
    AlcubierreKnob knobGrainSize { "GRN",   juce::Colour(0xffFF4040) }; // red
    AlcubierreKnob knobFeedback  { "FB",    juce::Colour(0xffFF4040) }; // red
    AlcubierreKnob knobDelta     { "DELTA", juce::Colours::white      }; // white

    // ── NOW button ────────────────────────────────────────────────────────────
    NowButton nowButton;

    // ── APVTS attachments ─────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState::SliderAttachment attTime;
    juce::AudioProcessorValueTreeState::SliderAttachment attY, attZ;
    juce::AudioProcessorValueTreeState::SliderAttachment attGrainSize, attFeedback, attDelta;

    // ── NOW ramp state ────────────────────────────────────────────────────────
    bool  nowRampActive      = false;
    float nowRampProgress    = 0.0f;
    float nowRampStartY      = 0.0f;
    float nowRampStartZ      = 0.0f;
    float nowRampStartGrain  = 80.0f;
    float nowRampStartFeed   = 0.0f;
    float nowRampStartTime   = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlcubierreEditor)
};
