# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Spectral Drift Shifter: fourteen controls in total. Drift Depth, Bin Spread, Stereo Phase, LFO Shape (sine, triangle, saw, square), Spectral Shift (±32 bins), Spectral Tilt (±2), Spectral Gate, Feedback Damping, Drift Start and Output Gain.
- Dynamic Saturation Companion: thirteen controls. Attack, Release, Detect HPF, Stereo Link, Tube Bias, Fold, Tone and Envelope Curve are new.
- Dynamic Saturation Companion now runs its shaper at 2× oversampling. The latency is reported to the host.
- Micro-Timing Chaos Sequencer: a lerp curve with five draggable points. Drag a point to shape it, right-click a point to reset it, and double-click the graph to reset the curve. The curve is saved with the session.
- Micro-Timing Chaos Sequencer: a Lerp control, and X, Y and Z multipliers. Lerp scales how far each step drifts toward the attractor. Each multiplier scales one axis (timing, pitch and gate, velocity).
- Micro-Timing Chaos Sequencer: a Curve and Preview tab. Preview shows the live drift for each axis and the timing, pitch and velocity of the last 32 steps.
- Micro-Timing Chaos Sequencer: the BPM Sync button shows an icon and a texture when active, and the host tempo is shown under it. The Tempo knob is disabled while the sync is on.
- Micro-Timing Chaos Sequencer: a help line at the bottom of the editor describes the control under the pointer, and the knobs have tooltips.
- Micro-Timing Chaos Sequencer: the editor is grouped into Pattern, Melody, Chaos, Lerp Curve and Transport.
- Plugin test executables for all three plugins. They cover parameters, state, latency, bus layouts, audio behaviour, MIDI timing and the editor.
- Host tests that load each built VST3 bundle through JUCE's own plugin host, then open the editor and process audio and MIDI.
- Unit tests for the shared UI widgets (`SuiteUITests`).
- Shared test helpers in `tests/common/PluginTestUtils.h`.
- `AGENTS.md`, the project knowledge base for coding agents.
- `release.yml`, a manual workflow that builds, tests, packages and publishes a GitHub release.
- GNU GPL v3 licence (`LICENSE`).

### Changed
- Spectral Drift Shifter: the Feedback control runs from 0 to 20 %. Processing still caps the feedback at 12 %, so values above 12 % act as 12 %.
- Spectral Drift Shifter: Bin Spread is 0 to 1 rad (was 0 to 0.5), Spectral Shift is ±32 bins (was ±16), and Spectral Tilt is ±2 (was ±1).
- Spectral Drift Shifter: the preview says "Waiting for audio" until signal arrives, and has frequency labels. Before, a silent input left the panel blank with nothing to explain it.
- Dynamic Saturation Companion: Attack is 1 to 100 ms (was 0.1 to 50), Release is 50 to 2000 ms (was 10 to 1000), Envelope Curve is 0.5 to 3.0 (was 0.5 to 2.0), Tube Bias is 0 to 0.25 (was 0 to 0.5), and Fold is 0 to 50 % (was 0 to 100 %).
- Micro-Timing Chaos Sequencer: Swing is documented as −20 to +20 % (the README said 0 to 50 %). Negative values move the odd steps early.
- Micro-Timing Chaos Sequencer: Chaos now drives the lerp curve. Timing, pitch and velocity drift are scaled by Lerp and the multipliers, so at the defaults the sequencer is quieter than before. Velocity variation at the defaults is about ±7 %, against about ±20 % before.
- Micro-Timing Chaos Sequencer: the Chaos Speed control is now Speed, 0.5 to 2.0, default 1.0 (was 0 to 0.5 per beat, default 0.5).
- Micro-Timing Chaos Sequencer: declares a stereo audio pass-through bus, so VST3 hosts that drop audio-less effects can load it. The MIDI behaviour is unchanged.
- The UI module (`SuiteUI`) is compiled into each consumer rather than built as a library. This avoids duplicate-symbol link errors with JUCE.
- The CI workflow builds before it runs the tests, since CTest needs the test executables to exist.
- The macOS CI job is pinned to `macos-14` with Xcode 15.4.
- Windows CI and release builds use the newest installed Visual Studio instead of a pinned year.
- Tests always run with `NDEBUG` undefined, so checks also run in Release builds.

### Fixed

- Dynamic Saturation Companion: the output is held to ±1 before the output level, so oversampler and tone overshoot cannot exceed the stage ceiling.
- Micro-Timing Chaos Sequencer: note-offs are clamped so they never precede their note-on.
- Micro-Timing Chaos Sequencer: negative swing was ignored, because the engine clamped swing to 0 to 1.
- Micro-Timing Chaos Sequencer: the icons and textures never loaded, because the plugin resource properties are not JUCE 7.0.9 options. They are now embedded in the binary.
- Plugin tests: the expected parameter ranges were out of date with the processors (Spectral Drift and Dynamic Saturation), and now match them.
- The CI workflow no longer fails because of the Visual Studio version in the runner image.

## [1.0.0] - Initial release candidate

### Added

- Spectral Drift Shifter: FFT overlap-add phase-drift effect with rate, mix, feedback and freeze.
- Dynamic Saturation Companion: envelope-driven Tube, Tape and Diode saturation with mix and output level.
- Micro-Timing Chaos Sequencer: MIDI effect that shifts note timing and velocity with a Lorenz attractor.
- `CoreDSP` header-only DSP module with unit tests (`CoreDSPTests`).
- `SuiteUI` module: shared look and feel, spectrum, phase-space, transfer-curve, meter and segmented-selector widgets.
- CMake build for Windows (MSVC) and macOS (Xcode), with JUCE 7.0.9 fetched at configure time.
- GitHub Actions workflow that builds, tests and uploads VST3 bundles.
- README and developer notes.
