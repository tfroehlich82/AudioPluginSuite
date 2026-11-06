# AGENTS.md: Audio Plugin Suite

Knowledge base for coding agents working on this repository. Read this before changing anything. It records what is built, why it is built that way, what is verified and what is not, and the JUCE and CMake pitfalls already hit.

Keep this file in step with the code. When you change a parameter range, a default, a latency, a test count, or a file layout, update the matching section in the same change. The user-facing docs are `README.md`, `CHANGELOG.md` and `plugins/MicroTimingChaos/README.md`.

The original brief (`coding_agent_prompt.md`) is no longer in the repository. Section 12 lists where the built code differs from it.

---

## 1. What this project is

Three audio plugins written with JUCE 9.0.5 in C++17. Formats: VST3 on Windows and macOS, Audio Unit on macOS, and Standalone on both.

| Plugin | Folder | Kind | DSP core | Editor accent |
|---|---|---|---|---|
| Spectral Drift Shifter | `plugins/SpectralDrift` | audio effect | `CoreDSP::Spectral::DriftEngine` | cyan `#2DE2FF` |
| Dynamic Saturation Companion | `plugins/DynamicSaturation` | audio effect | `CoreDSP::Saturation` | amber `#FF9F1C` |
| Micro-Timing Chaos Sequencer | `plugins/MicroTimingChaos` | MIDI effect, with stereo pass-through | `CoreDSP::Chaos::LorenzAttractor`, `CoreDSP::Sequencer` | lime `#7DFF5A` |

- Manufacturer name `AudioSuite`, manufacturer code `Asui`, bundle IDs `com.audiosuite.<name>`.
- Plugin codes `Sdft`, `Dsat`, `Mtch`. The Micro-Timing CMake file sets `Mtch`, but this document previously said `Mtsq`. Confirm which one is intended before a release. Codes and parameter IDs are saved in sessions and **must never change** after a release.
- Licence: GNU GPL v3 (`LICENSE`, the unmodified GNU text). JUCE is used under its GPLv3 option. Closed-source distribution would need a commercial JUCE licence.
- Repository: `https://github.com/tfroehlich82/AudioPluginSuite`. The `origin` remote points there.
- The owner works on Windows in VS Code with CMake Tools. The existing `build/` cache uses the `Visual Studio 18 2026` generator with the `clang-cl` compiler.

---

## 2. Repository layout

```
CMakeLists.txt                    Root: JUCE pin, global definitions, options, SUITE_PLUGIN_FORMATS
AGENTS.md / CLAUDE.md             This file / Claude Code specifics (points here)
README.md, CHANGELOG.md, LICENSE  User and developer docs, history, licence
modules/
  core_dsp/                       Header-only DSP, depends on juce::juce_dsp
    include/CoreDSP.h             Umbrella header; decibelsToGain()
    include/SaturationEngine.h    Shapers, fold, OnePoleFilter, ToneControl, EnvelopeFollower
    include/ChaosEngine.h         LorenzAttractor (RK4), LorenzParams, LorenzState
    include/SpectralEngine.h      DriftEngine (overlap-add FFT), LfoShape, lfoValue()
    include/SequencerEngine.h     CoreDSP::Sequencer: step rates, hashed pattern and melody, timing, swing, gate, velocity
    include/ModulationEngine.h    CoreDSP::Modulation: LFO shapes (cycles in, value in [-1, 1]) and the normalised offset clamp
    include/ModulationTiming.h    CoreDSP::Modulation: division table (beats per cycle), fractionalPart, cyclesFromSeconds
    tests/DSPTests.cpp            CoreDSPTests (29 test functions)
    CMakeLists.txt                CoreDSP INTERFACE target, CoreDSPTests
  suite_modulation/               Shared LFOs, routing slots, LFO scope, modulation panel, control display. NOT a library target, see section 8.
    include/SuiteModulationRouter.h   Router: LFO and slot parameters (addParameters), state, offsets, modulate(), publish
    include/SuiteModulationScope.h    LfoScope: one LFO cycle with the current position
    include/SuiteModulationPanel.h    ModulationPanel: LFO strips and the six routing rows (uses SuiteUI::LabelledSlider)
    include/SuiteModulationControls.h ControlDisplay: shows modulated values on sliders, skips the one under the mouse
    Source/*.cpp                  Implementations
    tests/SuiteModulationTests.cpp  SuiteModulationTests (9 test functions), with a stub processor
    suite_modulation.cmake        suite_modulation_add_to_target(<target>): adds sources + include dir. Needs suite_ui_add_to_target on the same target
  suite_ui/                       Shared UI. NOT a library target, see section 8
    include/SuiteLookAndFeel.h    Colours namespace, SuiteLookAndFeel (LookAndFeel_V4)
    include/SuiteWidgets.h        SpectrumDisplay, TrailPlot, CurveView, LevelMeter, SegmentedSelector, LabelledSlider
    Source/*.cpp                  Implementations
    SuiteUI.cmake                 suite_ui_add_to_target(<target>): adds sources + include dir
    tests/SuiteUITests.cpp        SuiteUITests (9 test functions)
    CMakeLists.txt                SuiteUITests only
plugins/<Name>/
  CMakeLists.txt                  juce_add_plugin, links, <Name>Tests executable, add_test
  Source/PluginProcessor.{h,cpp}  APVTS, processing, state, createPluginFilter()
  Source/PluginEditor.{h,cpp}     Editor, 60 Hz Timer reads processor snapshots
  tests/PluginTests.cpp           Plugin tests: SpectralDrift 13, DynamicSaturation 10, MicroTimingChaos 34
plugins/MicroTimingChaos/         Additional files:
  MicroTimingChaosLookAndFeel.h   At the plugin root (not include/). Palette namespace and LookAndFeel
  Source/MicroTimingChaosLookAndFeel.cpp  Rotary knobs, BPM Sync button, combo box; textures and icons from BinaryData
  Source/LerpCurveEditor.{h,cpp}  The 5-point curve editor (Curve tab)
  Source/LerpPreview.{h,cpp}      The live drift preview (Preview tab)
  The LFO panel and scope are in modules/suite_modulation. The plugin's own copies were removed.
  Resources/icons/*.svg           BPM Sync icons, embedded in the binary
  Resources/textures/*.png        4K textures, embedded and scaled down at load
  generate_texture.py, make_png.py  Scripts that produced the textures (owner's tools)
  README.md                       User documentation for this plugin
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
- `-DSUITE_BUILD_PLUGINS=OFF` builds only CoreDSP and its tests. The SuiteUI tests are only registered when plugins are on.
- macOS CI runs `macos-14` with Xcode 15.4 (see [section 10](#10-open-items-and-known-risks)).
- JUCE is fetched by `FetchContent` into `build/_deps/juce-src`. That directory is a full local copy: grep it to check any API before using it.
- Asset embedding: `juce_add_binary_data(MicroTimingChaosAssets ...)` builds a `BinaryData` target that the plugin and its test executable link. Add new icons or textures to its `SOURCES` list, then reference them by the generated name (`file_name_with_underscores`, for example `bpm_sync_active_svg`).

Last verified status (2026-11-06): JUCE 9.0.5. Full Release build succeeds on Windows. All 9 CTest entries pass: CoreDSPTests, SuiteUI_Tests, SuiteModulation_Tests, the three plugin test executables, and the three VST3 host-load tests. SuiteUI_Tests also passes against JUCE 8.0.6, the previous pin, which is the version CI ran. macOS is not verified on JUCE 9.

---

## 4. Parameters

Parameter IDs are `static constexpr const char*` members of each processor. Defaults and ranges below are what `createParameterLayout()` defines. Each float uses a `NormalisableRange`, and some use a log or skew. The plugin tests check these values with `checkRange`, so a change here must change the test too.

### 4.1 Spectral Drift Shifter (14 parameters)

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `driftRate` | Drift Rate | 0.1 – 10 Hz, log | 1 Hz | LFO speed |
| `depth` | Drift Depth | 0 – 100 % | 19.1 % | Maps to 0 – π rad in the engine. 19.1 % = 0.6 rad |
| `spread` | Bin Spread | 0 – 1 rad | 0.05 | LFO phase step between neighbouring bins |
| `stereoPhase` | Stereo Phase | 0 – 180° | 90° | Offset of the right channel's LFO |
| `lfoShape` | LFO Shape | choice: Sine, Triangle, Saw, Square | Sine | `CoreDSP::Spectral::LfoShape` |
| `shift` | Spectral Shift | −32 – +32 bins | 0 | Magnitudes read from bin k − shift, linear interpolation |
| `tilt` | Spectral Tilt | −2 – +2 | 0 | Gain (k / 32)^tilt, clamped to [1/16, 8] |
| `gate` | Spectral Gate | −90 – 0 dB | −90 | At −90 the gate is open for every bin |
| `feedback` | Feedback | 0 – 20 % | 0 % | The range is 20 %, but processing still caps the value at `maxFeedbackPercent` = 12 %. See section 10. Engine accepts up to 0.95. Fed back through tanh |
| `feedbackDamping` | Feedback Damping | 0 – 100 % | 30 % | One-pole low-pass in the feedback loop. 0 % passes the loop unchanged |
| `driftStart` | Drift Start | 0 – 100 % | 0 % | Bins below this fraction of the spectrum are not drifted |
| `outputGain` | Output Gain | −12 – +12 dB | 0 dB | Applied to the whole output after the engines |
| `mix` | Mix | 0 – 100 % | 100 % | Dry is delayed by the frame latency |
| `freeze` | Freeze | bool | off | Holds captured magnitudes, phases still advance |

Latency reported: 2048 samples (`setLatencySamples`). Tail length: 0.

### 4.2 Dynamic Saturation Companion (13 parameters)

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `drive` | Drive | 0 – 24 dB | 6 dB | Pre-gain base, smoothed 50 ms |
| `sensitivity` | Envelope Sensitivity | 0 – 1 | 0.5 | Dynamic Drive = Drive × (1 + Sens × Env) |
| `mode` | Saturation Mode | choice: Tube, Tape, Diode | Tube | |
| `mix` | Mix | 0 – 100 % | 100 % | Smoothed 50 ms |
| `outputLevel` | Output Level | −12 – +6 dB | 0 dB | Smoothed 50 ms |
| `attack` | Attack | 1 – 100 ms, skew 0.35 | 5 ms | Envelope rise |
| `release` | Release | 50 – 2000 ms, skew 0.4 | 150 ms | Envelope fall |
| `detectHpf` | Detect HPF | 20 – 800 Hz, log | 20 Hz | High-pass on the detector only |
| `stereoLink` | Stereo Link | 0 – 100 % | 100 % | 100 % = linked envelope (max of channels) |
| `bias` | Tube Bias | 0 – 0.25 | 0.2 | Asymmetry of the tube curve. Tube only |
| `fold` | Fold | 0 – 50 % | 0 % | Wavefolder after shaping, 2× domain |
| `tone` | Tone | −1 – +1 | 0 | Tilt around 1 kHz after the shaper |
| `curve` | Envelope Curve | 0.5 – 3.0 | 1.0 | Envelope exponent |

Latency reported: the oversampler's integer latency, computed in `prepareToPlay`. The dry path is delayed by the same amount. Tail length: 0.

### 4.3 Micro-Timing Chaos Sequencer (46 parameters)

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `rate` | Rate | 1/4, 1/8, 1/16, 1/32 | 1/16 | Step length |
| `length` | Pattern Length | 2 – 16 | 8 | |
| `density` | Density | 0 – 100 % | 70 % | Share of steps that fire, curved by `log1p` |
| `seed` | Seed | 0 – 999 | 42 | Fixes the pattern and the hashed melody |
| `gate` | Gate | 10 – 100 % | 60 % | Note length as a share of the step. Lorenz Y and the pitch drift move it |
| `swing` | Swing | −20 – +20 % | 0 % | Odd steps. Negative moves them early |
| `baseVelocity` | Velocity | 30 – 127 | 100 | Before chaos |
| `root` | Root Note | 24 – 72 | 48 | MIDI note |
| `scale` | Scale | Chromatic, Major, Minor, Minor Pentatonic, Dorian, Whole Tone | Minor | |
| `octaves` | Octaves | 1 – 3 | 2 | |
| `chaos` | Chaos | 0 – 100 % | 50 % | The input to the lerp curve. At 0 nothing moves |
| `timingRange` | Timing Range | 0 – 50 ms | 20 ms | Largest timing shift at full drift. Does not change latency |
| `velocityChaos` | Velocity Chaos | 0 – 100 % | 40 % | Scaled by the Z drift |
| `lerp` | Lerp | 0 – 100 % | 50 % | Global drift amount. 0 turns all drift off |
| `lerpXMult` | Lerp X Mult | 0 – 2 | 1 | Timing drift multiplier |
| `lerpYMult` | Lerp Y Mult | 0 – 2 | 1 | Pitch and gate drift multiplier |
| `lerpZMult` | Lerp Z Mult | 0 – 2 | 1 | Velocity drift multiplier |
| `bpm` | Tempo | 40 – 240 BPM | 120 | Used when Host Sync is off, and disabled while it is on |
| `sync` | Host Sync | off / on | off | |
| `speed` | Speed | 0.5 – 2.0 | 1.0 | Attractor speed relative to the tempo |
| `lfo1Rate` / `lfo2Rate` | LFO Rate | 0.02 – 20 Hz, log | 0.25 Hz | Used when that LFO's Sync is off. Disabled in the UI while synced |
| `lfo1Shape` / `lfo2Shape` | LFO Shape | Sine, Triangle, Saw, Square, Sample & Hold, Random, Noise | Sine | Index 0 to 6 |
| `lfo1Sync` / `lfo2Sync` | LFO Sync | off / on | off | Locks the LFO to musical time |
| `lfo1Division` / `lfo2Division` | LFO Division | 4 bars, 2 bars, 1 bar, 1/2, 1/4, 1/8, 1/16 | 1/4 | Cycle length: 16, 8, 4, 2, 1, 0.5, 0.25 beats. Used while synced |
| `mod1Source` … `mod6Source` | Mod Source | LFO 1, LFO 2 | LFO 1 | Which LFO drives the slot |
| `mod1Target` … `mod6Target` | Mod Target | the 14 targets in section 4.4 | None | Index 0 to 14. Append-only, see 4.4 |
| `mod1Amount` … `mod6Amount` | Mod Amount | −100 – +100 % | 0 % | Signed. 0 % leaves the slot idle |

The lerp curve is **not** a parameter. It is five points (x, y) stored as the `lerpCurve` property of the APVTS state (`"x,y;x,y;..."`, four decimals). `loadLerpCurveFromState()` reads it on construction and after `setStateInformation`. The first and last x are fixed at 0 and 1. The default is `y = log10(1 + 9x)`, sampled at x = 0, 0.25, 0.5, 0.75, 1.

Latency reported: 50 ms fixed (`maxTimingRangeMs`, 2400 samples at 48 kHz), regardless of Timing Range. Tail length: 0.

---

### 4.4 LFOs and modulation routing

Two LFOs and six routing slots. A slot adds `amount × LFO value` to its target, in the target's normalised range (0 to 1 across the parameter's range, so log and bipolar ranges behave the same). The result is clamped to [0, 1], and converted back with the parameter's own `convertFrom0to1`. With no slot aimed at a target, the base value is returned exactly, which is what keeps the committed output unchanged at the defaults.

The LFO and routing parameters are created by `SuiteModulation::Router::addParameters`, which every plugin that uses modulation calls. The target table is per plugin: the plugin passes its own `TargetInfo` list to the router, and the choice index is the position in that list plus one (0 is None). The table order is saved in sessions, so it is append-only per plugin.

Targets (`ModTarget` in `PluginProcessor.h`; index order is saved in sessions, so append only): 0 None, 1 Chaos, 2 Timing Range, 3 Velocity Chaos, 4 Lerp, 5 Lerp X Mult, 6 Lerp Y Mult, 7 Lerp Z Mult, 8 Density, 9 Gate, 10 Swing, 11 Velocity (rounded), 12 Speed, 13 Tempo (applies only while Host Sync is off), 14 Seed (changes the pattern and melody). Excluded on purpose: Rate, Length, Scale, Root, Octaves and Host Sync.

LFO phase is counted in cycles. Free LFOs accumulate `rate × samples / sampleRate` at the end of each block, so a rate change never jumps the phase, and they keep running when the transport stops. Synced LFOs use `beat / divisionBeats`, where the beat is the sequencer's beat (host or internal), so they hold still when the transport stops. Sample and hold takes its seed from `lfoSeeds` and its cycle index from the unwrapped cycle count.

LFOs are sampled at step times, not per sample, so a fast LFO can alias against the step grid (section 10).

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
- `frameCount` increments once per frame. The UI uses it, together with the input energy, to decide whether to show "Waiting for audio". The message itself is drawn by the shared `SuiteUI::SpectrumDisplay`.
- The engine feedback is capped at `maxFeedbackPercent` (12 %) in `readSettings`, whatever the parameter range.

Reconstruction test: with drift depth 0, no shift/tilt/gate, feedback 0 and mix 1, the output equals the input delayed by 2048 samples to within 1e-3. This is the main correctness check for the engine.

### 5.2 Dynamic Saturation processing

Three passes in `processBlock`:

1. **Base rate.** Detector high-pass per channel → linked peak → linked envelope and per-channel envelopes, mixed by Stereo Link → shaped by Curve → Dynamic Drive per sample into `preGainBuffer`. Mix and output level are smoothed per sample.
2. **2× oversampled.** `juce::dsp::Oversampling<float>` (half-band FIR, integer latency) upsamples; each oversampled sample is `fold(shape(x × preGain[n/2], mode, bias), fold)`; then downsample back into the buffer.
3. **Base rate.** `wet = clamp(±1, tone(...))`. This ceiling is deliberate: the oversampler and tone can overshoot by about 10 %, and the output test checks a hard ±2.0 after the output gain. Then `mixed = dryDelayed + mix × (wet − dryDelayed)` and `× outputGain`. `dryDelayed` is a ring of `latencySamples`.

Shapers: Tube is `tanh(x + bias)`, re-centred and normalised to [−1, 1]. Tape is a symmetric cubic soft clip, `1.5 · (y − y³/3)` on the clamped input, so the rails sit at ±1. Diode is an exponential curve with a soft knee for positive excursions and a tighter one for negative.

The editor's transfer-curve display (`SuiteUI::CurveView`) redraws with the current drive, bias and fold. Its input meter shows the input level.

### 5.3 Micro-Timing Chaos sequencer

The sequencer is driven by musical time, not by sample count. Each block:

1. **Transport.** With Host Sync on and a host playhead present, the beat position follows the host's PPQ position. A jump of more than 1e-3 beats, or a resync request, resets the step grid. With Host Sync off, the beat position advances by `numSamples / samplesPerBeat`, where `samplesPerBeat` comes from `currentBpm()`. The Tempo parameter is used unless the host reports a tempo while synced. A stopped transport releases every sounding note (`stopAllNotes`) and sets the playhead to −1.
2. **Steps.** While `nextStep × stepLength` lies inside the block, `scheduleStep()` runs for that step:
   - `advanceAttractorTo(stepBeat, speed)` advances the Lorenz attractor by `Δbeats × speed` time units, in chunks of at most `LorenzAttractor::maxAdvance`. The attractor therefore moves per step, so the same pattern gets the same chaos at any tempo. The trail is published once per step.
   - Every control is read through `modulatedValue()` with this step's offsets: Chaos, Timing Range, Speed, the Lerp amount and multipliers, Density, Gate, Swing, Velocity and Velocity Chaos. The lerp curve is then evaluated on the modulated Chaos, so a modulated Chaos moves along the curve. The drifts are published for the Preview tab, and the offsets are published for `getTargetOffset()`.
   - `stepFires(seed, patternIndex, density)` decides whether the step sounds. Silent steps stop here.
   - The pitch is `blendDegree(hashDegree, lorenzDegree, pitchDrift, count)`, mapped to the scale by `notePitch`.
   - Velocity is `velocityFor(baseVelocity, velocityChaos × velocityDrift, z)`.
   - Timing is `timingOffsetMs(timingDrift, rangeMs, x)`. Swing is added on odd steps, and gate is `gateBeats(stepLength, gate, pitchDrift, y)`.
   - `queueNoteOn` places the note-on at `max(streamSample, gridSample + swing + latency + offset)`. If an earlier copy of the same note is still pending, it is cut first, so a retrigger never hangs.
   - `queueNoteOff` is placed at note-on + gate, at least one sample later.
3. **Pass-through.** Incoming MIDI goes into the same queue at `streamSample + samplePosition + latency`. Audio is copied through untouched.
4. **Release.** Queued events due in this block are written to the MIDI buffer with `offset = max(0, time − streamSample)`, and the sounding-note set is kept up to date.

**Lerp curve (hybrid model).** Each axis has a drift amount:

```text
curve        = evaluateLerpCurve(chaos)                 (piecewise linear through the 5 points, 0 to 1)
timingDrift  = lerp × clamp(curve × lerpXMult, 0, 1)
pitchDrift   = lerp × clamp(curve × lerpYMult, 0, 1)
velocityDrift = lerp × clamp(curve × lerpZMult, 0, 1)
```

Chaos at 0 gives `curve = 0`, so nothing moves. Lerp at 0 turns all drift off. A zero multiplier switches its axis off. The three drifts are stored in atomics and read by `getDriftAmounts()`.

**Latency and reporting.** `latencySamples = maxTimingRangeMs × sampleRate / 1000`, reported with `setLatencySamples` in `prepareToPlay`. It is fixed, so a change to Timing Range never changes the reported latency.

Audio: the stereo buffer passes through unchanged (tested).

**Block start.** Before the steps, `processBlock` resyncs to the host position, computes the block-start offsets (for the free tempo), and publishes each LFO's phase and value for the UI. At the end of the block the free LFOs advance.

---

## 6. Test architecture

Nine CTest entries run seven executables: `CoreDSPTests` (29 test functions), `SuiteUI_Tests` (`SuiteUITests`, 9), `SuiteModulation_Tests` (`SuiteModulationTests`, 9), `SpectralDriftTests` (13), `DynamicSaturationTests` (10), `MicroTimingChaosTests` (34), and `PluginHostTests`, which has one ctest entry per plugin (3). The README has the full list.

**Module tests** (`modules/suite_modulation/tests/SuiteModulationTests.cpp`): the router, with a stub processor (no plugin). They cover the routed LFO, the zero case (bit for bit), bipolar amounts, whole-number targets, Host Sync ignoring Tempo, the one-bar period, sample and hold, out-of-range indices, and the control display (values set without notifying). Not covered: the control display skipping a slider the mouse holds, since the test cannot press a mouse button. The panel and scope are checked only by the MicroTimingChaos editor test.

**Assertions.** Every test file starts with `#undef NDEBUG` before `<cassert>`. Release builds (what CI uses) would otherwise compile every `assert` away. Keep this in every new test file.

**DSP tests** (`modules/core_dsp/tests/DSPTests.cpp`): pure functions and engines, no GUI. This includes the sequencer helpers (`blendDegree`, `timingOffsetMs`, `swingBeats`, `gateBeats`, `velocityFor`).

**Plugin tests** (`plugins/<Name>/tests/PluginTests.cpp`): each executable compiles the plugin's own sources directly. The Micro-Timing Chaos plugin keeps its source list in the CMake variable `MICRO_TIMING_CHAOS_SOURCES`, which the plugin and its test executable both use, so a new source file only needs adding in one place. Tests include `tests/common/PluginTestUtils.h`. They run under `juce::ScopedJuceInitialiser_GUI`, and `JUCE_MODAL_LOOPS_PERMITTED=1` is defined for them so `MessageManager::runDispatchLoopUntil` exists. `checkEditor()` runs the message loop so the editor's 60 Hz timer fires, then paints the editor into an image.

The Micro-Timing Chaos tests cover the lerp curve (defaults, point editing and ordering, state round trip) and the hybrid drift (Lerp 0 and a zero X multiplier each switch the axis off). The LFO tests cover the no-modulation regression (an embedded table of the committed event log, which must match exactly), timing modulation and its bipolar symmetry, the Amount 0 case, the synced period against the host beat, the free rate in Hz, sample and hold, every target staying in range, state round trip, latency, and the editor with modulation in use. They do not cover mouse interaction with the curve, tab switching, the BPM readout, or the LFO panel's mouse use.

**Host tests** (`tests/host/PluginHostTests.cpp`): loads the built `.vst3` bundle through `AudioPluginFormatManager` with `JUCE_PLUGINHOST_VST3=1`. This define is needed because the host is compiled out by default in JUCE 7. The test scans, instantiates, opens the editor, prepares and processes. CTest passes the bundle path as `$<TARGET_FILE_DIR:<Plugin>_VST3>/../..`, and each host test depends on its plugin's VST3 target.

**SuiteUI tests** (`modules/suite_ui/tests/SuiteUITests.cpp`): widget logic and painting into images, including the look and feel's drawing paths.

Test helpers you will use: `setParameter(apvts, id, value)` takes denormalised values. `checkRange` checks the normalisable range and the default. `checkParameterIds` checks the count against `numControls`. `checkStateRoundTrip` saves state, changes a value and restores it. `addSine` adds to a buffer using a global sample index.

Mouse events in tests use the 15-argument `juce::MouseEvent` constructor, which still compiles on JUCE 9.0.5.

---

## 7. Decisions and why

- **JUCE pinned to 9.0.5** (was 7.0.9, then 8.0.6). Major versions change APIs and `juce_add_plugin` properties. The 9.0 changes are in section 11. The macOS job has not been verified on 9.0.5. `ParameterAttachment::setValueAsCompleteGesture` takes a denormalised value.
- **`suite_ui` is a module, not in the original layout.** Three editors share the same LookAndFeel and widgets. This is an accepted deviation (section 12).
- **`CoreDSP` is an INTERFACE target linking `juce_dsp`.** The DSP core uses `juce::dsp::FFT`, so it is not JUCE-free.
- **Spectral engine latency = 2048.** The dry path is delayed by the same amount, so mix stays phase-aligned.
- **Dynamic Saturation oversamples 2×** with JUCE's half-band FIR and an integer latency.
- **Output ceiling ±1 before the output level.** Protects against oversampler and tone overshoot. Tested.
- **Micro-Timing has a stereo pass-through bus.** The earlier version declared zero audio buses. The JUCE host loads that version fine, but some DAW scanners drop VST3 audio-effect classes with no audio I/O. This is the most likely cause of the reported DAW load failure. It is a best guess: the DAW failure could not be reproduced here.
- **Micro-Timing latency fixed at 50 ms.** Notes can be early or late by up to 50 ms without the reported latency changing. The earlier plan said latency should equal Timing Range; that was not implemented (section 12).
- **Lerp curve stored in the state, not as parameters.** Five points would have added ten host-visible parameters. Stored as one state property, they save with the session and keep the parameter list stable. The end points keep x fixed, so the curve always spans Chaos 0 to 1.
- **Chaos feeds the curve; Lerp and the multipliers scale the drift.** Chaos no longer drives the melody blend or the timing directly. At the defaults, timing drift is about 37 % (curve 0.74 × Lerp 0.5), so the sequencer is quieter than the earlier versions. Velocity variation is smaller too: about ±7 % at defaults, against ±20 % before.
- **Swing is bipolar** (−20 to +20 %). The engine clamps swing to [−1, 1], so negative values move odd steps early. The earlier clamp to [0, 1] silently ignored negative swing.
- **Assets are embedded with `juce_add_binary_data`.** The `PLUGIN_RESOURCE_DIR` and `PLUGIN_RESOURCE_FILES` arguments are not JUCE 7.0.9 options and do nothing. Embedding makes the plugin load its textures and icons from inside the bundle.
- **Note-offs clamped to note-ons** in the sequencer, and a retrigger cuts the earlier copy first. Prevents stuck notes.
- **Atomics only between audio and UI.** The audio thread never locks.
- **Parameter IDs and plugin codes are permanent.** Renaming breaks saved sessions.
- **`COPY_PLUGIN_AFTER_BUILD FALSE`.** The JUCE default writes into Program Files, which fails without admin rights.
- **Modulation in normalised space.** An amount of ±100 % sweeps the full range of the target, whatever its scale.
- **Discrete targets excluded** (Rate, Length, Scale, Root, Octaves, Host Sync). They would change the length or pitch set abruptly. Seed is a target since commit 5eff266: it is read through `modulatedValue`, so a modulated seed changes the pattern and melody.
- **Tempo modulation applies only while Host Sync is off**, because the host tempo is authoritative when synced.
- **LFO and routing live in `suite_modulation`.** The `Router` uses target indices, not enums, so any plugin can share it. Each plugin supplies its own target table (`TargetInfo`), with `wholeNumber` and `ignoredWhileHostSync` flags. The plugin's `ModTarget` enum keeps its numbers, which are the saved indices. The shared module reproduces MicroTimingChaos's routing exactly. Phase 4 of `docs/plans/suite-modulation-refactor.md` checked this against the editor render (byte-identical).
- **The control display is a module class, not editor code.** `ControlDisplay` takes the router and the list of sliders, and it reads Host Sync through the router. Whole-number targets are rounded inside the router, so the display and the sequencer agree.
- **Modulation applies before the lerp curve**, so a modulated Chaos moves along the curve.
- **The routing and LFO settings are plain APVTS parameters**, so they automate, save and restore like the rest.
- **The LFO panel sits at the bottom of the left column, and the window is 980 px tall.** The plan's right-column option left the lerp curve about 100 px high.
- **Interface typography.** Group titles are 15 pt bold in the accent colour, for every group. Knob captions are 13 pt bold, and value text 14 pt (look and feel `getLabelFont`). Knobs are capped at 110 x 150 px (`knobMaxWidth`, `knobMaxHeight`) and centred in their cells. The root note name is part of the parameter text (`C2 (48)`), so the separate note label is gone.
- **Pattern display.** The sequencer view computes the pattern with the modulated Seed, Density and Timing Range, and `CoreDSP::Sequencer::densityCurve` (shared with the processor). Keep the two in step if either changes.
- **LFO shapes.** Random is smooth value noise with two points per cycle. Noise is three layers of value noise (8, 16 and 32 points per cycle). Sample and hold is one level per cycle. The DSP tests check that Random and Noise are continuous and vary.
- **Live control displays.** Each routable control shows `getModulatedValue()`: its base value moved by the offset published for its target. Offsets are published once per block, so displays follow LFOs in real time. Host automation shows through the base value. While the mouse is held on a control, the display is the user's value, and the modulation shows again on release. Tempo is shown unmodulated while Host Sync is on.
- **Editor sizing (mockup layout).** The layout is designed at 1695 × 1380 (minimum width 1695, minimum height 1202). One gap, `groupGap` = 14 px, separates every pair of groups horizontally and vertically, and sets the outer margin. Left column 830 px (lerp, pattern, and melody + chaos side by side), right column 823 px (attractor, modulation, transport), and the pattern and modulation rows share a height of 299 px. The editor overrides `setScaleFactor` to multiply the host scale by 2/3, so the window shows at two thirds of the design size, about 1133 × 920 by default. The minimum width comes from the melody, chaos and transport row, which needs about 1700 px side by side. Fixed groups: lerp curve 830 × 310 (top left), pattern 830 × 292, modulation 797 × 299 (right), melody 398 × 167, chaos 418 × 167, transport 823 × 167. Only the attractor (top right, width to the window edge) and the step grid (full width, height to the window edge) resize.
- **Spectral Drift preview:** the old panel was blank because, with no audio, the traces sat at the −120 dB floor. The fix adds a "Waiting for audio" state (shown until frames and input energy arrive) and frequency labels. A regression test checks that a 1 kHz tone reaches the snapshot at bin 43.

---

## 8. SuiteUI is compiled into each consumer

`SuiteUI` is **not** a library target. Any consumer calls `suite_ui_add_to_target(<target>)` from `modules/suite_ui/SuiteUI.cmake`, which adds the two `.cpp` files and the include directory.

Reason: a static library that links JUCE module targets bundles those module objects into the archive. Each consumer also links the JUCE modules itself, so the link fails with hundreds of duplicate symbols (`juce::Colour`, `JUCEApplicationBase`, `juce_audio_utils`, and so on). A plain `OBJECT` library had the same problem. Compiling the sources directly avoids it. Consumers still link their JUCE modules explicitly. The test executables link `CoreDSP` and `SuiteUI` but not extra JUCE modules, so they do not need them listed.

If you add a new consumer, use `suite_ui_add_to_target`. Do not add `SuiteUI` to `target_link_libraries`.

`suite_modulation` follows the same rule (`suite_modulation_add_to_target`, never `target_link_libraries`). Its panel uses `SuiteUI::LabelledSlider`, so a consumer calls `suite_ui_add_to_target` on the same target first. Its tests are registered in `modules/suite_modulation/CMakeLists.txt` only when `SUITE_BUILD_TESTS` is on, and the directory is added only inside the `SUITE_BUILD_PLUGINS` block.

---

## 9. Procedure for common changes

**Adding a parameter**
1. Add the ID constant to the processor header, in the matching group.
2. Add the `AudioParameter*` to `createParameterLayout()` with its range and default. Use a `NormalisableRange` lambda for log ranges.
3. Cache its `std::atomic<float>*` in the constructor, and read it in `processBlock` or `readSettings`.
4. Bind a widget in the editor: `SliderAttachment`, `ButtonAttachment`, or a `ParameterAttachment` for choices. Add a `LabelledSlider` to the layout.
5. Update `numControls`, the plugin test's `checkParameterIds` list and its `checkRange` calls. The count must equal the number of parameters the layout creates.
6. Add DSP tests if the engine changed, and a plugin test for the audible effect.
7. Update the README parameter table, section 4 of this file, and `CHANGELOG.md`.

**Changing a parameter range or default.** Update `createParameterLayout()`, the plugin test's `checkRange`, section 4 here, and the user README. Note the change in `CHANGELOG.md`, since saved sessions can be clamped by a new range.

**Changing a DSP engine**
- Add a test in `DSPTests.cpp` first where possible. Keep the reconstruction test passing.
- If latency changes, update `getLatencySamples`, the plugin latency tests, and the README latency table.

**Editing files.** The working tree is edited by the IDE and by the owner. Some files change between reads. Re-read a file before editing it.

**Commits.** Do not commit or push unless the owner asks. Git is initialised, and the origin remote is set.

---

**Adding modulation to a plugin** (from `docs/plans/suite-modulation-refactor.md`, phase 7 procedure)
1. In the plugin's CMake: `suite_ui_add_to_target` then `suite_modulation_add_to_target`, for the plugin and its test executable.
2. Declare a `static constexpr SuiteModulation::TargetInfo` table of the controls the routing may move, in ModTarget order, with `wholeNumber` and `ignoredWhileHostSync` where they apply. Append only.
3. Add `SuiteModulation::Router::addParameters (params, names)` to `createParameterLayout()`, with `names` = `"None"` followed by the target names, in table order.
4. Construct the router in the processor after `apvts`, with the table and the host sync ID (or `nullptr`). Call `prepare`, `computeOffsets`, `publish` and `advanceBlock` as MicroTimingChaos does. Read each routed control through `modulate`.
5. The editor builds `SuiteModulation::ModulationPanel` and `ControlDisplay`. Add the plugin's `numControls` and the parameter tests (`checkParameterIds`, `checkRange`).
6. Record a probe of the unmodulated output before the change, and diff it after. Document the synced-LFO fallback for a plugin with no beat.

## 10. Open items and known risks

- **MicroTiming DAW load:** the reported failure is not reproduced. JUCE's VST3 host loads and opens it, both before and after the stereo bus change. The remedy applied (stereo pass-through) is a best guess. If it still fails in a DAW, ask for the DAW name, its scanner log, and the exact error.
- **Spectral Drift feedback range vs cap.** The Feedback range is 0–20 %, but processing caps the value at `maxFeedbackPercent` = 12, so the 12–20 % part of the knob does nothing. Decide whether the range should be 0–12 % or the cap should be raised to 20 %, then update the code, section 4.1, the README and the changelog together.
- **LFO sampling.** LFOs are evaluated at step times, so an LFO faster than the step rate aliases against the grid. Tempo and Speed are evaluated per block and per step respectively.
- **Discrete targets not modulated:** Root and Octaves (sample and hold on these is a possible follow-up), and Rate, Length and Scale.
- **Spectral Drift and Dynamic Saturation have no modulation.** The shared module is ready for them. Adopting it needs the owner's choice of targets (proposed lists in `docs/plans/suite-modulation-refactor.md`, phase 7). Not started.
- **Module tests do not cover the mouse hold in `ControlDisplay`.** The test cannot press a mouse button. The check is by reading the code only.
- **Micro-Timing Chaos: plan items not built.** Zoom and pan on the curve view. UI automation tests for point dragging, tab switching, the BPM readout and the help line. The plan's "latency = timing range" (deliberately not built, see section 7).
- **Micro-Timing Chaos: not exercised live.** The curve's mouse handling (drag, right-click reset, double-click reset) and hover help were written and compiled but not driven in a running host. The editor was checked by rendering it to an image at the default size (1200 × 820). The screenshot check found and fixed a header overlap and a readout collision.
- **macOS CI** is pinned to `macos-14` with Xcode 15.4. The `macos-latest` image failed to compile JUCE 7.0.9's `juce_gui_basics.mm` while building `juceaide`. The cause is unconfirmed. If the pin fails, move to JUCE 7.0.12 (patch release, same API).
- **GUI tests on CI.** The plugin and SuiteUI tests create editors, so CI needs a desktop session. Windows and macOS hosted runners normally have one. If the GUI tests fail there only, check that first.
- **Not verified:** loading in specific DAWs, and the Standalone and AU builds. The on-screen layout was checked only at the default size.
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
- `juce_add_plugin` has no `PLUGIN_RESOURCE_DIR` or `PLUGIN_RESOURCE_FILES` in JUCE 7.0.9. Those arguments are silently ignored. Embed assets with `juce_add_binary_data` and load them through `BinaryData::getNamedResource` (names like `sequencer_bg_4k_png`, `bpm_sync_active_svg`).
- `juce_add_plugin`: `PLUGIN_NAME` maps to `JUCE_PLUGIN_NAME`. `IS_MIDI_EFFECT TRUE` and `NEEDS_MIDI_INPUT/OUTPUT TRUE` make a MIDI effect.
- `juce::Vector3D` is in `juce_opengl`, which the plugins do not link. Use a small struct for three values.
- A namespace cannot be aliased with `using`. Write `namespace Palette = MicroTimingChaos::Palette;`.
- `LookAndFeel_V3` (the base of `LookAndFeel_V4`) has a private member `backgroundTexture()`. A free function with the same name, called inside a `LookAndFeel` method, resolves to the member. Name helpers differently.
- `juce::Slider` and `juce::ComboBox` are `SettableTooltipClient`s, so they take `setTooltip`. `juce::Component` takes `setHelpText` and `getHelpText`, which a mouse listener can read from `e.eventComponent`.
- `juce::Graphics::drawText` takes a `Rectangle<float>` or a `Rectangle<int>`, and `juce::String (float, int)` formats decimals.
- `juce::MouseEvent` has `isPopupMenu()` for right-click on Windows.
- `JUCE_PLUGINHOST_VST3` is 0 by default in JUCE 9, as in 7. It must be 1 to use the VST3 host in a test.
- `MessageManager::runDispatchLoopUntil` exists only when `JUCE_MODAL_LOOPS_PERMITTED` is 1.
- `juce::MouseEvent` in 7.0.9 takes 15 arguments, with no pen details. The same 15-argument form compiles in 9.0.5.
- Clang rejects a nested struct's default member initialiser used as a default argument in its enclosing class. `LorenzState` is at namespace scope for this reason.
- `juce::File bundle (juce::String (argv[1]));` is a function declaration (most vexing parse). Use braces.
- Duplicate-symbol link errors with JUCE: see section 8.
- `add_library(... STATIC)` linking JUCE modules bundles them. `OBJECT` libraries have the same problem.
- The root `CMakeLists.txt` must `include()` `SuiteUI.cmake` before any plugin subdirectory, because `suite_ui_add_to_target` is a function.
- `juce::AudioParameterFloat`'s seven-argument constructor (label, category, `stringFromValue`) gives the value box its text. Setting `slider.textFromValueFunction` after the `SliderAttachment` exists does not refresh the box, so put the formatting on the parameter.
- In JUCE 7.0.9 the text box position is `juce::Slider::TextBoxRight`. `juce::TextEntryBoxPosition::TextBoxRight` does not compile.
- A baseline for a processing change: build the committed code in a `git worktree` with `-DFETCHCONTENT_SOURCE_DIR_JUCE=<repo>/build/_deps/juce-src` (no re-download), run the same probe there, and diff against the new build.
- Test failures reported as `Exit code 0xc0000409` with `Assertion failed` are a failed `checkRange` or similar assert. The message names the file and line. Run the executable directly to see the full output.
- `juce::AudioParameterFloat` with the eight-argument form (label, category, `stringFromValue`, `valueFromString`) is deprecated in JUCE 7.0.9 and 9.0.5. MSVC warns with C4996. The shared router uses it as the MicroTimingChaos layout did. Move to the `Attributes` form when the plugins are next touched.
- `juce::Component` has `setHelpText`, not `setTooltip`. The editor's help line reads the help text of the component under the pointer, so copy the exact strings when moving a panel.
- `juce::AudioParameterChoice::choices` is a public member. `getParameter (id)` returns a `juce::RangedAudioParameter*`, so `dynamic_cast` it to read the names.
- A broken test source blocks the whole build, not just its own tests. `cmake --build` stops at the first failing target, and the other executables then run from stale binaries. Before reporting a green CTest run, check that each test executable was built from the current sources.
- **JUCE 8 and later: release a `Graphics` before reading its image.** Drawing through a live `juce::Graphics` is not visible in the `juce::Image` until the `Graphics` is destroyed. Reading pixels inside its scope gives zero. JUCE 7 showed the drawing immediately, which is why the SuiteUI paint check passed on 7.0.9 and failed on CI's 8.0.6 (`Assertion failed: painted > 0`, reported as `Exit code 0xc0000409`). Scope the `Graphics` in every test helper that reads pixels.
- **JUCE 9 editor lifetime.** `AudioProcessor::createEditorIfNeeded()` is deprecated; use `createEditorAndMakeActive()`, which returns nullptr while an editor is active. Before an editor that the test owns is destroyed, call `processor.editorBeingDeleted (editor.get())`. A host's wrapper does this, and `checkEditor` in `tests/common/PluginTestUtils.h` does it too.
- **JUCE 9 removed APIs:** `AudioPluginFormatManager::addDefaultFormats()` (add `juce::VST3PluginFormat` directly); `Drawable::createFromSVG (XmlElement)` (use `createFromSVGString`); `Font::getStringWidthFloat` (use `juce::GlyphArrangement::getStringWidth (font, text)`).
- **JUCE 9 splits the audio processor code** into `juce_audio_processors_headless` (no GUI) and `juce_audio_processors`. The plugins and tests still link `juce_audio_processors`, which is correct.
- **Changing the JUCE tag does not refresh an existing checkout.** FetchContent keeps `build/_deps/juce-src` at its old version. After a version change, delete `build/_deps/juce-src` (or the whole build directory) and reconfigure, then check `JUCE_MAJOR_VERSION` in `juce_StandardHeader.h`. A stale checkout built silently against 7.0.9 when the pin said 9.0.5.

---

## 12. Deviations from the original brief

The original brief (`coding_agent_prompt.md`) is no longer in the repository. This is what the built code does differently, and why.

| Brief | Built | Why |
|---|---|---|
| Layout lists only `modules/core_dsp` | Adds `modules/suite_ui` and `tests/` | Shared UI for three editors, and integration tests |
| Tests use `assert` | `assert` with `NDEBUG` undefined, plus plugin and host tests | Release builds would otherwise drop the asserts |
| `SpectralEngine.h`, `SaturationEngine.h`, `ChaosEngine.h` in `CoreDSP` | Same, plus `CoreDSP.h` umbrella and `SequencerEngine.h` | As specified, plus the sequencer maths |
| Spectral Drift: four parameters | Fourteen. The four named ones are unchanged | User request for more FX controls |
| Dynamic Saturation: five parameters | Thirteen | User request for eight more |
| Micro-Timing: MIDI effect, no audio buses | MIDI effect with stereo pass-through | Some DAW scanners drop audio-less VST3 effects |
| Micro-Timing: latency equals Timing Range (plan) | Fixed 50 ms | Changing the reported latency with a knob would disturb host delay compensation |
| Micro-Timing: five curve points as parameters (plan) | One state property holding the points | Keeps the parameter list small and stable across sessions |
| Micro-Timing: Lerp shown in Lerp and in Transport (plan) | Lerp appears once, in the Lerp Curve group | Avoids two controls for one value |
| Micro-Timing: `LookAndFeel` in `include/` (plan) | In the plugin root, next to the other source headers | The root is on the include path for the plugin and its tests |
| Output spectrum on the display | Measured on finished output (one frame behind) | Shows the real overlap-add result |
| Micro-Timing: LFO panel in the right column, window 940 px (plan, D9) | LFO panel at the bottom of the left column, window 980 px | The right column had no room for a lerp curve |
| Micro-Timing: LFO and routing as APVTS parameters (plan) | 26 new parameters, `numControls` 46 | The plan's design; sessions save them |
| Shared LFO and routing module (refactor plan) | `ControlDisplay` takes the router and the slider list, with no APVTS argument. The router reads Host Sync itself. The panel uses `SuiteUI::LabelledSlider`, so it needs SuiteUI in the same target | The router already knows the target table and Host Sync. A second copy of the sync flag would be a second source of truth |

---

## 13. Quick reference

- Build: `cmake -S . -B build -A x64`, then `cmake --build build --config Release`.
- Test: `ctest --test-dir build -C Release --output-on-failure`.
- Bundles: `build/plugins/<Name>/<Name>_artefacts/Release/VST3/`.
- Release: GitHub Actions → *Release* → *Run workflow* (version without `v`, draft on by default).

---

## Owner Rules

- **Build tooling** CMake is not on the bash PATH. Use the full path: C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe (and ctest.exe in the same folder). See section 3.

- **Files change on disk** The IDE and the owner edit files between reads. The harness reports changes; take the on-disk state as current and re-read before editing.

- **Git** Do not commit or push unless the owner asks.

- **Test expectations** Run the full CTest suite before reporting a change as working. Editor tests need a desktop session, so a headless run can fail for reasons that are not code bugs.

- **Prefer the existing helpers** tests/common/PluginTestUtils.h and the DSP tests already cover most patterns. Reuse them before writing new ones.
