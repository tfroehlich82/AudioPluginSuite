# Audio Plugin Suite

[![Build](https://github.com/tfroehlich82/AudioPluginSuite/actions/workflows/build.yml/badge.svg)](https://github.com/tfroehlich82/AudioPluginSuite/actions/workflows/build.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![JUCE](https://img.shields.io/badge/JUCE-7.0.9-8A2BE2)
![Formats](https://img.shields.io/badge/formats-VST3%20%7C%20AU%20%7C%20Standalone-informational)
![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20macOS-lightgrey)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

Three creative audio plugins built with [JUCE](https://juce.com) and C++17: an FFT phase-drift processor, a dynamic saturation stage, and a chaos-driven MIDI micro-timing sequencer. Every plugin ships with its own dark, neon-accented interface and real-time visualisers.

| Plugin | Type | What it does |
|---|---|---|
| **Spectral Drift Shifter** | Audio effect | FFT overlap-add processing with an LFO that drifts the phase of every frequency bin. Freeze the spectrum, or feed it back into itself. |
| **Dynamic Saturation Companion** | Audio effect | Waveshaping (Tube, Tape or Diode) whose drive follows the signal envelope. |
| **Micro-Timing Chaos Sequencer** | MIDI effect | Shifts note timing and velocity using the Lorenz attractor, for loose, human-sounding grooves. |

---

## Contents

- [For users](#for-users)
  - [Installation](#installation)
  - [Spectral Drift Shifter](#spectral-drift-shifter)
  - [Dynamic Saturation Companion](#dynamic-saturation-companion)
  - [Micro-Timing Chaos Sequencer](#micro-timing-chaos-sequencer)
  - [Latency](#latency)
- [For developers](#for-developers)
  - [Requirements](#requirements)
  - [Building](#building)
  - [Running the tests](#running-the-tests)
  - [Repository layout](#repository-layout)
  - [Architecture](#architecture)
  - [Adding a plugin](#adding-a-plugin)
  - [Continuous integration](#continuous-integration)
  - [Coding conventions](#coding-conventions)
  - [Known limitations](#known-limitations)
- [Contributing](#contributing)
- [License](#license)

---

## For users

### Installation

Download the latest build from the [Releases](https://github.com/tfroehlich82/AudioPluginSuite/releases) page, or [build it yourself](#building).

Copy the plugin bundles to your system plugin folder:

| Platform | Format | Location |
|---|---|---|
| Windows | VST3 | `C:\Program Files\Common Files\VST3\` |
| macOS | VST3 | `~/Library/Audio/Plug-Ins/VST3/` |
| macOS | Audio Unit | `~/Library/Audio/Plug-Ins/Components/` |

Then rescan plugins in your DAW. The Standalone build runs without a host and is useful for quick checks.

The plugins need a VST3 host on Windows, or a VST3 or Audio Unit host on macOS.

### Spectral Drift Shifter

An audio effect that breaks the input into 2048-point FFT frames, drifts the phase of each frequency bin, and resynthesises the result.

| Control | Range | Default | Description |
|---|---|---|---|
| **Drift Rate** | 0.1 – 10 Hz (logarithmic) | 1 Hz | Speed of the drift LFO. |
| **Mix** | 0 – 100 % | 100 % | Dry/wet balance. |
| **Feedback** | 0 – 95 % | 0 % | Feeds the wet signal back into the input. The feedback path is soft-limited, so it cannot run away. |
| **Freeze** | on / off | off | Holds the captured spectrum and keeps advancing the phases, for a sustained texture. |

The left and right channels run on LFOs 90° apart, which decorrelates them and widens the image. The display shows the input spectrum (dim) over the output spectrum (bright).

### Dynamic Saturation Companion

An audio effect with a saturation stage whose drive follows the signal envelope:

> Dynamic Drive = Drive × (1 + Sensitivity × Envelope)

| Control | Range | Default | Description |
|---|---|---|---|
| **Drive** | 0 – 24 dB | 6 dB | Base pre-gain. |
| **Sensitivity** | 0 – 1 | 0.5 | How strongly the envelope pushes the drive harder. |
| **Mode** | Tube / Tape / Diode | Tube | Transfer curve (see below). |
| **Mix** | 0 – 100 % | 100 % | Dry/wet balance. |
| **Output** | −12 – +6 dB | 0 dB | Output level. |

- **Tube**: asymmetric, `tanh(x + c)`, re-centred so that silence stays silent.
- **Tape**: symmetric soft clip, `x − x³/3`.
- **Diode**: exponential curve with a soft knee on positive peaks and a tighter one on negative peaks.

Every curve is bounded to ±1, so the stage never exceeds full scale. The transfer-curve display redraws in real time at the current input level, and the meter shows the envelope.

### Micro-Timing Chaos Sequencer

A MIDI effect. Put it before an instrument, and every note-on is shifted in time and velocity by a Lorenz attractor. The attractor's X coordinate sets the timing and its Z coordinate sets the velocity.

| Control | Range | Default | Description |
|---|---|---|---|
| **Chaos Intensity** | 0 – 100 % | 50 % | Scales how far notes are shifted. |
| **Timing Offset Max** | 0 – 50 ms | 15 ms | Largest possible shift, early or late. |
| **Velocity Drift** | 0 – 30 % | 10 % | Largest possible velocity change, up or down. |
| **BPM Sync** | Free / Host Sync | Free | *Free* runs the attractor in real time. *Host Sync* advances it with the host's tempo, so it only moves while the transport plays. |

The phase-space plot draws the attractor's X/Y trajectory in real time, and the readout shows the offset of the last note.

### Latency

| Plugin | Reported latency | Why |
|---|---|---|
| Spectral Drift Shifter | 2048 samples (≈ 46 ms at 44.1 kHz) | Overlap-add FFT frame length. The dry signal is delayed to match, so the mix stays phase-aligned. |
| Micro-Timing Chaos Sequencer | 50 ms | Early notes are held back by this much so they can be sent on time. The host compensates. |
| Dynamic Saturation Companion | 0 samples | Memoryless processing. |

---

## For developers

### Requirements

| Tool | Version | Notes |
|---|---|---|
| CMake | 3.22 or newer | |
| Git | any recent | JUCE is downloaded during configuration. |
| C++ compiler | C++17 | MSVC (Visual Studio 2022 or newer) on Windows; Xcode / Apple Clang on macOS. |
| Internet access | at configure time | `FetchContent` downloads JUCE 7.0.9 from GitHub. |

No separate JUCE install is needed. The version is pinned in the root [`CMakeLists.txt`](CMakeLists.txt).

### Building

**Windows (Visual Studio, x64):**

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

**macOS (Xcode):**

```bash
cmake -S . -B build -G Xcode
cmake --build build --config Release --parallel
```

**Any platform, Ninja or Make (single-config):**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

**VS Code:** open the folder with the *CMake Tools* extension and select the Release kit.

Plugin bundles are written to `build/plugins/<Name>/<Name>_artefacts/Release/`.

**CMake options**

| Option | Default | Effect |
|---|---|---|
| `SUITE_BUILD_PLUGINS` | `ON` | Build the plugins and the UI module. Set to `OFF` to build only the DSP core and its tests, which is faster. |
| `SUITE_BUILD_TESTS` | `ON` | Build and register the unit tests. |

Example, DSP tests only:

```bash
cmake -S . -B build -DSUITE_BUILD_PLUGINS=OFF
cmake --build build --config Release --target CoreDSPTests
```

Plugin formats are chosen automatically: VST3 and Standalone everywhere, plus AU on macOS.

### Running the tests

```bash
ctest --test-dir build -C Release --output-on-failure
```

The tests are standard `assert`s, but they are compiled with `NDEBUG` undefined so they also run in Release. Without that, every check would be compiled out.

The suite covers:

| Test | What it checks |
|---|---|
| `testSaturationCurves` | Every shaper maps 0 to 0 and stays within [−1, 1], up to 10 000× input gain. |
| `testLorenzChaos` | 200 000 RK4 steps stay inside the attractor box, the normalised output stays in [−1, 1], and consecutive steps are continuous. |
| `testSpectralFFT` | Silence gives a silent spectrum. A tone on bin 64 shows its peak on bin 64. |
| `testSpectralReconstruction` | With no drift, the engine is an exact delay: output equals input delayed by the latency, to within 1e-3. |
| `testEnvelopeFollower` | Attack and release times match their configured values. |
| `testDynamicDrive` | The drive formula. |

### Repository layout

```
AudioPluginSuite/
├── CMakeLists.txt              Root build: JUCE pin, global definitions, options
├── modules/
│   ├── core_dsp/               Header-only DSP primitives (no plugin code)
│   │   ├── include/            CoreDSP.h, SaturationEngine.h, ChaosEngine.h, SpectralEngine.h
│   │   └── tests/              DSPTests.cpp
│   └── suite_ui/               Shared look and feel and visualiser widgets
│       ├── include/            SuiteLookAndFeel.h, SuiteWidgets.h
│       └── Source/             SuiteLookAndFeel.cpp, SuiteWidgets.cpp
├── plugins/
│   ├── SpectralDrift/          Processor, editor and CMake for each plugin
│   ├── DynamicSaturation/
│   └── MicroTimingSeq/
├── .github/workflows/build.yml CI: build and test on Windows and macOS
├── CLAUDE.md                   Working notes for AI coding agents
├── CHANGELOG.md
└── coding_agent_prompt.md      Original project specification
```

### Architecture

```
            ┌────────────────────── plugins/<Name> ──────────────────────┐
  host ───► │ PluginProcessor: APVTS parameters, audio/MIDI processing   │ ──► host
            │        │ publishes lock-free snapshots (atomics)           │
            │        ▼                                                    │
            │ PluginEditor: Timer (60 Hz) reads snapshots, draws widgets  │
            └──────────┬──────────────────────────────┬───────────────────┘
                       │ uses DSP                     │ uses UI
              ┌────────▼────────┐            ┌────────▼────────┐
              │   CoreDSP       │            │    SuiteUI      │
              │ (header-only,   │            │ LookAndFeel and │
              │  depends on     │            │ visualisers     │
              │  juce_dsp)      │            └─────────────────┘
              └─────────────────┘
```

Key decisions:

- **Parameters** live in an `AudioProcessorValueTreeState` in every processor, so they automate, save and restore the same way in every host. Parameter IDs are `static constexpr` members of each processor.
- **DSP is kept out of the processors.** `CoreDSP` contains the algorithms as small classes with no plugin dependencies, which is what makes the unit tests possible.
- **Audio thread → UI** communication uses atomics only. The audio thread never blocks on the message thread.
- **Stereo drift** uses two `DriftEngine` instances whose LFOs are offset by π/2.
- **Overlap-add normalisation** is measured from the actual window values, not assumed, so the reconstruction stays exact for any window shape.

### Adding a plugin

1. Create `plugins/<Name>/` with `CMakeLists.txt` and `Source/PluginProcessor.{h,cpp}` and `Source/PluginEditor.{h,cpp}`. Copy an existing plugin as the starting point.
2. Give it unique `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE` values and a `BUNDLE_ID`. The codes must be four characters with at least one uppercase letter, and they must not change once a plugin has been released, because hosts use them to identify saved sessions.
3. Put the DSP in `modules/core_dsp/include/` as a header-only class, and add a test for it in `DSPTests.cpp`.
4. Register the plugin in the root `CMakeLists.txt` under `if(SUITE_BUILD_PLUGINS)`.
5. Add it to the table in this README and to `CHANGELOG.md`.

### Continuous integration

[`build.yml`](.github/workflows/build.yml) runs on every push and pull request to `main`:

1. Configures with CMake on `windows-latest` (newest installed Visual Studio, x64) and `macos-latest` (Xcode).
2. Builds the whole suite in Release.
3. Runs `ctest`.
4. Uploads the VST3 bundles as build artifacts.

The build runs before the tests, since CTest needs the test executable to exist.

### Coding conventions

- C++17. No compiler extensions; `CMAKE_CXX_EXTENSIONS` is `OFF`.
- Namespaces: `CoreDSP::…` for DSP, `SuiteUI::…` for the UI module.
- Audio-thread code is allocation-free and lock-free. Allocate in `prepareToPlay` and in constructors.
- Any function that runs per sample is marked `noexcept`.
- Every transfer function returns values in [−1, 1] for any finite input.
- Parameter IDs and names are defined once, in the processor.
- Comments explain *why*, not *what*.

### Known limitations

- **No oversampling** in the saturation stage. Strong drive creates aliasing above about 10 kHz.
- **Spectral Drift feedback tail:** `getTailLengthSeconds()` returns 0, so a host won't render the tail of a feedback echo after input stops.
- **Micro-Timing Chaos Sequencer** delays every note by 50 ms (reported as latency). Hosts that ignore MIDI-effect latency will shift the whole sequence earlier by that amount.
- **Spectral Drift** is phase-only drift. Magnitudes are preserved, so the effect is most audible on sustained, harmonic material.
- Plugins are not code-signed or notarised, so macOS will ask for approval on first launch.

---

## Contributing

Issues and pull requests are welcome. Before opening a pull request:

1. Build in Release and run `ctest --test-dir build -C Release --output-on-failure`.
2. Add tests for new DSP behaviour.
3. Describe audible changes in the pull request, and note any change to parameter ranges or IDs, since those affect saved sessions.

## License

This project is licensed under the **GNU General Public License v3.0**. See [LICENSE](LICENSE) for the full text.

The project builds on JUCE, which is available under the GPLv3 or a commercial licence from its vendor. Because this project is GPLv3, JUCE is used under its GPLv3 option. If you distribute closed-source builds, you will need a commercial JUCE licence instead; see the [JUCE licensing terms](https://juce.com/juce-7-licence/).

---

<sub>JUCE is a trademark of its respective owner. VST is a trademark of Steinberg Media Technologies GmbH.</sub>
