#pragma once

#include <algorithm>
#include <cmath>

/**
    Memoryless waveshapers and a peak envelope follower.

    Every transfer function here is bounded to [-1, 1] and maps 0 to 0, so the
    shapers never add DC offset and never exceed full scale, however hard they are driven.
*/
namespace CoreDSP::Saturation
{
    enum class Mode
    {
        tube = 0,
        tape,
        diode
    };

    /** Asymmetric tube-style curve: tanh(x + bias), re-centred on zero and normalised to [-1, 1]. */
    inline float tube (float x) noexcept
    {
        constexpr float bias = 0.2f;
        const float t = std::tanh (bias);
        return (std::tanh (x + bias) - t) / (1.0f + t);
    }

    /** Symmetric soft clip: x - x^3 / 3 on the clamped input, scaled so the rails sit at +/-1. */
    inline float tape (float x) noexcept
    {
        const float y = std::clamp (x, -1.0f, 1.0f);
        return 1.5f * (y - (y * y * y) / 3.0f);
    }

    /** Diode-style exponential curve: a soft knee for positive excursions, a tighter one for negative. */
    inline float diode (float x) noexcept
    {
        return x >= 0.0f ? 1.0f - std::exp (-x) : std::exp (1.4f * x) - 1.0f;
    }

    /** Applies the transfer function selected by mode. */
    inline float shape (float x, Mode mode) noexcept
    {
        switch (mode)
        {
            case Mode::tube:  return tube (x);
            case Mode::tape:  return tape (x);
            case Mode::diode: return diode (x);
        }

        return x;
    }

    /**
        Effective pre-gain when the envelope modulates the drive:
        Dynamic Drive = Drive * (1 + Sensitivity * Envelope).
    */
    inline float dynamicDrive (float baseGain, float sensitivity, float envelope) noexcept
    {
        return baseGain * (1.0f + sensitivity * envelope);
    }

    /** One-pole peak follower with independent attack and release times. */
    class EnvelopeFollower
    {
    public:
        void prepare (double sampleRate, float attackMs = 5.0f, float releaseMs = 150.0f) noexcept
        {
            attackCoeff = coefficient (attackMs, sampleRate);
            releaseCoeff = coefficient (releaseMs, sampleRate);
            reset();
        }

        void reset() noexcept { level = 0.0f; }

        /** Feeds one sample and returns the smoothed level. Only the magnitude of the input matters. */
        float process (float sample) noexcept
        {
            const float rectified = std::abs (sample);
            const float coeff = rectified > level ? attackCoeff : releaseCoeff;
            level = rectified + coeff * (level - rectified);
            return level;
        }

        float getLevel() const noexcept { return level; }

    private:
        static float coefficient (float timeMs, double sampleRate) noexcept
        {
            if (timeMs <= 0.0f || sampleRate <= 0.0)
                return 0.0f;

            return std::exp (-1.0f / (0.001f * timeMs * static_cast<float> (sampleRate)));
        }

        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        float level = 0.0f;
    };
}
