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
     * Output in [-1, 1] for a number of elapsed cycles.
     *
     * Sine: sin(2 pi p). Triangle: 1 - 4|p - 0.5|, starting at -1. Saw: 2p - 1, rising.
     * Square: +1 for p < 0.5, otherwise -1. Sample and hold: a random value per cycle, from hashUnit.
     * Random: a random value from a hash, but varies more slowly.
     * Noise: white noise based on cycles and seed.
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
            case LfoShape::random:
            case LfoShape::noise:
            {
                // For sampleAndHold and random, we use the cycleFloor to generate a pseudo-random value
                const auto index = static_cast<std::uint32_t> (static_cast<std::int64_t> (cycleFloor));
                // Combine seed and index for variation
                const std::uint32_t combinedSeed = seed ^ index;
                const float value = 2.0f * CoreDSP::Sequencer::hashUnit (combinedSeed, index) - 1.0f;
                
                if (shape == LfoShape::random)
                {
                    // For random, add some additional variation by using a different hash
                    return 2.0f * CoreDSP::Sequencer::hashUnit (combinedSeed, combinedSeed) - 1.0f;
                }
                else if (shape == LfoShape::noise)
                {
                    // Simple noise: use the fractional part p to vary noise level
                    return (float) (std::sin (p * 100.0) * value);
                }
                
                return value;
            }
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