#include "PluginEditor.h"
#include <BinaryData.h>

static constexpr int   EDITOR_W          = 700;
static constexpr int   EDITOR_H          = 770;
static constexpr int   KNOB_SIZE         = 60;
static constexpr float TIME_SLIDER_Y_FRAC = 0.44f;
static constexpr float NOW_BUTTON_Y_FRAC  = 0.52f;

// ─── Constructor ─────────────────────────────────────────────────────────────
AlcubierreEditor::AlcubierreEditor(AlcubierreProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      attTime     (p.apvts, "time_position",       timeSlider),
      attY        (p.apvts, "warp_density",         knobY.getSlider()),
      attZ        (p.apvts, "dimensional_scatter",  knobZ.getSlider()),
      attGrainSize(p.apvts, "grain_size_ms",        knobGrainSize.getSlider()),
      attFeedback (p.apvts, "feedback",             knobFeedback.getSlider()),
      attDelta    (p.apvts, "delta",                knobDelta.getSlider())
{
    setSize(EDITOR_W, EDITOR_H);
    setResizable(false, false);

    bgImage = juce::ImageCache::getFromMemory(BinaryData::alcubierre_bg_png,
                                              BinaryData::alcubierre_bg_pngSize);

    addAndMakeVisible(warpField);
    warpField.toBack();

    addAndMakeVisible(timeSlider);
    addAndMakeVisible(knobY);
    addAndMakeVisible(knobZ);
    addAndMakeVisible(knobGrainSize);
    addAndMakeVisible(knobFeedback);
    addAndMakeVisible(knobDelta);
    addAndMakeVisible(nowButton);

    // Seed warp-field visualizer with initial param values
    {
        const float tp = p.apvts.getRawParameterValue("time_position")->load();
        warpField.setXParam(std::abs(tp));
        warpField.setZParam(p.apvts.getRawParameterValue("dimensional_scatter")->load());
        warpField.setMixParam(p.apvts.getRawParameterValue("delta")->load());
    }

    // Wire visualizer to live parameter changes
    timeSlider.onValueChange = [this]
    {
        warpField.setXParam(std::abs((float)timeSlider.getValue()));
    };
    knobZ.onValueChange = [this]
    {
        warpField.setZParam((float)knobZ.getValue());
    };
    knobDelta.onValueChange = [this]
    {
        warpField.setMixParam((float)knobDelta.getValue());
        repaint();  // redraw DELTA triangle
    };

    // NOW button: flash visualizer and start return ramp
    nowButton.onClick = [this]
    {
        // Brief max-intensity flash
        warpField.setXParam(1.0f);
        warpField.setZParam(1.0f);

        // Capture start values for interpolation
        nowRampStartY     = (float)knobY.getValue();
        nowRampStartZ     = (float)knobZ.getValue();
        nowRampStartGrain = (float)knobGrainSize.getValue();
        nowRampStartFeed  = (float)knobFeedback.getValue();
        nowRampStartTime  = (float)timeSlider.getValue();

        nowRampProgress = 0.0f;
        nowRampActive   = true;
    };

    startTimerHz(30);
}

AlcubierreEditor::~AlcubierreEditor()
{
    stopTimer();
}

// ─── NOW ramp timer ───────────────────────────────────────────────────────────
void AlcubierreEditor::timerCallback()
{
    if (!nowRampActive)
        return;

    nowRampProgress += (1.0f / 30.0f) / 1.5f;  // 30 Hz over 1.5 seconds

    if (nowRampProgress >= 1.0f)
    {
        nowRampProgress = 1.0f;
        nowRampActive   = false;
    }

    // Smoothstep easing
    const float t = nowRampProgress * nowRampProgress * (3.0f - 2.0f * nowRampProgress);

    // Helper: set a parameter by its denormalised value
    auto setParam = [&](const char* id, float startVal, float targetVal)
    {
        const float v = startVal + (targetVal - startVal) * t;
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(
                processor.apvts.getParameter(id)))
            rp->setValueNotifyingHost(rp->convertTo0to1(v));
    };

    setParam("warp_density",        nowRampStartY,     0.0f);
    setParam("dimensional_scatter", nowRampStartZ,     0.0f);
    setParam("grain_size_ms",       nowRampStartGrain, 80.0f);
    setParam("feedback",            nowRampStartFeed,  0.0f);
    setParam("time_position",       nowRampStartTime,  0.0f);
    // NOTE: "delta" (mix) is intentionally NOT moved during NOW ramp
}

// ─── Layout ──────────────────────────────────────────────────────────────────
void AlcubierreEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    warpField.setBounds(0, 0, w, h);

    // TIME slider: 60% of width, centered
    const int sliderW = (int)(w * 0.60f);
    const int sliderH = 44;
    const int sliderX = (w - sliderW) / 2;
    const int sliderY = (int)(h * TIME_SLIDER_Y_FRAC) - sliderH / 2;
    timeSlider.setBounds(sliderX, sliderY, sliderW, sliderH);

    // NOW button: centered below TIME slider
    const int btnW = 84, btnH = 22;
    const int btnX = (w - btnW) / 2;
    const int btnY = (int)(h * NOW_BUTTON_Y_FRAC) - btnH / 2;
    nowButton.setBounds(btnX, btnY, btnW, btnH);

    // Remaining knobs
    struct KP { AlcubierreKnob* k; float xf, yf; };
    KP kps[] = {
        { &knobY,         0.50f, 0.32f },
        { &knobZ,         0.72f, 0.38f },
        { &knobGrainSize, 0.28f, 0.60f },
        { &knobFeedback,  0.72f, 0.60f },
        { &knobDelta,     0.50f, 0.78f },
    };

    const int half = KNOB_SIZE / 2;
    for (auto& kp : kps)
    {
        const int cx = juce::roundToInt(kp.xf * (float)w);
        const int cy = juce::roundToInt(kp.yf * (float)h);
        kp.k->setBounds(cx - half, cy - half, KNOB_SIZE, KNOB_SIZE);
    }
}

// ─── Paint ────────────────────────────────────────────────────────────────────
void AlcubierreEditor::paint(juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();

    g.fillAll(juce::Colours::black);

    if (bgImage.isValid())
        g.drawImage(bgImage,
                    0, 0, w, h,
                    0, 0, bgImage.getWidth(), bgImage.getHeight());

    // ── DELTA triangle at top-center ─────────────────────────────────────────
    // Downward-pointing equilateral triangle, brightness = DELTA value
    const float delta = processor.apvts.getRawParameterValue("delta")->load();
    if (delta > 0.005f)
    {
        const float triSide   = 40.0f;
        const float triH      = triSide * std::sqrt(3.0f) / 2.0f;
        const float triCentX  = (float)w * 0.5f;
        const float triTopY   = 10.0f;

        juce::Path tri;
        tri.startNewSubPath(triCentX - triSide * 0.5f, triTopY);         // top-left
        tri.lineTo        (triCentX + triSide * 0.5f, triTopY);          // top-right
        tri.lineTo        (triCentX,                  triTopY + triH);   // apex (down)
        tri.closeSubPath();

        // Soft glow halos (expanded triangle outlines)
        for (int i = 3; i >= 1; --i)
        {
            const float ex = (float)i * 4.0f;
            juce::Path halo;
            halo.startNewSubPath(triCentX - triSide * 0.5f - ex, triTopY - ex);
            halo.lineTo        (triCentX + triSide * 0.5f + ex, triTopY - ex);
            halo.lineTo        (triCentX,                        triTopY + triH + ex);
            halo.closeSubPath();
            g.setColour(juce::Colours::white.withAlpha(0.07f * delta / (float)i));
            g.fillPath(halo);
        }

        // Filled triangle — fully white at delta=1
        g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.0f, 1.0f, delta)));
        g.fillPath(tri);
    }

    // ── Title ─────────────────────────────────────────────────────────────────
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                         18.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xff505050));
    g.drawText("ALCUBIERRE", 20, 20, 300, 24,
               juce::Justification::centredLeft, false);

    // ── Bottom credit ─────────────────────────────────────────────────────────
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
