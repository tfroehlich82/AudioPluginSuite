#pragma once

#include <algorithm>
#include <cmath>

/**
 * Timing helpers for LFOs: the division table used when an LFO is synced to the beat, and the
 * conversions from beats and seconds to cycles. Header-only, no JUCE dependency.
 */
namespace CoreDSP::Modulation
{
    /** Cycle length in beats for each division choice (index order is saved in sessions): 4 bars, 2 bars, 1 bar, 1/2, 1/4, 1/8, 1/16. */
    inline constexpr double divisionBeats[7] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25 };

    /** The cycle length in beats for a division index. The index is clamped to 0 to 6. */
    inline double divisionToBeats (int index) noexcept
    {
        return divisionBeats[std::clamp (index, 0, 6)];
    }

    /** The fractional part of x, in [0, 1). Negative input wraps as well: fractionalPart(-0.25) is 0.75. */
    inline double fractionalPart (double x) noexcept
    {
        return x - std::floor (x);
    }

    /** The number of cycles an LFO advances in a time: seconds times rate in Hz. */
    inline double cyclesFromSeconds (double seconds, double rateHz) noexcept
    {
        return seconds * rateHz;
    }
}  // namespace CoreDSP::Modulation
