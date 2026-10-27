# CLAUDE.md: AudioPluginSuite

Working notes for AI coding agents. Read this first. The full spec is in `coding_agent_prompt.md`; this file records what is actually built and the decisions that are not obvious from the code.

## What this is

Three JUCE 7.0.9 plugins in C++17: VST3 (and AU on macOS) plus Standalone.

| Plugin | Folder | Kind | Processor / DSP core |
|---|---|---|---|
| Spectral Drift Shifter | `plugins/SpectralDrift` | audio effect | `CoreDSP::Spectral::DriftEngine` |
| Dynamic Saturation Companion | `plugins/DynamicSaturation` | audio effect | `CoreDSP::Saturation` |
| Micro-Timing Chaos Sequencer | `plugins/MicroTimingSeq` | MIDI effect (no audio buses) | `CoreDSP::Chaos::LorenzAttractor` |

Targets: Windows (MSVC, VS 2022 or later, x64) and macOS (Xcode). The owner works on Windows, in VS Code with CMake Tools. The existing `build/` directory was configured with the `Visual Studio 18 2026` generator and clang-cl.

## Layout

```
modules/core_dsp/      header-only DSP (depends on juce::juce_dsp only); tests in tests/DSPTests.cpp
modules/suite_ui/      shared LookAndFeel and widgets (added beyond the spec's layout; see decisions)
plugins/<Name>/        CMakeLists.txt, Source/PluginProcessor.{h,cpp}, Source/PluginEditor.{h,cpp}

.github/workflows/build.yml
```

## Build and test

```powershell
# configure (plugins on; use -DSUITE_BUILD_PLUGINS=OFF for DSP-only)
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target CoreDSPTests
cmake --build build --config Release --parallel 8
ctest --test-dir build -C Release --output-on-failure
```

CMake at `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe` (not on the bash PATH). JUCE is fetched by `FetchContent` into `build/_deps/juce-src`, which is a local copy you can grep for API checks.

Build status at the last check: full Release build succeeds on Windows; all 6 CoreDSP tests pass; three `.vst3` bundles built.

## Decisions and why

- **JUCE pinned to 7.0.9.** Changing the version can break `juce_add_plugin` properties and APIs such as `ParameterAttachment::setValueAsCompleteGesture`, which takes a denormalised value.
- **Tests undefine `NDEBUG`** before `<cassert>`. Release builds (what CI runs) would otherwise compile every `assert` away.
- **`suite_ui` module added.** The spec's layout has no shared UI folder, but three editors need the same LookAndFeel and widgets. Duplicating them would be worse. Treat this as an accepted deviation.
- **`CoreDSP` is an INTERFACE library** that links `juce_dsp`. Spec requires `juce::dsp::FFT`, so the DSP core cannot be JUCE-free.
- **Spectral latency = 2048 samples**, reported with `setLatencySamples`. The dry path is delayed by the same amount so mix stays phase-aligned.
- **Output spectrum display** is measured on the already-finished output ring, one frame behind, so it shows the real overlap-add result rather than the synthesised frame.
- **Feedback** goes through `tanh` before re-entering the input. Loop gain stays bounded even at 95 %.
- **Dynamic Saturation envelope is linked across channels** so the stereo image is preserved.
- **Micro-Timing latency is fixed at 50 ms** (`maxTimingOffsetMs`), regardless of the offset parameter, so latency never changes mid-session. Early notes are scheduled at `event + latency + offset`. Note-offs are clamped to never precede their note-on, which avoids stuck notes.
- **BPM Sync**: *Free* advances the attractor at 2 Lorenz units per second. *Host Sync* advances 0.5 units per quarter note from PPQ deltas, ignores jumps over 4 beats, and holds still when the transport stops. With no playhead info it falls back to Free.
- **Parameter IDs are stable.** They are saved in sessions. Renaming one breaks recall. Plugin codes (`Sdft`, `Dsat`, `Mtsq`) and manufacturer code `Asui` must also never change after release.
- **Audio→UI data** goes through `std::atomic` arrays only. The audio thread must never block on the UI.
- **Copy after build is off** (`COPY_PLUGIN_AFTER_BUILD FALSE`). The JUCE default writes into Program Files, which can fail the build without admin rights.

## Conventions

- English everywhere: code, comments, CI, docs. The original CI and some comments were German; that was changed.
- Namespaces `CoreDSP::…` and `SuiteUI::…`.
- No allocation or locking on the audio thread. Allocate in constructors or `prepareToPlay`.
- Per-sample functions are `noexcept`. Every shaper output is in [-1, 1].
- Do not commit or push unless the owner asks.

## Open items

- **License: GPLv3** (`LICENSE` at repo root, official GNU text, unmodified). JUCE is used under its GPLv3 option. Closed-source distribution would need a commercial JUCE licence.
- **GitHub owner/repo name unknown.** README badges and links use the placeholder `OWNER/REPO`. Replace them once the repository exists.
- **CI macOS job is pinned to `macos-14` with Xcode 15.4.** `macos-latest` failed to compile JUCE 7.0.9's `juce_gui_basics.mm` while building `juceaide`. The cause is unconfirmed. If the pin fails, try JUCE 7.0.12 (patch release, same API).
- **No code signing or notarisation** (macOS plugins will need approval on first launch).
- **Saturation has no oversampling.** Aliasing is expected at high drive.
- **Feedback tail not reported** (`getTailLengthSeconds` returns 0).
- Lint: README tables trigger MD060 (compact spacing) warnings. Cosmetic.
- Not yet verified by running: the plugins in a DAW, Standalone launch, the macOS build, and the Release CI workflow on GitHub.

## Gotchas

- `juce::dsp::FFT::perform` inverse output is already divided by N. Do not normalise again.
- JUCE's `WindowingFunction` normalises by default. This project passes `normalise = false` and derives the overlap gain numerically.
- In `juce_add_plugin`, `PLUGIN_NAME` maps to `JUCE_PLUGIN_NAME`. `MicroTimingSeq` sets `IS_MIDI_EFFECT TRUE` and has no audio buses.
- `AudioProcessor::getPlayHead()->getPosition()` returns `juce::Optional`, not `std::optional`.
- A Clang rule blocks a nested struct's default member initialiser being used as a default argument in its enclosing class. `LorenzState` is therefore namespace-scope.
