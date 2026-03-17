#include "PluginEditor.h"

static constexpr int   EDITOR_W          = 700;
static constexpr int   EDITOR_H          = 790;
static constexpr float TIME_SLIDER_Y_FRAC = 0.46f;
static constexpr float NOW_BUTTON_Y_FRAC  = 0.55f;

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
        constexpr float PITCH_OCTAVES = 1.5f;
        warpField.setPitchParam(std::pow(2.0f, tp * PITCH_OCTAVES) - 1.0f);
    }

    // Wire visualizer to live parameter changes
    timeSlider.onValueChange = [this]()
    {
        const float tp = (float)timeSlider.getValue();
        warpField.setXParam(std::abs(tp));
        constexpr float PITCH_OCTAVES = 1.5f;
        warpField.setPitchParam(std::pow(2.0f, tp * PITCH_OCTAVES) - 1.0f);
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

    // TIME slider: 62% of width, centered
    const int sliderW = (int)(w * 0.62f);
    const int sliderH = 44;
    const int sliderX = (w - sliderW) / 2;
    const int sliderY = (int)(h * TIME_SLIDER_Y_FRAC) - 22;
    timeSlider.setBounds(sliderX, sliderY, sliderW, sliderH);

    // NOW button: centered below TIME slider
    const int btnW = 84, btnH = 22;
    const int btnX = (w - btnW) / 2;
    const int btnY = (int)(h * NOW_BUTTON_Y_FRAC) - btnH / 2;
    nowButton.setBounds(btnX, btnY, btnW, btnH);

    // Knob layout: Y and Z are 80×80, others are 60×60
    struct KP { juce::Component* k; float xf, yf; int size; };
    KP kps[] = {
        { &knobY,         0.30f, 0.33f, 80 },
        { &knobZ,         0.70f, 0.33f, 80 },
        { &knobGrainSize, 0.25f, 0.65f, 60 },
        { &knobFeedback,  0.75f, 0.65f, 60 },
        { &knobDelta,     0.50f, 0.80f, 60 },
    };

    for (auto& kp : kps)
    {
        const int cx = juce::roundToInt(kp.xf * (float)w);
        const int cy = juce::roundToInt(kp.yf * (float)h);
        const int half = kp.size / 2;
        kp.k->setBounds(cx - half, cy - half, kp.size, kp.size);
    }
}

// ─── Paint ────────────────────────────────────────────────────────────────────
void AlcubierreEditor::paint(juce::Graphics& g)
{
    const float W  = (float)getWidth();
    const float H  = (float)getHeight();
    const float cx = W * 0.5f;

    // 1. Background fill
    g.fillAll(juce::Colour(0xff0A0A0A));

    // 2. Carapace path (organic shield/heart form)
    juce::Path carapace;
    carapace.startNewSubPath(cx, H * 0.05f);
    carapace.cubicTo(W*0.72f, H*0.02f,  W*0.92f, H*0.11f,  W*0.91f, H*0.28f);
    carapace.cubicTo(W*0.90f, H*0.44f,  W*0.84f, H*0.52f,  W*0.82f, H*0.60f);
    carapace.cubicTo(W*0.78f, H*0.72f,  W*0.64f, H*0.84f,  cx,      H*0.97f);
    carapace.cubicTo(W*0.36f, H*0.84f,  W*0.22f, H*0.72f,  W*0.18f, H*0.60f);
    carapace.cubicTo(W*0.16f, H*0.52f,  W*0.10f, H*0.44f,  W*0.09f, H*0.28f);
    carapace.cubicTo(W*0.08f, H*0.11f,  W*0.28f, H*0.02f,  cx,      H*0.05f);
    carapace.closeSubPath();

    // 3. Fill carapace with radial gradient: centre bright, edges dark ivory
    {
        juce::ColourGradient grad(
            juce::Colour(0xffF0EDE8), cx, H * 0.3f,
            juce::Colour(0xffCCCAC4), cx + W * 0.45f, H * 0.3f,
            true);
        g.setGradientFill(grad);
        g.fillPath(carapace);
    }

    // 4. Contour lines: 8 concentric inward steps from carapace shape
    for (int i = 1; i <= 8; ++i)
    {
        const float scale = 1.0f - (float)i * 0.025f;
        juce::Path contour = carapace;
        contour.applyTransform(juce::AffineTransform::scale(scale, scale, cx, H * 0.3f));
        g.setColour(juce::Colour(0xffAAAAAA).withAlpha(0.12f));
        g.strokePath(contour, juce::PathStrokeType(0.5f));
    }

    // 5. Central spine: thin bezier from top to bottom with subtle S-curve
    {
        juce::Path spine;
        spine.startNewSubPath(cx, H * 0.08f);
        spine.cubicTo(cx + 15.0f, H * 0.30f,
                      cx - 15.0f, H * 0.60f,
                      cx,         H * 0.90f);
        g.setColour(juce::Colour(0xff111111));
        g.strokePath(spine, juce::PathStrokeType(30.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 6. Recessed knob wells at Y (30%, 33%) and Z (70%, 33%)
    const float wellPositions[2][2] = {
        { W * 0.30f, H * 0.33f },
        { W * 0.70f, H * 0.33f }
    };
    for (auto& wp : wellPositions)
    {
        const float wx = wp[0], wy = wp[1];

        // Dark fill
        g.setColour(juce::Colour(0xff131313));
        g.fillEllipse(wx - 52.0f, wy - 52.0f, 104.0f, 104.0f);

        // 3 concentric ring lines
        const float ringR[3]  = { 48.0f, 42.0f, 34.0f };
        const juce::Colour ringC[3] = {
            juce::Colour(0xff1C1C1C),
            juce::Colour(0xff232323),
            juce::Colour(0xff2C2C2C)
        };
        for (int ri = 0; ri < 3; ++ri)
            g.setColour(ringC[ri]),
            g.drawEllipse(wx - ringR[ri], wy - ringR[ri],
                          ringR[ri] * 2.0f, ringR[ri] * 2.0f, 1.5f);

        // 24 tick marks at rim radius 54
        g.setColour(juce::Colour(0xff444444));
        for (int ti = 0; ti < 24; ++ti)
        {
            const float angle = (float)ti / 24.0f * juce::MathConstants<float>::twoPi;
            const float cosA = std::cos(angle), sinA = std::sin(angle);
            g.drawLine(wx + cosA * 49.0f, wy + sinA * 49.0f,
                       wx + cosA * 54.0f, wy + sinA * 54.0f, 1.0f);
        }
    }

    // 7. DELTA triangle at top-center (downward-pointing)
    const float delta = processor.apvts.getRawParameterValue("delta")->load();
    if (delta > 0.005f)
    {
        const float triSide  = 40.0f;
        const float triH     = triSide * std::sqrt(3.0f) / 2.0f;
        const float triCentX = cx;
        const float triTopY  = (float)getHeight() * 0.04f;

        juce::Path tri;
        tri.startNewSubPath(triCentX - triSide * 0.5f, triTopY);
        tri.lineTo        (triCentX + triSide * 0.5f, triTopY);
        tri.lineTo        (triCentX,                  triTopY + triH);
        tri.closeSubPath();

        // Soft glow halos
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

        g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.0f, 1.0f, delta)));
        g.fillPath(tri);
    }

    // 8. Two cyan LED dots
    const float cyanPos[2][2] = {
        { W * 0.14f, H * 0.14f },
        { W * 0.86f, H * 0.14f }
    };
    for (auto& cp : cyanPos)
    {
        const float lx = cp[0], ly = cp[1];
        // Glow halo
        g.setColour(juce::Colour(0xff00C8F0).withAlpha(0.15f));
        g.fillEllipse(lx - 10.0f, ly - 10.0f, 20.0f, 20.0f);
        g.setColour(juce::Colour(0xff00C8F0).withAlpha(0.08f));
        g.fillEllipse(lx - 15.0f, ly - 15.0f, 30.0f, 30.0f);
        // Core dot
        g.setColour(juce::Colour(0xff00C8F0).withAlpha(0.80f));
        g.fillEllipse(lx - 4.0f, ly - 4.0f, 8.0f, 8.0f);
    }

    // 9. Red LED dot at top-center
    {
        const float lx = cx, ly = H * 0.07f;
        g.setColour(juce::Colour(0xffFF3030).withAlpha(0.15f));
        g.fillEllipse(lx - 8.0f, ly - 8.0f, 16.0f, 16.0f);
        g.setColour(juce::Colour(0xffFF3030).withAlpha(0.70f));
        g.fillEllipse(lx - 3.0f, ly - 3.0f, 6.0f, 6.0f);
    }

    // 10. Title
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                         13.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xff383838));
    g.drawText("ALCUBIERRE  V3",
               (int)(W * 0.10f), (int)(H * 0.07f) - 7,
               300, 14,
               juce::Justification::centredLeft, false);

    // 11. Bottom credit
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                         8.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xff555555));
    g.drawText("FINIS MUSICAE CORP",
               0, (int)(H * 0.975f) - 8, (int)W, 12,
               juce::Justification::centred, false);
}

// ─── Factory ─────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* AlcubierreProcessor::createEditor()
{
    return new AlcubierreEditor(*this);
}
