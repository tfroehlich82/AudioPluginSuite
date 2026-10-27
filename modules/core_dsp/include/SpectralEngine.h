#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>
#include <complex>

/**
    FFT overlap-add engine with LFO-driven spectral phase drift.

    Signal path per channel:
      input -> Hann analysis window -> FFT (2048) -> [freeze? hold magnitudes : analyse]
            -> per-bin phase drift -> Hermitian IFFT -> Hann synthesis window -> overlap-add
            -> dry/wet mix

    The engine is a streaming processor: samples go in one at a time and come out
    getLatencySamples() later. Magnitudes are unchanged by the drift, so the audible
    effect comes from phase relationships between overlapping frames.
*/
namespace CoreDSP::Spectral
{
    constexpr int frameOrder = 11;
    constexpr int frameSize = 1 << frameOrder;     // 2048 samples
    constexpr int hopSize = frameSize / 4;         // 75 % overlap
    constexpr int numBins = frameSize / 2 + 1;     // DC .. Nyquist

    /** Default peak phase swing of the drift LFO, in radians. */
    constexpr float defaultDriftDepth = 0.6f;

    /** Phase change between adjacent bins of the drift pattern, in radians. */
    constexpr float binSpread = 0.05f;

    namespace detail
    {
        constexpr float twoPi = 6.28318530717958647692f;

        inline float wrapPhase (float phase) noexcept
        {
            phase = std::fmod (phase, twoPi);
            return phase < 0.0f ? phase + twoPi : phase;
        }
    }

    class DriftEngine
    {
    public:
        struct Settings
        {
            float driftRateHz = 1.0f;
            float driftDepth = defaultDriftDepth;   // radians
            float feedback = 0.0f;                  // 0 .. 0.95
            float mix = 1.0f;                       // 0 (dry) .. 1 (wet)
            bool frozen = false;                    // hold the captured spectrum
        };

        /** lfoPhaseOffset decorrelates channels: use 0 for left and pi/2 for right. */
        explicit DriftEngine (float lfoPhaseOffset = 0.0f);

        /** Sets the sample rate and clears all state. */
        void prepare (double newSampleRate) noexcept;

        /** Updates the processing settings. Safe to call once per block. */
        void setSettings (const Settings& newSettings) noexcept { settings = newSettings; }

        /** Processes one sample and returns the sample that entered getLatencySamples() ago. */
        float processSample (float input) noexcept;

        /** Linear magnitudes of the most recent input frame (not updated while frozen). */
        const std::array<float, numBins>& getInputMagnitudes() const noexcept { return inputMagnitudes; }

        /** Linear magnitudes of the most recent output frame, measured on the overlap-added signal. */
        const std::array<float, numBins>& getOutputMagnitudes() const noexcept { return outputMagnitudes; }

        static constexpr int getLatencySamples() noexcept { return frameSize; }

    private:
        void processFrame() noexcept;

        float lfoPhaseOffset;
        float lfoPhase = 0.0f;
        double sampleRate = 44100.0;
        Settings settings;

        juce::dsp::FFT fft { frameOrder };
        juce::dsp::WindowingFunction<float> window { frameSize, juce::dsp::WindowingFunction<float>::hann, false };

        // Inverse of the sum of squared windows for each hop phase, so overlap-add is unity gain.
        std::array<float, hopSize> invOverlapGain {};

        // Circular buffers indexed by absolute sample number modulo frameSize.
        // Slot ringPos always holds the sample that is frameSize samples old.
        std::array<float, frameSize> inputRing {};
        std::array<float, frameSize> dryRing {};
        std::array<float, frameSize> wetRing {};
        std::array<float, frameSize> outputAccumulator {};

        std::array<float, frameSize> frameBuffer {};
        std::array<std::complex<float>, frameSize> fftInput {};
        std::array<std::complex<float>, frameSize> fftOutput {};
        std::array<std::complex<float>, frameSize> spectrum {};

        std::array<float, numBins> magnitudes {};
        std::array<float, numBins> phases {};
        std::array<float, numBins> inputMagnitudes {};
        std::array<float, numBins> outputMagnitudes {};

        int ringPos = 0;
        int hopIndex = 0;
    };

    inline DriftEngine::DriftEngine (float offset)
        : lfoPhaseOffset (offset)
    {
        // Each output sample is covered by frameSize / hopSize windows, and those windows sum to a
        // constant only for a periodic window. Measure the sum directly so the gain is exact.
        std::array<float, frameSize> probe;
        probe.fill (1.0f);
        window.multiplyWithWindowingTable (probe.data(), frameSize);

        for (int r = 0; r < hopSize; ++r)
        {
            float sum = 0.0f;

            for (int j = r; j < frameSize; j += hopSize)
                sum += probe[(size_t) j] * probe[(size_t) j];

            invOverlapGain[(size_t) r] = 1.0f / sum;
        }
    }

    inline void DriftEngine::prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        lfoPhase = 0.0f;
        ringPos = 0;
        hopIndex = 0;

        inputRing.fill (0.0f);
        dryRing.fill (0.0f);
        wetRing.fill (0.0f);
        outputAccumulator.fill (0.0f);
        magnitudes.fill (0.0f);
        phases.fill (0.0f);
        inputMagnitudes.fill (0.0f);
        outputMagnitudes.fill (0.0f);
    }

    inline float DriftEngine::processSample (float input) noexcept
    {
        // The slot at ringPos holds index n - frameSize. Every frame that covers that index has already
        // been added, so the sample is final and can be emitted now.
        const float wet = outputAccumulator[(size_t) ringPos] * invOverlapGain[(size_t) hopIndex];
        outputAccumulator[(size_t) ringPos] = 0.0f;
        wetRing[(size_t) ringPos] = wet;

        const float dry = dryRing[(size_t) ringPos];
        dryRing[(size_t) ringPos] = input;

        // Feedback is taken from the bounded wet signal so the loop cannot run away.
        inputRing[(size_t) ringPos] = input + settings.feedback * std::tanh (wet);

        ringPos = (ringPos + 1) % frameSize;
        hopIndex = (hopIndex + 1) % hopSize;

        if (hopIndex == 0)
            processFrame();

        return dry + settings.mix * (wet - dry);
    }

    inline void DriftEngine::processFrame() noexcept
    {
        // The oldest sample in the input ring starts the frame.
        const int start = ringPos;

        if (! settings.frozen)
        {
            for (int i = 0; i < frameSize; ++i)
                frameBuffer[(size_t) i] = inputRing[(size_t) ((start + i) % frameSize)];

            window.multiplyWithWindowingTable (frameBuffer.data(), frameSize);

            for (int i = 0; i < frameSize; ++i)
                fftInput[(size_t) i] = { frameBuffer[(size_t) i], 0.0f };

            fft.perform (fftInput.data(), fftOutput.data(), false);

            for (int k = 0; k < numBins; ++k)
            {
                magnitudes[(size_t) k] = std::abs (fftOutput[(size_t) k]);
                phases[(size_t) k] = std::arg (fftOutput[(size_t) k]);
                inputMagnitudes[(size_t) k] = magnitudes[(size_t) k];
            }
        }
        else
        {
            // Frozen: keep the captured magnitudes and advance each bin at its own frequency.
            constexpr float frozenPhaseStep = detail::twoPi * static_cast<float> (hopSize) / static_cast<float> (frameSize);

            for (int k = 0; k < numBins; ++k)
                phases[(size_t) k] = detail::wrapPhase (phases[(size_t) k] + frozenPhaseStep * static_cast<float> (k));
        }

        lfoPhase = detail::wrapPhase (lfoPhase + detail::twoPi * settings.driftRateHz
                                                 * static_cast<float> (hopSize) / static_cast<float> (sampleRate));
        const float lfo = lfoPhase + lfoPhaseOffset;

        for (int k = 0; k < numBins; ++k)
        {
            const float drift = settings.driftDepth * std::sin (lfo + binSpread * static_cast<float> (k));
            spectrum[(size_t) k] = std::polar (magnitudes[(size_t) k], phases[(size_t) k] + drift);
        }

        // A real signal needs real DC and Nyquist bins, and conjugate symmetry everywhere else.
        spectrum[0].imag (0.0f);
        spectrum[frameSize / 2].imag (0.0f);

        for (int k = 1; k < frameSize / 2; ++k)
            spectrum[(size_t) (frameSize - k)] = std::conj (spectrum[(size_t) k]);

        fft.perform (spectrum.data(), fftInput.data(), true);

        for (int i = 0; i < frameSize; ++i)
            frameBuffer[(size_t) i] = fftInput[(size_t) i].real();

        window.multiplyWithWindowingTable (frameBuffer.data(), frameSize);

        for (int i = 0; i < frameSize; ++i)
            outputAccumulator[(size_t) ((start + i) % frameSize)] += frameBuffer[(size_t) i];

        // Measure the output spectrum on the samples that are already final (one frame old).
        for (int i = 0; i < frameSize; ++i)
            frameBuffer[(size_t) i] = wetRing[(size_t) ((start + i) % frameSize)];

        window.multiplyWithWindowingTable (frameBuffer.data(), frameSize);

        for (int i = 0; i < frameSize; ++i)
            fftInput[(size_t) i] = { frameBuffer[(size_t) i], 0.0f };

        fft.perform (fftInput.data(), fftOutput.data(), false);

        for (int k = 0; k < numBins; ++k)
            outputMagnitudes[(size_t) k] = std::abs (fftOutput[(size_t) k]);
    }
}
