#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

/**
    Pure sequencing rules for the Micro-Timing Chaos plugin.

    Everything here is deterministic and has no state, so the timing, pitch and firing rules can be
    tested in isolation. The plugin supplies the chaotic inputs (the Lorenz X, Y and Z values).
*/
namespace CoreDSP::Sequencer
{
    constexpr int maxSteps = 16;
    constexpr int minSteps = 2;
    constexpr int numRates = 4;
    constexpr int numScales = 6;

    /** Step length in beats for a rate choice: 0 = 1/4, 1 = 1/8, 2 = 1/16, 3 = 1/32. */
    inline double stepBeats (int rateIndex) noexcept
    {
        return 1.0 / (double) (1 << std::clamp (rateIndex, 0, numRates - 1));
    }

    /** Deterministic hash of (seed, index), mapped to [0, 1). The same seed always gives the same pattern. */
    inline float hashUnit (std::uint32_t seed, std::uint32_t index) noexcept
    {
        std::uint32_t x = seed * 0x9E3779B9u + index * 0x85EBCA6Bu + 0x27D4EB2Fu;
        x ^= x >> 16;
        x *= 0x7FEB352Du;
        x ^= x >> 15;
        x *= 0x846CA68Bu;
        x ^= x >> 16;
        return (float) (x >> 8) / 16777216.0f;   // 24 random bits, so the result is always below 1
    }

    /** True if the step at patternIndex fires. density is 0 to 1. */
    inline bool stepFires (std::uint32_t seed, int patternIndex, float density) noexcept
    {
        return hashUnit (seed, (std::uint32_t) patternIndex) < std::clamp (density, 0.0f, 1.0f);
    }

    struct ScaleDefinition
    {
        int size;
        std::array<int, 12> intervals;
    };

    /** Scales in the order of the plugin's scale choice. */
    inline const ScaleDefinition& scaleDefinition (int scaleIndex) noexcept
    {
        static constexpr ScaleDefinition scales[numScales] = {
            { 12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },   // chromatic
            { 7,  { 0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0, 0 } },    // major
            { 7,  { 0, 2, 3, 5, 7, 8, 10, 0, 0, 0, 0, 0 } },    // natural minor
            { 5,  { 0, 3, 5, 7, 10, 0, 0, 0, 0, 0, 0, 0 } },    // minor pentatonic
            { 7,  { 0, 2, 3, 5, 7, 9, 10, 0, 0, 0, 0, 0 } },    // dorian
            { 6,  { 0, 2, 4, 6, 8, 10, 0, 0, 0, 0, 0, 0 } }     // whole tone
        };

        return scales[std::clamp (scaleIndex, 0, numScales - 1)];
    }

    /** Number of distinct pitch degrees across the octave range. */
    inline int degreeCount (int scaleIndex, int octaves) noexcept
    {
        return scaleDefinition (scaleIndex).size * std::clamp (octaves, 1, 3);
    }

    /** Maps a value in [-1, 1] onto a degree index in [0, degreeCount). */
    inline int degreeFromUnit (float unit, int scaleIndex, int octaves) noexcept
    {
        const int count = degreeCount (scaleIndex, octaves);
        const float position = (std::clamp (unit, -1.0f, 1.0f) + 1.0f) * 0.5f * (float) count;
        return std::clamp ((int) std::floor (position), 0, count - 1);
    }

    /** MIDI note for a degree index counted up from the root, in the given scale. */
    inline int notePitch (int root, int scaleIndex, int degree) noexcept
    {
        const auto& scale = scaleDefinition (scaleIndex);
        const int octave = degree / scale.size;
        const int note = root + scale.intervals[(size_t) (degree % scale.size)] + 12 * octave;
        return std::clamp (note, 0, 127);
    }

    /**
        Blends between the fixed melody (hashDegree) and the chaotic melody (lorenzDegree).
        chaos 0 repeats the melody exactly, chaos 1 follows the attractor completely.
    */
    inline int blendDegree (int hashDegree, int lorenzDegree, float chaos, int count) noexcept
    {
        const float t = std::clamp (chaos, 0.0f, 1.0f);
        const float blended = (1.0f - t) * (float) hashDegree + t * (float) lorenzDegree;
        return std::clamp ((int) std::lround (blended), 0, count - 1);
    }

    /** Offset in milliseconds of a step: chaos scales the Lorenz X value over the timing range. */
    inline float timingOffsetMs (float chaos, float rangeMs, float x) noexcept
    {
        return std::clamp (chaos, 0.0f, 1.0f) * std::clamp (x, -1.0f, 1.0f) * rangeMs;
    }

    /** Swing shifts every second step by swing (-1 to +1) of a step: positive is late, negative is early. */
    inline double swingBeats (int step, float swing, double stepLengthBeats) noexcept
    {
        return (step % 2 == 1) ? std::clamp (swing, -1.0f, 1.0f) * stepLengthBeats : 0.0;
    }

    /** Gate length in beats. The Lorenz Y value lengthens or shortens the gate by up to half. */
    inline double gateBeats (double stepLengthBeats, float gate, float chaos, float y) noexcept
    {
        const float fraction = std::clamp (gate, 0.05f, 1.0f) * (1.0f + 0.5f * std::clamp (chaos, 0.0f, 1.0f) * std::clamp (y, -1.0f, 1.0f));
        return std::clamp (fraction, 0.05f, 1.0f) * stepLengthBeats;
    }

    /** Velocity with Velocity Chaos: the Lorenz Z value moves it by up to half of the chaos amount. */
    inline int velocityFor (int base, float velocityChaos, float z) noexcept
    {
        const float scaled = (float) base * (1.0f + 0.5f * std::clamp (velocityChaos, 0.0f, 1.0f) * std::clamp (z, -1.0f, 1.0f));
        return std::clamp ((int) std::lround (scaled), 1, 127);
    }
}
