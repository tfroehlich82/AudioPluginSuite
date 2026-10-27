// Unit tests for CoreDSP.
//
// NDEBUG is undefined before <cassert> on purpose. The CI runs Release builds, where
// the default setting would compile every assert() away and let the tests pass vacuously.

#undef NDEBUG
#include <cassert>

#include <cmath>
#include <cstdio>
#include <vector>

#include "CoreDSP.h"

namespace
{
    constexpr double pi = 3.14159265358979323846;

    void testSaturationCurves()
    {
        const CoreDSP::Saturation::Mode modes[] = { CoreDSP::Saturation::Mode::tube,
                                                    CoreDSP::Saturation::Mode::tape,
                                                    CoreDSP::Saturation::Mode::diode };
        const float gains[] = { 1.0f, 10.0f, 100.0f, 10000.0f };

        for (auto mode : modes)
        {
            // Zero in, zero out: no DC offset from any shaper.
            assert (std::abs (CoreDSP::Saturation::shape (0.0f, mode)) < 1e-6f);

            for (float gain : gains)
            {
                for (int i = -1000; i <= 1000; ++i)
                {
                    const float x = gain * static_cast<float> (i) / 1000.0f;
                    const float y = CoreDSP::Saturation::shape (x, mode);

                    assert (std::isfinite (y));
                    assert (y >= -1.0f && y <= 1.0f);
                }
            }
        }

        std::puts ("[PASS] testSaturationCurves");
    }

    void testLorenzChaos()
    {
        CoreDSP::Chaos::LorenzAttractor lorenz;

        // Let the trajectory settle onto the attractor before measuring it.
        lorenz.advance (50.0);

        float previousX = lorenz.normalisedX();
        float maxStepDelta = 0.0f;

        for (int i = 0; i < 200000; ++i)
        {
            lorenz.advance (CoreDSP::Chaos::LorenzAttractor::maxStepSize);

            const auto& state = lorenz.getState();
            assert (std::isfinite (state.x) && std::isfinite (state.y) && std::isfinite (state.z));

            // The raw state must stay inside the known attractor box.
            assert (std::abs (state.x) < 25.0);
            assert (std::abs (state.y) < 35.0);
            assert (state.z > -1.0 && state.z < 55.0);

            const float nx = lorenz.normalisedX();
            const float ny = lorenz.normalisedY();
            const float nz = lorenz.normalisedZ();

            assert (nx >= -1.0f && nx <= 1.0f);
            assert (ny >= -1.0f && ny <= 1.0f);
            assert (nz >= -1.0f && nz <= 1.0f);

            // Continuity: one integration step must not jump across the normalised range.
            maxStepDelta = std::max (maxStepDelta, std::abs (nx - previousX));
            previousX = nx;
        }

        assert (maxStepDelta < 0.1f);

        // Determinism: resetting to the same state reproduces the same trajectory.
        CoreDSP::Chaos::LorenzAttractor a, b;
        a.advance (3.0);
        b.advance (3.0);
        assert (a.getState().x == b.getState().x);

        std::puts ("[PASS] testLorenzChaos");
    }

    void testSpectralFFT()
    {
        using namespace CoreDSP::Spectral;

        // Silence in must give a silent spectrum out.
        {
            DriftEngine engine;
            engine.prepare (48000.0);

            for (int n = 0; n < 4 * frameSize; ++n)
                engine.processSample (0.0f);

            for (float magnitude : engine.getOutputMagnitudes())
                assert (magnitude < 1e-6f);
        }

        // A pure tone on bin 64 must show its energy at bin 64.
        {
            DriftEngine engine;
            engine.prepare (48000.0);
            DriftEngine::Settings settings;
            settings.driftDepth = 0.0f;
            engine.setSettings (settings);

            const int bin = 64;
            for (int n = 0; n < 8 * frameSize; ++n)
                engine.processSample (static_cast<float> (std::sin (2.0 * pi * bin * n / frameSize)));

            const auto& spectrum = engine.getOutputMagnitudes();
            int peak = 0;

            for (int k = 1; k < numBins; ++k)
                if (spectrum[(size_t) k] > spectrum[(size_t) peak])
                    peak = k;

            assert (peak == bin);
            assert (spectrum[(size_t) bin] > 100.0f);
        }

        std::puts ("[PASS] testSpectralFFT");
    }

    void testSpectralReconstruction()
    {
        using namespace CoreDSP::Spectral;

        // With no drift, no feedback and full wet mix, the engine is a pure delay line:
        // output(n) must equal input(n - latency) to float precision.
        DriftEngine engine;
        engine.prepare (48000.0);

        DriftEngine::Settings settings;
        settings.driftDepth = 0.0f;
        settings.mix = 1.0f;
        engine.setSettings (settings);

        const int latency = DriftEngine::getLatencySamples();
        const int total = 6 * frameSize;
        std::vector<float> input ((size_t) total);

        for (int n = 0; n < total; ++n)
        {
            input[(size_t) n] = 0.5f * static_cast<float> (std::sin (2.0 * pi * 440.0 * n / 48000.0))
                              + 0.25f * static_cast<float> (std::sin (2.0 * pi * 3100.0 * n / 48000.0));
        }

        float maxError = 0.0f;

        for (int n = 0; n < total; ++n)
        {
            const float out = engine.processSample (input[(size_t) n]);

            if (n >= latency)
                maxError = std::max (maxError, std::abs (out - input[(size_t) (n - latency)]));
        }

        assert (maxError < 1e-3f);

        std::puts ("[PASS] testSpectralReconstruction");
    }

    void testEnvelopeFollower()
    {
        CoreDSP::Saturation::EnvelopeFollower follower;
        follower.prepare (48000.0, 5.0f, 150.0f);

        // A 5 ms attack: after 30 ms of constant input the level is within 1 % of the input.
        for (int n = 0; n < 1440; ++n)
            follower.process (1.0f);

        assert (follower.getLevel() > 0.99f);

        // A 150 ms release: after 150 ms of silence the level has fallen to about 1/e.
        for (int n = 0; n < 7200; ++n)
            follower.process (0.0f);

        assert (follower.getLevel() > 0.3f && follower.getLevel() < 0.4f);

        std::puts ("[PASS] testEnvelopeFollower");
    }

    void testDynamicDrive()
    {
        assert (std::abs (CoreDSP::Saturation::dynamicDrive (1.0f, 0.5f, 1.0f) - 1.5f) < 1e-6f);
        assert (std::abs (CoreDSP::Saturation::dynamicDrive (2.0f, 0.0f, 1.0f) - 2.0f) < 1e-6f);

        std::puts ("[PASS] testDynamicDrive");
    }
}

int main()
{
    std::puts ("Running CoreDSP unit tests...");

    testSaturationCurves();
    testLorenzChaos();
    testSpectralFFT();
    testSpectralReconstruction();
    testEnvelopeFollower();
    testDynamicDrive();

    std::puts ("All CoreDSP unit tests passed.");
    return 0;
}
