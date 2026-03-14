#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "AlcubierreKnob.h"
#include "WarpFieldVisualizer.h"

// ─── AlcubierreEditor ─────────────────────────────────────────────────────────
// 700 × 770 px portrait window.
// The alcubierre_bg.png is drawn full-bleed; all 7 controls appear as glowing
// LED dots at hard-coded fractional positions on top of the image.
class AlcubierreEditor : public juce::AudioProcessorEditor
{
public:
    explicit AlcubierreEditor(AlcubierreProcessor&);
    ~AlcubierreEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AlcubierreProcessor& processor;

    // ── Warp-field overlay (transparent, sits above background) ───────────────
    WarpFieldVisualizer warpField;

    // ── LED knobs ─────────────────────────────────────────────────────────────
    AlcubierreKnob knobX         { "X",   juce::Colour(0xff00C8F0) }; // cyan
    AlcubierreKnob knobY         { "Y",   juce::Colour(0xff00C8F0) }; // cyan
    AlcubierreKnob knobZ         { "Z",   juce::Colour(0xff00C8F0) }; // cyan
    AlcubierreKnob knobAO        { "\xce\xa9", juce::Colour(0xffF0C030) }; // amber Ω
    AlcubierreKnob knobGrainSize { "GRN", juce::Colour(0xffFF4040) }; // red
    AlcubierreKnob knobFeedback  { "FB",  juce::Colour(0xffFF4040) }; // red
    AlcubierreKnob knobMix       { "MIX", juce::Colours::white      }; // white

    // ── APVTS attachments ─────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState::SliderAttachment attX, attY, attZ, attAO;
    juce::AudioProcessorValueTreeState::SliderAttachment attGrainSize, attFeedback, attMix;

    // ── Background image (loaded from BinaryData) ─────────────────────────────
    juce::Image bgImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlcubierreEditor)
};
