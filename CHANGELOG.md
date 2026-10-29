# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Spectral Drift Shifter: three new controls. Feedback Damping, Drift Start and Output Gain.

- Spectral Drift Shifter: seven new controls. Drift Depth, Bin Spread, Stereo Phase, LFO Shape (sine, triangle, saw, square), Spectral Shift (±16 bins), Spectral Tilt and Spectral Gate.
- Dynamic Saturation Companion: eight new controls. Attack, Release, Detect HPF, Stereo Link, Tube Bias, Fold, Tone and Envelope Curve.
- Dynamic Saturation Companion now runs its shaper at 2× oversampling. The latency is reported to the host.
- Plugin test executables for all three plugins. They cover parameters, state, latency, bus layouts, audio behaviour, MIDI timing and the editor.
- Host tests that load each built VST3 bundle through JUCE's own plugin host, then open the editor and process audio and MIDI.
- Unit tests for the shared UI widgets (`SuiteUITests`).
- Shared test helpers in `tests/common/PluginTestUtils.h`.
- `AGENTS.md`, the project knowledge base for coding agents.
- `release.yml`, a manual workflow that builds, tests, packages and publishes a GitHub release.
- GNU GPL v3 licence (`LICENSE`).

### Changed
- Spectral Drift Shifter: the Feedback range is reduced from 0–95 % to 0–12 %, about one eighth. The old range was too aggressive to use. Existing sessions saved at higher values are clamped by the new range.

- Spectral Drift Shifter: the preview now says "Waiting for audio" until signal arrives, and has frequency labels. Before, a silent input left the panel blank with nothing to explain it.
- Micro-Timing Chaos Sequencer: declares a stereo audio pass-through bus, so VST3 hosts that drop audio-less effects can load it. The MIDI behaviour is unchanged.
- The UI module (`SuiteUI`) is compiled into each consumer rather than built as a library. This avoids duplicate-symbol link errors with JUCE.
- The CI workflow builds before it runs the tests, since CTest needs the test executables to exist.
- The macOS CI job is pinned to `macos-14` with Xcode 15.4.
- Windows CI and release builds use the newest installed Visual Studio instead of a pinned year.
- Tests always run with `NDEBUG` undefined, so checks also run in Release builds.

### Fixed

- Dynamic Saturation Companion: the output is held to ±1 before the output level, so oversampler and tone overshoot cannot exceed the stage ceiling.
- Micro-Timing Chaos Sequencer: note-offs are clamped so they never precede their note-on.
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
