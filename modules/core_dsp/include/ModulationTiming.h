namespace CoreDSP::Modulation
{
    /** Division table matching the indices: 4 bars, 2 bars, 1 bar, 1/2, 1/4, 1/8, 1/16 beats. */
    inline constexpr double divisionBeats[7] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25 };

    /** Returns the beat value corresponding to the division index, clamped to the valid range. */
    inline double divisionToBeats (int index) noexcept
    {
        if (index < 0 || index >= 7)
            return 0.0;
        return divisionBeats[index];
    }

    /** Returns the fractional part of a number, useful for cycle calculations. */
    inline double fractionalPart (double x) noexcept
    {
        return x - std::floor(x);
    }

    /** Converts seconds into the number of cycles a LFO should advance based on its rate. */
    inline double cyclesFromSeconds (double seconds, double rateHz) noexcept
    {
        return seconds * rateHz;
    }
}