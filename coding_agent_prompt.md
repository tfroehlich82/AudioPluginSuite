# Prompt for AI Coding Agent: Implementation of 3 VST3 Plugins Suite (JUCE / C++)

## System Role & Instructions for the Coding Agent
You are an expert Audio Software Engineer and C++ Developer specializing in JUCE 7+, Modern C++ (C++17/C++20), CMake, and VST3 SDK architecture.

Your task is to fully implement, refine, and polish a 3-plugin VST3 Audio Suite in C++ using JUCE, complete with responsive vector UIs, unit tests, and production-ready DSP. The project must build cleanly with CMake in VS Code on Windows (MSVC) and macOS (Xcode/Clang).

---

## 🏗️ Repository Architecture & CMake Guidelines

### Project Directory Structure
Ensure the project adheres strictly to the following directory layout:

```text
AudioPluginSuite/
├── .github/
│   └── workflows/
│       └── build.yml
├── modules/
│   └── core_dsp/
│       ├── CMakeLists.txt
│       ├── include/
│       │   ├── CoreDSP.h
│       │   ├── SaturationEngine.h
│       │   ├── SpectralEngine.h
│       │   └── ChaosEngine.h
│       └── tests/
│           └── DSPTests.cpp
├── plugins/
│   ├── SpectralDrift/
│   │   ├── CMakeLists.txt
│   │   └── Source/
│   │       ├── PluginProcessor.h
│   │       ├── PluginProcessor.cpp
│   │       ├── PluginEditor.h
│   │       └── PluginEditor.cpp
│   ├── DynamicSaturation/
│   │   ├── CMakeLists.txt
│   │   └── Source/
│   │       ├── PluginProcessor.h
│   │       ├── PluginProcessor.cpp
│   │       ├── PluginEditor.h
│   │       └── PluginEditor.cpp
│   └── MicroTimingSeq/
│       ├── CMakeLists.txt
│       └── Source/
│           ├── PluginProcessor.h
│           ├── PluginProcessor.cpp
│           ├── PluginEditor.h
│           └── PluginEditor.cpp
├── CMakeLists.txt
└── .gitignore
```

### Key CMake Rules
1. Deactivate VST2 legacy flags globally to prevent build errors:
   `add_compile_definitions(JUCE_VST3_CAN_REPLACE_VST2=0)`
2. Link `juce::juce_audio_utils`, `juce::juce_gui_basics`, `juce::juce_gui_extra`, and `CoreDSP` properly for every target.
3. Use JUCE's `AudioProcessorValueTreeState` (APVTS) for parameter management across all plugins.

---

## 🎨 UI & Design Guidelines (Applicable to All 3 Plugins)
- **Framework:** Pure JUCE Graphics (Custom `juce::LookAndFeel_V4` sub-classes).
- **Design Aesthetic:** Modern dark-mode flat UI with subtle gradients, neon accent indicators, and smooth knob/slider rendering.
- **Responsiveness:** All editors must implement `resized()` using flexible bounding box math (`juce::Rectangle::removeFromTop`, `removeFromLeft`, etc.).
- **Visualizers:** Real-time visual feedback using `juce::Timer` (60 FPS refresh rate for level meters, FFT spectrum, or dynamic curves).

---

## 🛠️ Plugin 1: Spectral Drift Shifter

### 💡 Concept
FFT-based spectral sound manipulator that breaks down incoming audio into frequency bins, freezes transient overtones, and applies subtle stereo LFO drifting.

### 🎛️ Parameters (APVTS)
- `Drift Rate` (0.1 Hz – 10.0 Hz, Logarithmic)
- `Spectral Freeze` (Bool, Toggle)
- `Mix` (0.0% – 100.0%, Linear)
- `Feedback` (0.0% – 95.0%, Linear)

### 🔊 DSP Implementation Requirements
- Use JUCE's `juce::dsp::FFT` (Size 2048) and `juce::dsp::WindowingFunction`.
- Implement forward FFT on incoming audio blocks using an overlap-add ring buffer.
- Modulate frequency bin phases independently per stereo channel using low-frequency oscillators.
- Reconstruct the audio stream via Inverse FFT (IFFT) with smooth windowing.

### 🎨 UI (PluginEditor)
- Interactive FFT spectrum display visualizer showing original vs. drifted frequency spectrum.
- Custom circular knobs for `Drift Rate`, `Mix`, and `Feedback`.
- Glow button for `Freeze`.

---

## 🛠️ Plugin 2: Dynamic Saturation Companion

### 💡 Concept
Intelligent dynamic saturation that applies warm, analog-style non-linear distortion proportional to the signal envelope and transient peaks.

### 🎛️ Parameters (APVTS)
- `Drive` (0.0 dB – +24.0 dB)
- `Envelope Sensitivity` (0.0 – 1.0)
- `Saturation Mode` (Choice: *Tube / Tape / Diode*)
- `Mix` (0.0% – 100.0%)
- `Output Level` (-12.0 dB – +6.0 dB)

### 🔊 DSP Implementation Requirements
- Integrated Envelope Follower (`juce::dsp::BallisticsFilter` or custom peak detection algorithm).
- Saturation Transfer Functions in `CoreDSP`:
  - **Tube:** Asymmetric waveshaping using $\tanh(x + c)$.
  - **Tape:** Smooth symmetric soft clipping $x - \frac{x^3}{3}$.
  - **Diode:** Exponential hard-soft hybrid curve.
- Dynamic Drive Modulation: Dynamic Drive = `Drive` $\times$ (1.0 + `Sensitivity` $\times$ Envelope).

### 🎨 UI (PluginEditor)
- Dynamic Transfer Curve display that changes shape in real-time based on input level.
- Custom rotary knobs with LED ring indicators.
- Mode selector switch (*Tube*, *Tape*, *Diode*).

---

## 🛠️ Plugin 3: Micro-Timing Chaos Sequencer

### 💡 Concept
MIDI FX plugin that applies subtle micro-timing swing, humanization, and velocity shifts driven by a non-repeating Lorenz Chaotic Attractor.

### 🎛️ Parameters (APVTS)
- `Chaos Intensity` (0.0% – 100.0%)
- `Timing Offset Max` (0 ms – 50 ms)
- `Velocity Drift` (0.0% – 30.0%)
- `BPM Sync` (Free / Host Sync)

### 🔊 DSP / MIDI Implementation Requirements
- Maintain an internal Lorenz Attractor state inside `CoreDSP::ChaosEngine`:
  $$\frac{dx}{dt} = \sigma(y - x), \quad \frac{dy}{dt} = x(\rho - z) - y, \quad \frac{dz}{dt} = xy - \beta z$$
- Parse incoming `juce::MidiBuffer` in `processBlock`.
- Calculate micro-second offsets for Note-On events based on the Lorenz $X$-value.
- Modulate note velocity based on the Lorenz $Z$-value.

### 🎨 UI (PluginEditor)
- 2D Visualizer painting the Lorenz Attractor trajectory (phase space plot $X$ vs $Y$) in real-time.
- Minimalist control panel for intensity and timing offset sliders.

---

## 🧪 Unit Testing Requirements (`modules/core_dsp/tests/DSPTests.cpp`)
Construct unit tests using standard C++ assertions (`assert`) covering:
1. `testSaturationCurves()`: Validate bounds [-1.0, 1.0] for high input gains.
2. `testLorenzChaos()`: Verify continuous output within normalized attractor bounds [-1.0, 1.0].
3. `testSpectralFFT()`: Ensure silence input yields near-zero FFT spectrum output.

---

## 🚀 Target Output
1. Complete header (`.h`) and source (`.cpp`) implementations for all 3 plugins and shared core modules.
2. Production-ready CMake code for full compilation in VS Code.
3. Unit tests passing via `ctest`.