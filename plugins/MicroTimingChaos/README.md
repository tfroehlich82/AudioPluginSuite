# Micro-Timing Chaos

A MIDI step sequencer plugin by AudioSuite.

## Overview

The plugin uses a Lorenz attractor to introduce micro-timing, pitch, and velocity variation to a fixed pattern. Each step's timing, pitch, and velocity are controlled by the attractor's X, Y, and Z coordinates.

### Controls

#### Pattern
- **Rate**: 1/4, 1/8, 1/16, 1/32
- **Pattern Length**: 2–16 steps
- **Density**: 0–100% (fires the pattern)
- **Seed**: 0–999 (random pattern)
- **Gate**: 10–100% (note length)
- **Swing**: −20–20% (groove)
- **Base Velocity**: 30–127

#### Melody
- **Root Note**: 24–72
- **Scale**: Chromatic, Major, Minor, Minor Pentatonic, Dorian, Whole Tone
- **Octaves**: 1–3

#### Chaos
- **Chaos**: 0–100% (attractor evolution)
- **Velocity Chaos**: 0–100% (velocity variation)

#### Lerp Curve
- **Lerp**: 0–100% (drift from fixed pattern to attractor)
- **Lerp Curve**: 5 control points (curve mapping)
- **Lerp X Mult**: 0–2 (timing multiplier)
- **Lerp Y Mult**: 0–2 (pitch multiplier)
- **Lerp Z Mult**: 0–2 (velocity multiplier)
- **View**: Curve / Preview (switchable views)

#### Transport
- **Tempo**: 40–240 BPM
- **BPM Sync**: Free / Host Sync
- **Host Sync**: Adapt the sequencer's timing to the host's BPM

## How the Lerp Curve works

The Lerp parameter controls how far each step's timing, pitch, and velocity drift from the fixed pattern toward the attractor. The curve defines *how* the attractor's X, Y, and Z coordinates are mapped to that drift.

### Hybrid Model

The curve applies to all three axes, and the multipliers scale each axis independently:

```
effectiveX = curve(chaos) * lerpX_multiplier
effectiveY = curve(chaos) * lerpY_multiplier
effectiveZ = curve(chaos) * lerpZ_multiplier
```

### Curve Mapping

The curve maps chaos (0–1) to timing/pitch/velocity drift (0–1). The default curve is "natural" (logarithmic, gentle slope).

### Views

- **Curve View**: 5 control points, drag to shape the curve. Grid, axis labels, and control point markers.
- **Preview View**: Live drift visualization (timing/pitch/velocity deviations).

## BPM Sync

- **Free**: The sequencer runs at its own tempo (Tempo parameter).
- **Host Sync**: The sequencer adapts its timing to the host's BPM.

## Assets

Resources are located in `plugins/MicroTimingChaos/Resources/`:

- `icons/`: Icon files (SVG)
- `textures/`: Texture files (PNG)

## Building

```bash
cmake -S . -B build -A x64 -DSUITE_BUILD_PLUGINS=ON
cmake --build build --config Release --parallel 8
```

## Testing

```bash
ctest --test-dir build -C Release --output-on-failure
```

## License

This project is licensed under the GNU GPL v3.
