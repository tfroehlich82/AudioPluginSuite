# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Spectral Drift Shifter: FFT overlap-add phase-drift effect with rate, mix, feedback and freeze.
- Dynamic Saturation Companion: envelope-driven Tube, Tape and Diode saturation with mix and output level.
- Micro-Timing Chaos Sequencer: MIDI effect that shifts note timing and velocity with a Lorenz attractor.
- `CoreDSP` header-only DSP module with unit tests (`CoreDSPTests`).
- `SuiteUI` module: shared look and feel, spectrum, phase-space, transfer-curve, meter and segmented-selector widgets.
- CMake build for Windows (MSVC) and macOS (Xcode), with JUCE 7.0.9 fetched at configure time.
- GitHub Actions workflow that builds, tests and uploads VST3 bundles.
- README, developer notes (`CLAUDE.md`) and this changelog.
