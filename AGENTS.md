# AGENTS.md: Audio Plugin Suite

Knowledge base for coding agents working on this repository. Read this before changing anything. It records what is built, why it is built that way, what is verified and what is not, and the JUCE and CMake pitfalls already hit.

The original specification is in `coding_agent_prompt.md`. Where this file and the spec differ, this file describes what the code does now, and the differences are listed under [Deviations from the spec](#deviations-from-the-spec).

---

## 1. What this project is

Three audio plugins written with JUCE 7.0.9 in C++17. Formats: VST3 on Windows and macOS, Audio Unit on macOS, and Standalone on both.

| Plugin | Folder | Kind | DSP core | Editor accent |
|---|---|---|---|---|
| Spectral Drift Shifter | `plugins/SpectralDrift` | audio effect | `CoreDSP::Spectral::DriftEngine` | cyan `#2DE2FF` |
| Dynamic Saturation Companion | `plugins/DynamicSaturation` | audio effect | `CoreDSP::Saturation` | amber `#FF9F1C` |
| Micro-Timing Chaos Sequencer | `plugins/MicroTimingSeq` | MIDI effect, with stereo pass-through | `CoreDSP::Chaos::LorenzAttractor` | lime `#7DFF5A` |

- Manufacturer name `AudioSuite`, manufacturer code `Asui`, bundle IDs `com.audiosuite.<name>`.
- Plugin codes `Sdft`, `Dsat`, `Mtsq`. Codes and parameter IDs are saved in sessions and **must never change** after a release.
- Licence: GNU GPL v3 (`LICENSE`, the unmodified GNU text). JUCE is used under its GPLv3 option. Closed-source distribution would need a commercial JUCE licence.
- Repository: `https://github.com/tfroehlich82/AudioPluginSuite`. The `origin` remote points there.
- The owner works on Windows in VS Code with CMake Tools. The existing `build/` directory uses the `Visual Studio 18 2026` generator with clang-cl.

---

## 2. Repository layout

```
CMakeLists.txt                    Root: JUCE pin, global definitions, options, SUITE_PLUGIN_FORMATS
AGENTS.md / CLAUDE.md             This file / Claude Code specifics (points here)
README.md, CHANGELOG.md, LICENSE  User and developer docs, history, licence
coding_agent_prompt.md            Original specification (historical)
modules/
  core_dsp/                       Header-only DSP, depends on juce::juce_dsp
    include/CoreDSP.h             Umbrella header; decibelsToGain()
    include/SaturationEngine.h    Shapers, fold, OnePoleFilter, ToneControl, EnvelopeFollower
    include/ChaosEngine.h         LorenzAttractor (RK4), LorenzParams, LorenzState
    include/SpectralEngine.h      DriftEngine (overlap-add FFT), LfoShape, lfoValue()
    tests/DSPTests.cpp            CoreDSPTests (19 tests)
    CMakeLists.txt                CoreDSP INTERFACE target, CoreDSPTests
  suite_ui/                       Shared UI. NOT a library target, see section 8
    include/SuiteLookAndFeel.h    Colours namespace, SuiteLookAndFeel (LookAndFeel_V4)
    include/SuiteWidgets.h        SpectrumDisplay, TrailPlot, CurveView, LevelMeter, SegmentedSelector, LabelledSlider
    Source/*.cpp                  Implementations
    SuiteUI.cmake                 suite_ui_add_to_target(<target>): adds sources + include dir
    tests/SuiteUITests.cpp        SuiteUITests (8 tests)
    CMakeLists.txt                SuiteUITests only
plugins/<Name>/
  CMakeLists.txt                  juce_add_plugin, links, <Name>Tests executable, add_test
  Source/PluginProcessor.{h,cpp}  APVTS, processing, state, createPluginFilter()
  Source/PluginEditor.{h,cpp}     Editor, 60 Hz Timer reads processor snapshots
  tests/PluginTests.cpp           Plugin tests (10 to 11 each)
tests/
  common/PluginTestUtils.h        Helpers: setParameter, checkRange, checkStateRoundTrip, checkEditor, addSine, ...
  host/PluginHostTests.cpp        Loads a built .vst3 through JUCE's VST3 host (scan, instantiate, editor, process)
  host/CMakeLists.txt             PluginHostTests + one ctest entry per plugin
.github/workflows/build.yml       CI: Windows (windows-latest) + macOS (macos-14, Xcode 15.4). Build, ctest, upload VST3
.github/workflows/release.yml     Manual release (workflow_dispatch): build, test, package zips, gh release create
```

Build output: `build/plugins/<Name>/<Name>_artefacts/Release/{VST3,Standalone,AU}/`. Test executables: `build/modules/core_dsp/Release/CoreDSPTests.exe`, `build/tests/host/Release/PluginHostTests.exe`, and so on.

---

## 3. Build and test

Windows (PowerShell). CMake is not on the bash PATH, so use the full path:

```powershell
$CM = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $CM -S . -B build -A x64 -DSUITE_BUILD_PLUGINS=ON
& $CM --build build --config Release --parallel 8
& "$(Split-Path $CM)\ctest.exe" --test-dir build -C Release --output-on-failure
```

- Do not pass `-G`. CI relies on CMake picking the newest installed Visual Studio, and the local `build/` cache already has a generator.
- `-DSUITE_BUILD_PLUGINS=OFF` builds only CoreDSP and its tests. Note that the SuiteUI tests are only registered when plugins are on.
- macOS CI runs `macos-14` with Xcode 15.4 (see [section 10](#10-open-items-and-known-risks)).
- JUCE is fetched by `FetchContent` into `build/_deps/juce-src`. That directory is a full local copy: grep it to check any API before using it.

Last verified status: full Release build succeeds on Windows. All 8 CTest entries pass: CoreDSPTests, SuiteUI_Tests, the three plugin test executables, and the three VST3 host-load tests.

---

## 4. Parameters

Parameter IDs are `static constexpr const char*` members of each processor. Defaults and ranges below are what `createParameterLayout()` defines. Each float uses a `NormalisableRange`, and some use a log or skew.

### 4.1 Spectral Drift Shifter (14 controls)

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `driftRate` | Drift Rate | 0.1 – 10 Hz, log | 1 Hz | LFO speed |
| `depth` | Drift Depth | 0 – 100 % | 19.1 % | Maps to 0 – π rad in the engine. 19.1 % = 0.6 rad |
| `spread` | Bin Spread | 0 – 0.5 rad | 0.05 | LFO phase step between neighbouring bins |
| `stereoPhase` | Stereo Phase | 0 – 180° | 90° | Offset of the right channel's LFO |
| `lfoShape` | LFO Shape | choice: Sine, Triangle, Saw, Square | Sine | `CoreDSP::Spectral::LfoShape` |
| `shift` | Spectral Shift | −16 – +16 bins | 0 | Magnitudes read from bin k − shift, linear interpolation |
| `tilt` | Spectral Tilt | −1 – +1 | 0 | Gain (k / 32)^tilt, clamped to [1/16, 8] |
| `gate` | Spectral Gate | −90 – 0 dB | −90 | At −90 the gate is open for every bin |
| `feedback` | Feedback | 0 – 12 % | 0 % | Plugin range is capped at 12 % (`maxFeedbackPercent`). Engine accepts up to 0.95. Fed back through tanh |
| `feedbackDamping` | Feedback Damping | 0 – 100 % | 30 % | One-pole low-pass in the feedback loop. 0 % passes the loop unchanged |
| `driftStart` | Drift Start | 0 – 100 % | 0 % | Bins below this fraction of the spectrum are not drifted |
| `outputGain` | Output Gain | −12 – +12 dB | 0 dB | Applied to the whole output after the engines |
| `mix` | Mix | 0 – 100 % | 100 % | Dry is delayed by the frame latency |
| `freeze` | Freeze | bool | off | Holds captured magnitudes, phases still advance |

Latency reported: 2048 samples (`setLatencySamples`).

### 4.2 Dynamic Saturation Companion (13 controls)

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `drive` | Drive | 0 – 24 dB | 6 dB | Pre-gain base, smoothed 50 ms |
| `sensitivity` | Envelope Sensitivity | 0 – 1 | 0.5 | Dynamic Drive = Drive × (1 + Sens × Env) |
| `mode` | Saturation Mode | choice: Tube, Tape, Diode | Tube | |
| `mix` | Mix | 0 – 100 % | 100 % | Smoothed 50 ms |
| `outputLevel` | Output Level | −12 – +6 dB | 0 dB | Smoothed 50 ms |
| `attack` | Attack | 0.1 – 50 ms, skew 0.35 | 5 ms | Envelope rise |
| `release` | Release | 10 – 1000 ms, skew 0.4 | 150 ms | Envelope fall |
| `detectHpf` | Detect HPF | 20 – 800 Hz, log | 20 Hz | High-pass on the detector only |
| `stereoLink` | Stereo Link | 0 – 100 % | 100 % | 100 % = linked envelope (max of channels) |
| `bias` | Tube Bias | 0 – 0.5 | 0.2 | Asymmetry of the tube curve. Tube only |
| `fold` | Fold | 0 – 100 % | 0 % | Wavefolder after shaping, 2× domain |
| `tone` | Tone | −1 – +1 | 0 | Tilt around 1 kHz after the shaper |
| `curve` | Envelope Curve | 0.5 – 2.0 | 1.0 | Envelope exponent |

Latency reported: the oversampler's integer latency, computed in `prepareToPlay`. The dry path is delayed by the same amount.

### 4.3 Micro-Timing Chaos Sequencer (4 controls)

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `chaosIntensity` | Chaos Intensity | 0 – 100 % | 50 % | Scales the timing offset |
| `timingOffsetMax` | Timing Offset Max | 0 – 50 ms | 15 ms | Offset = intensity × X × max |
| `velocityDrift` | Velocity Drift | 0 – 30 % | 10 % | Velocity × (1 + drift × Z), clamped 1 – 127 |
| `bpmSync` | BPM Sync | choice: Free, Host Sync | Free | See section 5.3 |

Latency reported: 50 ms fixed (`maxTimingOffsetMs`), regardless of the offset parameter, so latency never changes during a session.

---

## 5. Plugin behaviour

### 5.1 Spectral Drift engine

Per channel, `DriftEngine` runs this pipeline:

```
input → ring buffer → Hann analysis → FFT 2048 → [not frozen: store magnitudes and phases]
      → shapeMagnitudes() (shift, tilt, gate) → per-bin phase drift from the LFO
      → Hermitian IFFT → Hann synthesis → overlap-add → wet
      → feedback into input ring; dry/wet mix with the aligned dry signal
```

Frame parameters: `frameOrder = 11` (2048), `hopSize = frameSize / 4` (75 % overlap), `numBins = 1025`.

Invariants (the indexing is subtle, so do not change it casually):
- Every sample is stored at slot `n mod frameSize`. `ringPos` is `n mod frameSize`, and the slot at `ringPos` holds the sample that is `frameSize` samples old.
- At call `n`, the output emitted is the output sample for index `n − frameSize`. It is final, because the frame covering it was processed at the end of call `n − 1`.
- The overlap gain is `invOverlapGain[hopIndex]`, measured numerically from the window (`hopIndex = n mod hopSize`). It is not assumed to be constant.
- The frame at time `t` (when `hopIndex` wraps) reads `inputRing` starting at `ringPos`, which is the oldest sample `t − frameSize`.
- The output spectrum shown in the UI is measured on `wetRing`, which holds finished samples one frame behind. It is the real overlap-add result, not the synthesised frame.
- `inverse FFT` in JUCE is already divided by N. Do not normalise again.
- `frameCount` increments once per frame. The UI uses it, together with the input energy, to decide whether to show "Waiting for audio".

Reconstruction test: with drift depth 0, no shift/tilt/gate, feedback 0 and mix 1, the output equals the input delayed by 2048 samples to within 1e-3. This is the main correctness check for the engine.

### 5.2 Dynamic Saturation processing

Three passes in `processBlock`:
1. **Base rate.** Detector high-pass per channel → linked peak → linked envelope and per-channel envelopes, mixed by Stereo Link → shaped by Curve → Dynamic Drive per sample into `preGainBuffer`. Mix and output gain are smoothed per sample.
2. **2× oversampled.** `juce::dsp::Oversampling<float>` (half-band FIR, integer latency) upsamples; each oversampled sample is `fold(shape(x × preGain[n/2], mode, bias), fold)`; then downsample back into the buffer.
3. **Base rate.** `wet = clamp(±1, tone(...))`. This ceiling is deliberate: the oversampler and tone can overshoot by about 10 %, and the output test checks a hard ±2.0 after the output gain. Then `mixed = dryDelayed + mix × (wet − dryDelayed)` and `× outputGain`. `dryDelayed` is a ring of `latencySamples`.

### 5.3 Micro-Timing sequencer

Per block:
1. `advanceChaos(numSamples)`:
   - **Free:** advance the attractor by `numSamples / sampleRate × 2.0` Lorenz time units.
   - **Host Sync:** use `getPlayHead()->getPosition()`. If the transport is stopped, hold still. If playing, advance by `Δppq × 0.5` units, ignoring Δppq ≤ 0 or > 4 beats (seeks). With no playhead, fall back to Free.
2. For every MIDI event, the target time is `streamPosition + samplePosition + latency`, where `latency` is 2400 samples at 48 kHz:
   - Note-on: `target += round(intensity × X × maxOffsetMs × sr / 1000)`. Velocity is scaled by `1 + drift × Z`. The note's target time is stored per channel and note.
   - Note-off: `target = max(target, lastNoteOnTarget[ch][note])`. A note-off never precedes its note-on, which prevents stuck notes on short notes.
   - Other events: target as is.
3. Events are queued in a `std::multimap` keyed by target time, so equal times keep insertion order. Events whose time falls in the current block are released with `samplePosition = max(0, target − streamPosition)`.
4. The trail (X, Y) of the current attractor state is published to a lock-free ring once per block.

Audio: the stereo buffer passes through unchanged (tested).

---

## 6. Test architecture

Eight CTest entries run six executables. 58 test functions across the four executables, plus one host-load test per plugin. The README has the full list.

**Assertions.** Every test file starts with `#undef NDEBUG` before `<cassert>`. Release builds (what CI uses) would otherwise compile every `assert` away. Keep this in every new test file.

**DSP tests** (`modules/core_dsp/tests/DSPTests.cpp`): pure functions and engines, no GUI.

**Plugin tests** (`plugins/<Name>/tests/PluginTests.cpp`): each executable compiles the plugin's own `Source/PluginProcessor.cpp` and `PluginEditor.cpp` directly. It includes `tests/common/PluginTestUtils.h`. Tests run under `juce::ScopedJuceInitialiser_GUI`, and `JUCE_MODAL_LOOPS_PERMITTED=1` is defined for them so `MessageManager::runDispatchLoopUntil` exists. `checkEditor()` runs the message loop so the editor's 60 Hz timer fires, then paints the editor into an image.

**Host tests** (`tests/host/PluginHostTests.cpp`): loads the built `.vst3` bundle through `AudioPluginFormatManager` with `JUCE_PLUGINHOST_VST3=1`. This define is needed because the host is compiled out by default in JUCE 7. The test scans, instantiates, opens the editor, prepares and processes. CTest passes the bundle path as `$<TARGET_FILE_DIR:<Plugin>_VST3>/../..`, and each host test depends on its plugin's VST3 target.

**SuiteUI tests** (`modules/suite_ui/tests/SuiteUITests.cpp`): widget logic and painting into images, including the look and feel's drawing paths.

Test helpers you will use: `setParameter(apvts, id, value)` takes denormalised values. `checkRange` checks the normalisable range and the default. `checkStateRoundTrip` saves state, changes a value and restores it. `addSine` adds to a buffer using a global sample index.

Mouse events in tests use the 15-argument `juce::MouseEvent` constructor of JUCE 7.0.9.

---

## 7. Decisions and why

- **JUCE pinned to 7.0.9.** Later versions change APIs and `juce_add_plugin` properties. The macOS job may need 7.0.12 if the pin fails (section 10). `ParameterAttachment::setValueAsCompleteGesture` takes a denormalised value.
- **`suite_ui` is a module, not in the spec's layout.** Three editors share the same LookAndFeel and widgets. This is an accepted deviation from the spec's directory layout.
- **`CoreDSP` is an INTERFACE target linking `juce_dsp`.** The spec requires `juce::dsp::FFT`, so the DSP core is not JUCE-free.
- **Spectral engine latency = 2048.** The dry path is delayed by the same amount, so mix stays phase-aligned.
- **Dynamic Saturation oversamples 2×** with JUCE's half-band FIR and an integer latency. Earlier versions had no oversampling; the old limitation is gone.
- **Output ceiling ±1 before the output level.** Protects against oversampler and tone overshoot. Tested.
- **MicroTiming has a stereo pass-through bus.** The earlier version declared zero audio buses. The JUCE host loads that version fine, but some DAW scanners drop VST3 audio-effect classes with no audio I/O. This is the most likely cause of the reported DAW load failure. It is a best guess: the DAW failure could not be reproduced here.
- **Micro-Timing latency fixed at 50 ms.** Notes can be early or late by up to 50 ms without the reported latency changing.
- **Note-offs clamped to note-ons** in the sequencer. Prevents stuck notes.
- **Atomics only between audio and UI.** The audio thread never locks.
- **Parameter IDs and plugin codes are permanent.** Renaming breaks saved sessions.
- **`COPY_PLUGIN_AFTER_BUILD FALSE`.** The JUCE default writes into Program Files, which fails without admin rights.
- **Spectral Drift preview:** the old panel was blank because, with no audio, the traces sat at the −120 dB floor. The fix adds a "Waiting for audio" state (shown until frames and input energy arrive) and frequency labels. A regression test checks that a 1 kHz tone reaches the snapshot at bin 43.

---

## 8. SuiteUI is compiled into each consumer

`SuiteUI` is **not** a library target. Any consumer calls `suite_ui_add_to_target(<target>)` from `modules/suite_ui/SuiteUI.cmake`, which adds the two `.cpp` files and the include directory.

Reason: a static library that links JUCE module targets bundles those module objects into the archive. Each consumer also links the JUCE modules itself, so the link fails with hundreds of duplicate symbols (`juce::Colour`, `JUCEApplicationBase`, `juce_audio_utils`, and so on). A plain `OBJECT` library had the same problem. Compiling the sources directly avoids it. Consumers still link their JUCE modules explicitly. The test executables link `CoreDSP` and `SuiteUI` but not extra JUCE modules, so they do not need them listed.

If you add a new consumer, use `suite_ui_add_to_target`. Do not add `SuiteUI` to `target_link_libraries`.

---

## 9. Procedure for common changes

**Adding a parameter**
1. Add the ID constant to the processor header, in the matching group.
2. Add the `AudioParameter*` to `createParameterLayout()` with its range and default. Use a `NormalisableRange` lambda for log ranges.
3. Cache its `std::atomic<float>*` in the constructor, and read it in `processBlock` or `readSettings`.
4. Bind a widget in the editor: `SliderAttachment`, `ButtonAttachment`, or a `ParameterAttachment` for choices. Add a `LabelledSlider` to the layout.
5. Update `numControls` and the plugin test's `checkParameterIds` list and `checkRange` calls.
6. Add DSP tests if the engine changed, and a plugin test for the audible effect.
7. Update the README parameter table, `AGENTS.md` section 4, and `CHANGELOG.md`.

**Changing a DSP engine**
- Add a test in `DSPTests.cpp` first where possible. Keep the reconstruction test passing.
- If latency changes, update `getLatencySamples`, the plugin latency tests, and the README latency table.

**Editing files.** The working tree is edited by the IDE and by the owner. Some files change between reads. Re-read a file before editing it.

**Commits.** Do not commit or push unless the owner asks. Git is initialised, and the origin remote is set.

---

## 10. Open items and known risks

- **MicroTiming DAW load:** the reported failure is not reproduced. JUCE's VST3 host loads and opens it, both before and after the stereo bus change. The remedy applied (stereo pass-through) is a best guess. If it still fails in a DAW, ask for the DAW name, its scanner log, and the exact error.
- **macOS CI** is pinned to `macos-14` with Xcode 15.4. The `macos-latest` image failed to compile JUCE 7.0.9's `juce_gui_basics.mm` while building `juceaide`. The cause is unconfirmed. If the pin fails, move to JUCE 7.0.12 (patch release, same API).
- **GUI tests on CI.** The plugin and SuiteUI tests create editors, so CI needs a desktop session. Windows and macOS hosted runners normally have one. If the GUI tests fail there only, check that first.
- **Not verified:** loading in specific DAWs, the Standalone and AU builds, and on-screen layout at different sizes.
- **Repository licence:** GPLv3 is in place. Commercial redistribution would need a JUCE licence.
- **No code signing or notarisation.** macOS users will need to approve the plugins on first launch.
- **Feedback tail:** `getTailLengthSeconds` returns 0 in all plugins.
- **README lint:** MD060 table-spacing warnings. Cosmetic.

---

## 11. Gotchas (verified in this codebase)

- `juce::dsp::FFT::perform` with `inverse = true` divides by N already.
- `juce::dsp::WindowingFunction` normalises by default. Pass `normalise = false`, and measure the overlap gain numerically.
- `juce::dsp::Oversampling<float>(channels, factor, type, isMaxQuality, useIntegerLatency)`: factor 1 means 2×. `processSamplesUp` takes `AudioBlock<const float>`. `processSamplesDown` takes a non-const `AudioBlock<float>&`.
- `AudioPlayHead::getPosition()` returns `juce::Optional<PositionInfo>`, not `std::optional`.
- `ParameterAttachment` callbacks and `setValueAsCompleteGesture` both work with **denormalised** values. For a choice parameter that is the index as a float.
- `juce_add_plugin`: `PLUGIN_NAME` maps to `JUCE_PLUGIN_NAME`. `MicroTimingSeq` sets `IS_MIDI_EFFECT TRUE` and `NEEDS_MIDI_INPUT/OUTPUT TRUE`.
- `JUCE_PLUGINHOST_VST3` is 0 by default in JUCE 7. It must be 1 to use the VST3 host in a test.
- `MessageManager::runDispatchLoopUntil` exists only when `JUCE_MODAL_LOOPS_PERMITTED` is 1.
- `juce::MouseEvent` in 7.0.9 takes 15 arguments, with no pen details.
- Clang rejects a nested struct's default member initialiser used as a default argument in its enclosing class. `LorenzState` is at namespace scope for this reason.
- `juce::File bundle (juce::String (argv[1]));` is a function declaration (most vexing parse). Use braces.
- Duplicate-symbol link errors with JUCE: see section 8.
- `add_library(... STATIC)` linking JUCE modules bundles them. `OBJECT` libraries have the same problem.
- The root `CMakeLists.txt` must `include()` `SuiteUI.cmake` before any plugin subdirectory, because `suite_ui_add_to_target` is a function.

---

## 12. Deviations from the spec

The spec in `coding_agent_prompt.md` is the original brief. This is what differs and why.

| Spec | Built | Why |
|---|---|---|
| Layout lists only `modules/core_dsp` | Adds `modules/suite_ui` and `tests/` | Shared UI for three editors, and integration tests |
| Tests use `assert` | `assert` with `NDEBUG` undefined, plus plugin and host tests | Release builds would otherwise drop the asserts |
| `SpectralEngine.h`, `SaturationEngine.h`, `ChaosEngine.h` in `CoreDSP` | Same, plus `CoreDSP.h` umbrella | As specified |
| Spectral Drift: four parameters | Eleven. The four named ones are unchanged | User request for more FX controls |
| Dynamic Saturation: five parameters | Thirteen | User request for eight more |
| MIDI effect, no audio buses | MIDI effect with stereo pass-through | Some DAW scanners drop audio-less VST3 effects |
| Output spectrum on the display | Measured on finished output (one frame behind) | Shows the real overlap-add result |

---

## 13. Quick reference

- Build: `cmake -S . -B build -A x64`, then `cmake --build build --config Release`.
- Test: `ctest --test-dir build -C Release --output-on-failure`.
- Bundles: `build/plugins/<Name>/<Name>_artefacts/Release/VST3/`.
- Release: GitHub Actions → *Release* → *Run workflow* (version without `v`, draft on by default).
