#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "SequencerEngine.h"

/**
    Low-frequency oscillators and modulation maths for the Micro-Timing Chaos plugin.

    The LFO input is the number of cycles elapsed, as an absolute value: 2.25 is a quarter of the
    way through the third cycle. Keeping the count unwrapped lets sample and hold tell which cycle
    it is in, and lets the processor change the rate without the phase jumping.
*/
namespace CoreDSP::Modulation
{
    enum class LfoShape
    {
        sine = 0,
        triangle = 1,
        saw = 2,
        square = 3,
        sampleAndHold = 4
    };

    /**
        Output in [-1, 1] for a number of elapsed cycles.

        Sine: sin(2 pi p). Triangle: 1 - 4|p - 0.5|, starting at -1. Saw: 2p - 1, rising.
        Square: +1 for p < 0.5, otherwise -1. Sample and hold: a random value per cycle, from hashUnit.
        Here p is the fractional part of the cycle count.
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
                break;
        }

        const auto index = static_cast<std::uint32_t> (static_cast<std::int64_t> (cycleFloor));
        return 2.0f * CoreDSP::Sequencer::hashUnit (seed, index) - 1.0f;
    }

    /** Adds a modulation offset in normalised space (0 to 1 across the parameter's range) and clamps the result. */
    inline float modulatedNormalised (float normalised, float offsetSum) noexcept
    {
        return std::clamp (normalised + offsetSum, 0.0f, 1.0f);
    }
}
