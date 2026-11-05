#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "SequencerEngine.h"

/**
 * Low-frequency oscillators and modulation maths for the Micro-Timing Chaos plugin.
 *
 * The LFO input is the number of cycles elapsed, as an absolute value: 2.25 is a quarter of the
 * way through the third cycle. Keeping the count unwrapped lets sample and hold tell which cycle
 * it is in, and lets the processor change the rate without the phase jumping.
 */
namespace CoreDSP::Modulation
{
    enum class LfoShape
    {
        sine = 0,
        triangle = 1,
        saw = 2,
        square = 3,
        sampleAndHold = 4,
        random = 5,
        noise = 6,
        count
    };

    /**
     * Smooth value noise: a random value at each integer lattice point, joined with a smoothstep curve.
     * The lattice position x is in cycles (or any unit), so the curve is continuous everywhere. Output in [-1, 1].
     */
    inline float smoothNoise (std::uint32_t seed, double x) noexcept
    {
        const double cell = std::floor (x);
        const double t = x - cell;
        const double fade = t * t * (3.0 - 2.0 * t);
        const auto index = static_cast<std::uint32_t> (static_cast<std::int64_t> (cell));

        const float a = 2.0f * CoreDSP::Sequencer::hashUnit (seed, index) - 1.0f;
        const float b = 2.0f * CoreDSP::Sequencer::hashUnit (seed, index + 1u) - 1.0f;

        return a + (b - a) * (float) fade;
    }

    /**
     * Output in [-1, 1] for a number of elapsed cycles.
     *
     * Sine: sin(2 pi p). Triangle: 1 - 4|p - 0.5|, starting at -1. Saw: 2p - 1, rising.
     * Square: +1 for p < 0.5, otherwise -1.
     * Sample and hold: one random level per cycle, held until the next cycle.
     * Random: a smooth random curve, with two random points per cycle, so it sweeps up and down.
     * Noise: smooth fractal noise, three layers of random curves (8, 16 and 32 points per cycle, at
     * half the strength each), for an organic, irregular movement.
     * Here p is the fractional part of the cycle count.
     */
    inline float lfoValue (LfoShape shape, double cycles, std::uint32_t seed) noexcept
    {
        const double cycleFloor = std::floor (cycles);
        const double p = cycles - cycleFloor;

        switch (shape)
        {
            case LfoShape::sine:
                return (float) std::sin (2.0 * 3.14159265358979323846 * p);

            case LfoShape::triangle:
                return (float) (1.0 - 4.0 * std::abs (p - 0.5));

            case LfoShape::saw:
                return (float) (2.0 * p - 1.0);

            case LfoShape::square:
                return p < 0.5 ? 1.0f : -1.0f;

            case LfoShape::sampleAndHold:
            {
                const auto index = static_cast<std::uint32_t> (static_cast<std::int64_t> (cycleFloor));
                return 2.0f * CoreDSP::Sequencer::hashUnit (seed, index) - 1.0f;
            }

            case LfoShape::random:
                return smoothNoise (seed, cycles * 2.0);

            case LfoShape::noise:
            {
                float sum = 0.0f;
                float strength = 1.0f;
                float total = 0.0f;

                for (int layer = 0; layer < 3; ++layer)
                {
                    sum += strength * smoothNoise (seed + 0x9E37u * (std::uint32_t) layer, cycles * 8.0 * (double) (1 << layer));
                    total += strength;
                    strength *= 0.5f;
                }

                return sum / total;
            }

            case LfoShape::count:
                break;
        }

        return 0.0f;
    }

    /**
     * Adds a modulation offset in normalised space (0 to 1 across the parameter's range) and clamps the result.
     */
    inline float modulatedNormalised (float normalised, float offsetSum) noexcept
    {
        return std::clamp (normalised + offsetSum, 0.0f, 1.0f);
    }
}