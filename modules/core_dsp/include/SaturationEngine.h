#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>

/**
    Memoryless waveshapers, a wavefolder, a one-pole filter and an envelope follower.

    Every shaper here maps 0 to 0 and stays within [-1, 1] for any finite drive,
    so the stage never adds DC offset or exceeds full scale.
*/
namespace CoreDSP::Saturation
{
    enum class Mode
    {
        tube = 0,
        tape,
        diode
    };

    constexpr float defaultTubeBias = 0.2f;
    constexpr float maxTubeBias = 0.5f;

    /** Asymmetric tube-style curve: tanh(x + bias), re-centred on zero and normalised to [-1, 1]. */
    inline float tube (float x, float bias = defaultTubeBias) noexcept
    {
        const float b = std::clamp (bias, 0.0f, maxTubeBias);
        const float t = std::tanh (b);
        return (std::tanh (x + b) - t) / (1.0f + t);
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

    /** Applies the transfer function selected by mode. bias only affects Tube. */
    inline float shape (float x, Mode mode, float tubeBias = defaultTubeBias) noexcept
    {
        switch (mode)
        {
            case Mode::tube:  return tube (x, tubeBias);
            case Mode::tape:  return tape (x);
            case Mode::diode: return diode (x);
        }

        return x;
    }

    /**
        Wavefolder. amount 0 is a bypass. Higher amounts fold the waveform back on itself through a
        sine, which adds upper harmonics. The result is a blend of two bounded terms, so it stays in [-1, 1].
    */
    inline float fold (float x, float amount) noexcept
    {
        const float a = std::clamp (amount, 0.0f, 1.0f);
        const float y = std::clamp (x, -1.0f, 1.0f);
        return (1.0f - a) * y + a * std::sin (juce::MathConstants<float>::halfPi * (1.0f + 3.0f * a) * y);
    }

    /**
        Effective pre-gain when the envelope modulates the drive:
        Dynamic Drive = Drive * (1 + Sensitivity * Envelope).
    */
    inline float dynamicDrive (float baseGain, float sensitivity, float envelope) noexcept
    {
        return baseGain * (1.0f + sensitivity * envelope);
    }

    /** Reshapes an envelope level: curve 1 is linear, below 1 reacts earlier, above 1 only reacts to peaks. */
    inline float shapeEnvelope (float envelope, float curve) noexcept
    {
        return std::pow (std::max (envelope, 0.0f), curve);
    }

    /** One-pole low-pass. The high-pass output is the input minus the low-pass output. */
    class OnePoleFilter
    {
    public:
        void prepare (double sampleRate, float cutoffHz) noexcept
        {
            setCutoff (sampleRate, cutoffHz);
            reset();
        }

        void setCutoff (double sampleRate, float cutoffHz) noexcept
        {
            if (sampleRate <= 0.0)
                return;

            const float hz = std::clamp (cutoffHz, 1.0f, 0.45f * (float) sampleRate);
            coefficient = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * hz / (float) sampleRate);
        }

        void reset() noexcept { state = 0.0f; }

        float lowpass (float x) noexcept
        {
            state += coefficient * (x - state);
            return state;
        }

        float highpass (float x) noexcept { return x - lowpass (x); }

    private:
        float coefficient = 1.0f;
        float state = 0.0f;
    };

    /**
        Tilt tone control around a 1 kHz pivot. tone = 0 is transparent (low + high = input exactly).
        Negative values darken and positive values brighten. The output can exceed the input level
        slightly, so this sits after the shaper, not inside it.
    */
    class ToneControl
    {
    public:
        void prepare (double sampleRate) noexcept
        {
            filter.prepare (sampleRate, 1000.0f);
        }

        void reset() noexcept { filter.reset(); }

        float process (float x, float tone) noexcept
        {
            const float low = filter.lowpass (x);
            const float high = x - low;
            const float t = std::clamp (tone, -1.0f, 1.0f);
            return low * (1.0f - 0.5f * t) + high * (1.0f + 0.5f * t);
        }

    private:
        OnePoleFilter filter;
    };

    /** One-pole peak follower with independent attack and release times. */
    class EnvelopeFollower
    {
    public:
        void prepare (double sampleRate, float attackMs = 5.0f, float releaseMs = 150.0f) noexcept
        {
            sampleRateHz = sampleRate;
            setTimes (attackMs, releaseMs);
            reset();
        }

        void setTimes (float attackMs, float releaseMs) noexcept
        {
            attackCoeff = coefficient (attackMs, sampleRateHz);
            releaseCoeff = coefficient (releaseMs, sampleRateHz);
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

        double sampleRateHz = 44100.0;
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        float level = 0.0f;
    };
}
