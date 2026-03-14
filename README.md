# ALCUBIERRE

**Temporally Displaced Extraterrestrial Frequency Language**

A live audio effect that stores a 10-second ring buffer of the input signal and allows the user to warp between past, present, and an inferred future using granular time-stretching, glitching, and spectral interpolation.

---

## Concept

ALCUBIERRE bends linear time in audio. It continuously captures your signal into a circular ring buffer, then re-reads it through a granular engine that can reach anywhere in those 10 seconds. Multiple overlapping Hanning-windowed grains are summed at arbitrary buffer positions, creating smeared, alien textures from ordinary audio. The ALPHAOMEGA parameter pushes past the write head entirely — using LPC (linear predictive coding) to synthesise what the signal *might* become, inferring a plausible extraterrestrial future.

---

## DSP Architecture

| Stage | Description |
|---|---|
| **Ring Buffer** | 10-second circular float buffer at host sample rate, written on every block |
| **Temporal Engine** | Read-head offset controlled by X, reaching up to 10 s into the past |
| **Granular Engine** | 1–32 simultaneous Hanning-windowed grains drawn from ring buffer positions; density (Y) and scatter (Z) driven |
| **LPC Future Buffer** | Order-8 Levinson-Durbin predictor trained every 2048 samples; generates 4096-sample predicted future buffer |
| **Glitch Engine** | Probability-based stutter (Z²) and bit-depth reduction (Z > 0.3) applied to the wet signal |
| **Feedback** | Wet output looped back into the ring buffer write path |

---

## Parameters

| Parameter | ID | Range | Default | Description |
|---|---|---|---|---|
| **X** | `temporal_displacement` | 0.0 – 1.0 | 0.0 | Temporal displacement: 0 = present, 1 = 10 s back |
| **Y** | `warp_density` | 0.0 – 1.0 | 0.0 | Grain density/overlap: 0 = 1 grain, 1 = 16 simultaneous grains |
| **Z** | `dimensional_scatter` | 0.0 – 1.0 | 0.0 | Randomness of grain positions + glitch probability |
| **ALPHAOMEGA** | `alpha_omega` | 0.0 – 1.0 | 0.0 | Future inference blend: 0 = past only, 1 = LPC-predicted future |
| **GRAIN SIZE** | `grain_size_ms` | 10 – 500 ms | 80 ms | Duration of each grain |
| **FEEDBACK** | `feedback` | 0.0 – 0.99 | 0.0 | Wet signal fed back into ring buffer write path |
| **MIX** | `mix` | 0.0 – 1.0 | 1.0 | Dry/wet balance |

All parameters are fully automatable via the JUCE AudioProcessorValueTreeState.

---

## Build Instructions

### Prerequisites

- CMake ≥ 3.22
- Ninja (`brew install ninja`)
- Xcode Command Line Tools (`xcode-select --install`)
- macOS 10.13+

### Quick Build (preset)

```bash
cmake --preset default
cmake --build --preset default
```

Builds a universal binary (arm64 + x86_64) in `build/`.

### Manual Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(sysctl -n hw.logicalcpu)
```

### Output locations

After a successful build:

```
build/ALCUBIERRE_artefacts/Release/VST3/ALCUBIERRE.vst3
build/ALCUBIERRE_artefacts/Release/AU/ALCUBIERRE.component
```

Copy these to your plugin folders:

```bash
# VST3
cp -r build/ALCUBIERRE_artefacts/Release/VST3/ALCUBIERRE.vst3 \
      ~/Library/Audio/Plug-Ins/VST3/

# AU
cp -r build/ALCUBIERRE_artefacts/Release/AU/ALCUBIERRE.component \
      ~/Library/Audio/Plug-Ins/Components/

# Rescan AU cache
killall -9 AudioComponentRegistrar 2>/dev/null || true
```

---

## File Structure

```
ALCUBIERRE/
  CMakeLists.txt            — JUCE 7 FetchContent, VST3 + AU, universal macOS
  CMakePresets.json         — default / debug presets
  Source/
    PluginProcessor.h/.cpp  — AudioProcessor, APVTS, processBlock, LPC update
    PluginEditor.h/.cpp     — 800×500 custom UI
    RingBuffer.h            — Thread-safe 10-second circular audio buffer
    GranularEngine.h/.cpp   — Hanning-windowed granular synthesis engine
    GlitchEngine.h/.cpp     — Stutter, bit-mangle effects
    AlcubierreKnob.h        — Custom LookAndFeel rotary knob component
    WarpFieldVisualizer.h   — Animated warp-field background component
  README.md
```

---

## Notes

- **Thread safety**: Ring buffer write and read both occur on the audio thread. The editor's `WarpFieldVisualizer` reads only parameter atomics; no audio-thread data is accessed from the GUI thread.
- **Ableton Live compatibility**: No MIDI in/out, effect bus layout, standard VST3 bundle ID.
- **Stability**: LPC reflection coefficients are stability-checked; predicted samples are soft-clipped to ±1.0 to prevent runaway feedback in the future buffer.

---

## Credits

**ALCUBIERRE** is developed by **Finis Musicae Corp**.

Concept, DSP, and UI by Finis Musicae Corp.
Built with [JUCE](https://juce.com) 7.0.9.

*"The warp drive of audio — displacing the present through its own past."*

---

© 2024 Finis Musicae Corp. All rights reserved.
