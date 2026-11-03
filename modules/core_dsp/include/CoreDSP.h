#pragma once

/**
    Umbrella header for the CoreDSP module.

    CoreDSP is header-only. It depends on juce_dsp for the FFT and on nothing else,
    so the unit tests can build it without the plugin infrastructure.
*/

#include <cmath>

#include "ChaosEngine.h"
#include "ModulationEngine.h"
#include "SaturationEngine.h"
#include "SequencerEngine.h"
#include "SpectralEngine.h"

namespace CoreDSP
{
    /** Converts a level in decibels to a linear amplitude factor. */
    inline float decibelsToGain (float decibels) noexcept
    {
        return std::pow (10.0f, decibels / 20.0f);
    }
}
