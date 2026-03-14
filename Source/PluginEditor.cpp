#include "PluginEditor.h"
#include <BinaryData.h>

static constexpr int EDITOR_W = 700;
static constexpr int EDITOR_H = 770;
static constexpr int KNOB_SIZE = 60;

// LED centre positions as fractions of (EDITOR_W, EDITOR_H).
// Order: X, Y, Z, AO, GRAIN, FEEDBACK, MIX
struct LedFrac { float xf, yf; };
static constexpr LedFrac LED_POSITIONS[7] =
{
    { 0.28f, 0.38f }, // X  — Temporal
    { 0.50f, 0.32f }, // Y  — Density
    { 0.72f, 0.38f }, // Z  — Scatter
    { 0.50f, 0.58f }, // AO — AlphaOmega
    { 0.28f, 0.60f }, // GRAIN
    { 0.72f, 0.60f }, // FEEDBACK
    { 0.50f, 0.78f }, // MIX
};

// ─── Constructor ─────────────────────────────────────────────────────────────
AlcubierreEditor::AlcubierreEditor(AlcubierreProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      attX        (p.apvts, "temporal_displacement", knobX.getSlider()),
      attY        (p.apvts, "warp_density",           knobY.getSlider()),
      attZ        (p.apvts, "dimensional_scatter",    knobZ.getSlider()),
      attAO       (p.apvts, "alpha_omega",            knobAO.getSlider()),
      attGrainSize(p.apvts, "grain_size_ms",          knobGrainSize.getSlider()),
      attFeedback (p.apvts, "feedback",               knobFeedback.getSlider()),
      attMix      (p.apvts, "mix",                    knobMix.getSlider())
{
    setSize(EDITOR_W, EDITOR_H);
    setResizable(false, false);

    // Load background image
    bgImage = juce::ImageCache::getFromMemory(BinaryData::alcubierre_bg_png,
                                              BinaryData::alcubierre_bg_pngSize);

    // Add warp-field visualizer behind everything else
    addAndMakeVisible(warpField);
    warpField.toBack();

    // Add LED knobs
    addAndMakeVisible(knobX);
    addAndMakeVisible(knobY);
    addAndMakeVisible(knobZ);
    addAndMakeVisible(knobAO);
    addAndMakeVisible(knobGrainSize);
    addAndMakeVisible(knobFeedback);
    addAndMakeVisible(knobMix);

    // Sync visualizer with initial parameter values
    warpField.setXParam(p.apvts.getRawParameterValue("temporal_displacement")->load());
    warpField.setZParam(p.apvts.getRawParameterValue("dimensional_scatter")->load());
    warpField.setMixParam(p.apvts.getRawParameterValue("mix")->load());

    // Wire up visualizer updates on parameter change
    knobX.onValueChange = [this]
    {
        warpField.setXParam((float)knobX.getValue());
    };
    knobZ.onValueChange = [this]
    {
        warpField.setZParam((float)knobZ.getValue());
    };
    knobMix.onValueChange = [this]
    {
        warpField.setMixParam((float)knobMix.getValue());
    };
}

// ─── Layout ──────────────────────────────────────────────────────────────────
void AlcubierreEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    const int half = KNOB_SIZE / 2;

    warpField.setBounds(0, 0, w, h);

    AlcubierreKnob* knobs[7] =
    {
        &knobX, &knobY, &knobZ, &knobAO,
        &knobGrainSize, &knobFeedback, &knobMix
    };

    for (int i = 0; i < 7; ++i)
    {
        const int cx = juce::roundToInt(LED_POSITIONS[i].xf * (float)w);
        const int cy = juce::roundToInt(LED_POSITIONS[i].yf * (float)h);
        knobs[i]->setBounds(cx - half, cy - half, KNOB_SIZE, KNOB_SIZE);
    }
}

// ─── Paint ────────────────────────────────────────────────────────────────────
void AlcubierreEditor::paint(juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();

    // White fallback if image fails to load
    g.fillAll(juce::Colours::white);

    // Draw PNG background scaled to fill the window
    if (bgImage.isValid())
        g.drawImage(bgImage,
                    0, 0, w, h,
                    0, 0, bgImage.getWidth(), bgImage.getHeight());

    // ── Title (top-left, subtle dark gray) ────────────────────────────────────
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                         18.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xff505050));
    g.drawText("ALCUBIERRE", 20, 20, 300, 24,
               juce::Justification::centredLeft, false);

    // ── Bottom credit (centred, small gray) ───────────────────────────────────
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                         9.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xff808080));
    g.drawText("FINIS MUSICAE CORP",
               0, h - 20, w, 16,
               juce::Justification::centred, false);
}

// ─── Factory ─────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* AlcubierreProcessor::createEditor()
{
    return new AlcubierreEditor(*this);
}
