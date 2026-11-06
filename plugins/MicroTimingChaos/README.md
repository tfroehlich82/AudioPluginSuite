# Micro-Timing Chaos

A MIDI step sequencer plugin by AudioSuite.

## Overview

The plugin uses a Lorenz attractor to introduce micro-timing, pitch, and velocity variation to a fixed pattern. The attractor's X coordinate moves each step's timing, Y chooses its pitch and gate length, and Z sets its velocity. How strongly each of those happens is set by the **Lerp Curve**.

### Controls

#### Pattern

- **Rate**: 1/4, 1/8, 1/16, 1/32
- **Pattern Length**: 2–16 steps
- **Density**: 0–100% (fires the pattern)
- **Seed**: 0–999 (random pattern)
- **Gate**: 10–100% (note length)
- **Swing**: −20 to +20% (groove; positive delays odd steps, negative moves them early)
- **Base Velocity**: 30–127

#### Melody

- **Root Note**: 24–72
- **Scale**: Chromatic, Major, Minor, Minor Pentatonic, Dorian, Whole Tone
- **Octaves**: 1–3

#### Chaos

- **Chaos**: 0–100%. This is the input to the lerp curve. At 0% the pattern never moves.
- **Timing Range**: 0–50 ms, the largest timing shift
- **Velocity Chaos**: 0–100% (how far velocity moves)

#### Lerp Curve

- **Lerp**: 0–100%, default 50%. How far each step drifts from the fixed pattern toward the attractor. At 0% nothing drifts.
- **Lerp X Mult**, **Lerp Y Mult**, **Lerp Z Mult**: 0–2×, default 1×. Scale the drift of timing, pitch and gate, and velocity.

**Hybrid model.** The curve is evaluated at Chaos, then each axis gets its own multiplier, and Lerp scales the result:

```
curve      = lerpCurve(chaos)                       (0 to 1)
timingDrift = lerp * clamp(curve * lerpXMult, 0, 1)
pitchDrift  = lerp * clamp(curve * lerpYMult, 0, 1)
velocityDrift = lerp * clamp(curve * lerpZMult, 0, 1)
```

Each drift is then applied to its axis: the timing offset, the blend between the fixed and the attractor melody, and the velocity and gate spread.

**The curve.** Five control points map Chaos (x, 0 to 1) to drift (y, 0 to 1). The first point is fixed at x = 0 and the last at x = 1, and the three in between can move along x, between their neighbours. The curve is saved with the session.

**Default curve: "natural".** The points follow `y = log10(1 + 9x)`, a logarithmic curve with a gentle slope: small chaos gives a clear drift, and high chaos flattens out.

**Views** (tabs in the Lerp Curve group):

- **Curve**: drag a point to move it. Right-click a point to reset it to the natural curve. Double-click the graph to reset the whole curve. The live marker shows the drift for the current Chaos value.
- **Preview**: the live drift per axis (timing, pitch, velocity), and the timing, pitch and velocity of the last 32 steps.

#### Transport

- **Tempo**: 40–240 BPM, used when BPM Sync is off
- **Speed**: 0.5–2.0. How fast the attractor moves, relative to the tempo
- **BPM Sync**: when on, the sequencer follows the host's tempo and transport. The Tempo knob is disabled, and the host tempo is shown under the button.

### UI Design

- Dark, flat panels with a neon lime accent. Each drift axis has its own colour: cyan for timing, amber for pitch and gate, lime for velocity.
- Grouped sections: Pattern, Melody, Chaos, Lerp Curve, Transport.
- The BPM Sync button shows an icon, with a texture overlay and an ON or OFF state.
- Hover any control to see what it does in the help line at the bottom.

## LFO and Modulation

Two LFOs, each with a rate, a shape and a sync switch, and six routing slots. A slot sends one LFO to one control, with a signed amount.

| Control | Range | Default | Description |
|---|---|---|---|
| **LFO Rate** | 0.02 – 20 Hz (logarithmic) | 0.25 Hz | How fast the LFO cycles. Disabled while its Sync is on. |
| **LFO Shape** | Sine, Triangle, Saw, Square, Sample & Hold, Random, Noise | Sine | The waveform. Sample & Hold picks a new random level on each cycle. |
| **LFO Sync** | on / off | off | Locks the LFO to musical time. |
| **LFO Division** | 4 bars, 2 bars, 1 bar, 1/2, 1/4, 1/8, 1/16 | 1/4 | The cycle length while synced. |
| **LFO Offset** | −100 % to +100 % | 0 % | Moves the LFO up or down before it is routed. The output stops at −1 and +1. |
| **Mod Source** | LFO 1, LFO 2 | LFO 1 | Which LFO drives the slot. |
| **Mod Target** | see the list below | None | The control the slot moves. None turns the slot off. |
| **Mod Amount** | −100 to +100 % | 0 % | How far the target moves. Negative values invert the LFO. |

**Targets:** Chaos, Timing Range, Velocity Chaos, Lerp, Lerp X Mult, Lerp Y Mult, Lerp Z Mult, Density, Gate, Swing, Velocity, Speed, Tempo and Seed. Tempo only moves while Host Sync is off. Rate, Length, Scale, Root Note, Octaves and Host Sync are not modulation targets.

**How it works.** An amount of 100 % sweeps the whole range of the target, so a log or bipolar control behaves the same way. Chaos is modulated before the lerp curve, so a modulated Chaos moves along the curve. With no slot aimed at a control, nothing changes.

**Example.** LFO 1 synced to 1 bar, Sine, routed to Timing Range at +50 % gives a timing sweep once per bar.

The panel shows each LFO's waveform, with a dot at the position it is at now.

## Latency

The plugin reports a fixed latency of 50 ms, which is the largest timing range. It does not change when Timing Range changes, so the host's delay compensation stays steady during a session.

## BPM Sync

- **Free**: the sequencer runs at its own tempo (Tempo parameter).
- **Host Sync**: the sequencer follows the host's tempo and playhead. When the transport stops, the sequencer stops and its notes are released.

## Building

```bash
cmake -S . -B build -A x64 -DSUITE_BUILD_PLUGINS=ON
cmake --build build --config Release --parallel 8
```

The icons and textures are embedded in the plugin with `juce_add_binary_data`, so the plugin does not need any files next to it.

## Testing

```bash
ctest --test-dir build -C Release --output-on-failure
```

## License

This project is licensed under the GNU GPL v3.
