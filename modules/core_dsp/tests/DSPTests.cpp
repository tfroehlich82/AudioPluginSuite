// Unit tests for CoreDSP.
//
// NDEBUG is undefined before <cassert> on purpose. The CI runs Release builds, where
// the default setting would compile every assert() away and let the tests pass vacuously.

#undef NDEBUG
#include <cassert>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "CoreDSP.h"

namespace
{
    constexpr double pi = 3.14159265358979323846;
    constexpr double sampleRate = 48000.0;

    using namespace CoreDSP::Spectral;
    using Mode = CoreDSP::Saturation::Mode;

    // Runs a drift engine over a generated signal and returns the output samples.
    template <typename Generator>
    std::vector<float> runEngine (DriftEngine& engine, int numSamples, Generator generate)
    {
        std::vector<float> out ((size_t) numSamples);

        for (int n = 0; n < numSamples; ++n)
            out[(size_t) n] = engine.processSample (generate (n));

        return out;
    }

    float sine (int n, double hz, float amplitude)
    {
        return amplitude * static_cast<float> (std::sin (2.0 * pi * hz * n / sampleRate));
    }

    int peakBin (const std::array<float, numBins>& spectrum)
    {
        int peak = 1;

        for (int k = 1; k < numBins; ++k)
            if (spectrum[(size_t) k] > spectrum[(size_t) peak])
                peak = k;

        return peak;
    }

    // Root mean square over the last count samples of a buffer.
    float rms (const std::vector<float>& signal, int count)
    {
        double sum = 0.0;

        for (size_t i = signal.size() - (size_t) count; i < signal.size(); ++i)
            sum += (double) signal[i] * signal[i];

        return static_cast<float> (std::sqrt (sum / count));
    }

    //==============================================================================
    // Saturation

    void testSaturationCurves()
    {
        const Mode modes[] = { Mode::tube, Mode::tape, Mode::diode };
        const float gains[] = { 1.0f, 10.0f, 100.0f, 10000.0f };
        const float biases[] = { 0.0f, 0.2f, 0.5f };

        for (auto mode : modes)
        {
            // Zero in, zero out: no DC offset from any shaper.
            assert (std::abs (CoreDSP::Saturation::shape (0.0f, mode)) < 1e-6f);

            for (float gain : gains)
            {
                for (int i = -1000; i <= 1000; ++i)
                {
                    const float x = gain * static_cast<float> (i) / 1000.0f;
                    const float y = CoreDSP::Saturation::shape (x, mode, 0.5f);

                    assert (std::isfinite (y));
                    assert (y >= -1.0f && y <= 1.0f);
                }
            }
        }

        std::puts ("[PASS] testSaturationCurves");
    }

    void testTubeBias()
    {
        for (float bias : { 0.0f, 0.1f, 0.25f, 0.5f, 2.0f })
        {
            assert (std::abs (CoreDSP::Saturation::tube (0.0f, bias)) < 1e-6f);

            for (int i = -1000; i <= 1000; ++i)
            {
                const float y = CoreDSP::Saturation::tube (static_cast<float> (i) / 100.0f, bias);
                assert (y >= -1.0f && y <= 1.0f);
            }
        }

        // Bias makes the curve asymmetric: positive and negative peaks differ.
        const float plus = CoreDSP::Saturation::tube (1.0f, 0.5f);
        const float minus = CoreDSP::Saturation::tube (-1.0f, 0.5f);
        assert (std::abs (plus + minus) > 0.05f);

        std::puts ("[PASS] testTubeBias");
    }

    void testFold()
    {
        for (float x = -1.0f; x <= 1.0f; x += 0.01f)
        {
            // Amount 0 is a bypass.
            assert (std::abs (CoreDSP::Saturation::fold (x, 0.0f) - x) < 1e-6f);

            for (float amount : { 0.25f, 0.5f, 1.0f })
            {
                const float y = CoreDSP::Saturation::fold (x, amount);
                assert (std::isfinite (y) && y >= -1.0f - 1e-6f && y <= 1.0f + 1e-6f);
            }
        }

        assert (std::abs (CoreDSP::Saturation::fold (0.0f, 1.0f)) < 1e-6f);

        // Folding adds content: a full-amount fold is clearly different from the input at 0.5.
        assert (std::abs (CoreDSP::Saturation::fold (0.5f, 1.0f) - 0.5f) > 0.1f);

        std::puts ("[PASS] testFold");
    }

    void testShapeEnvelope()
    {
        assert (std::abs (CoreDSP::Saturation::shapeEnvelope (0.3f, 1.0f) - 0.3f) < 1e-6f);
        assert (std::abs (CoreDSP::Saturation::shapeEnvelope (0.5f, 2.0f) - 0.25f) < 1e-6f);
        assert (std::abs (CoreDSP::Saturation::shapeEnvelope (0.25f, 0.5f) - 0.5f) < 1e-6f);
        assert (CoreDSP::Saturation::shapeEnvelope (-1.0f, 2.0f) == 0.0f);

        std::puts ("[PASS] testShapeEnvelope");
    }

    void testDynamicDrive()
    {
        assert (std::abs (CoreDSP::Saturation::dynamicDrive (1.0f, 0.5f, 1.0f) - 1.5f) < 1e-6f);
        assert (std::abs (CoreDSP::Saturation::dynamicDrive (2.0f, 0.0f, 1.0f) - 2.0f) < 1e-6f);
        assert (std::abs (CoreDSP::Saturation::dynamicDrive (2.0f, 1.0f, 0.0f) - 2.0f) < 1e-6f);

        std::puts ("[PASS] testDynamicDrive");
    }

    void testEnvelopeFollower()
    {
        CoreDSP::Saturation::EnvelopeFollower follower;
        follower.prepare (sampleRate, 5.0f, 150.0f);

        // A 5 ms attack: after 30 ms of constant input the level is within 1 % of the input.
        for (int n = 0; n < 1440; ++n)
            follower.process (1.0f);

        assert (follower.getLevel() > 0.99f);

        // A 150 ms release: after 150 ms of silence the level has fallen to about 1/e.
        for (int n = 0; n < 7200; ++n)
            follower.process (0.0f);

        assert (follower.getLevel() > 0.3f && follower.getLevel() < 0.4f);

        // Changing the times at runtime takes effect: a 1 ms attack reaches the input much faster.
        follower.reset();
        follower.setTimes (1.0f, 150.0f);

        for (int n = 0; n < 240; ++n)
            follower.process (1.0f);

        assert (follower.getLevel() > 0.99f);

        std::puts ("[PASS] testEnvelopeFollower");
    }

    void testOnePoleFilter()
    {
        CoreDSP::Saturation::OnePoleFilter filter;
        filter.prepare (sampleRate, 1000.0f);

        float low = 0.0f;
        float high = 1.0f;

        for (int n = 0; n < 48000; ++n)
        {
            low = filter.lowpass (1.0f);
            high = filter.highpass (1.0f);
        }

        // DC passes the low-pass unchanged and is removed by the high-pass.
        assert (std::abs (low - 1.0f) < 1e-3f);
        assert (std::abs (high) < 1e-3f);

        // The cutoff is clamped to a usable range, so a nonsense value still produces a stable filter.
        filter.setCutoff (sampleRate, 1.0e9f);
        float y = 0.0f;

        for (int n = 0; n < 1000; ++n)
            y = filter.lowpass (std::sin (0.1f * (float) n));

        assert (std::isfinite (y));

        // Non-positive sample rates are ignored rather than dividing by zero.
        filter.setCutoff (0.0, 1000.0f);
        assert (std::isfinite (filter.lowpass (0.5f)));

        std::puts ("[PASS] testOnePoleFilter");
    }

    void testToneControl()
    {
        CoreDSP::Saturation::ToneControl tone;
        tone.prepare (sampleRate);

        // Zero tone is transparent: low and high parts sum exactly back to the input.
        for (int n = 0; n < 4800; ++n)
        {
            const float x = sine (n, 440.0, 0.5f);
            assert (std::abs (tone.process (x, 0.0f) - x) < 1e-6f);
        }

        // Positive tone favours highs over lows. Negative tone does the opposite.
        auto highToLowRatio = [] (float toneSetting)
        {
            CoreDSP::Saturation::ToneControl control;
            control.prepare (sampleRate);

            std::vector<float> lows, highs;

            for (int n = 0; n < 9600; ++n)
            {
                lows.push_back (control.process (sine (n, 100.0, 0.5f), toneSetting));
            }

            control.reset();

            for (int n = 0; n < 9600; ++n)
            {
                highs.push_back (control.process (sine (n, 8000.0, 0.5f), toneSetting));
            }

            return rms (highs, 4800) / rms (lows, 4800);
        };

        assert (highToLowRatio (1.0f) > highToLowRatio (-1.0f) * 2.0f);

        std::puts ("[PASS] testToneControl");
    }

    //==============================================================================
    // Chaos

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

        // Determinism: the same start state gives the same trajectory.
        CoreDSP::Chaos::LorenzAttractor a, b;
        a.advance (3.0);
        b.advance (3.0);
        assert (a.getState().x == b.getState().x);

        // Time limits: a negative advance is ignored and a huge one is capped.
        CoreDSP::Chaos::LorenzAttractor c;
        const auto before = c.getState().x;
        c.advance (-1.0);
        assert (c.getState().x == before);

        CoreDSP::Chaos::LorenzAttractor capped, reference;
        capped.advance (1.0e6);
        reference.advance (CoreDSP::Chaos::LorenzAttractor::maxAdvance);
        assert (capped.getState().x == reference.getState().x);

        // A non-default parameter set produces a different trajectory.
        CoreDSP::Chaos::LorenzAttractor tuned (CoreDSP::Chaos::LorenzParams { 10.0, 99.96, 8.0 / 3.0 });
        tuned.advance (5.0);
        CoreDSP::Chaos::LorenzAttractor standard;
        standard.advance (5.0);
        assert (tuned.getState().x != standard.getState().x);

        std::puts ("[PASS] testLorenzChaos");
    }

    //==============================================================================
    // Spectral

    void testLfoShapes()
    {
        const float halfPi = juce::MathConstants<float>::halfPi;
        const float pi = juce::MathConstants<float>::pi;

        // Sine matches the sine function.
        assert (std::abs (lfoValue (LfoShape::sine, 0.0f)) < 1e-6f);
        assert (std::abs (lfoValue (LfoShape::sine, halfPi) - 1.0f) < 1e-6f);

        // Triangle runs from -1 at 0 up to +1 at pi.
        assert (std::abs (lfoValue (LfoShape::triangle, 0.0f) + 1.0f) < 1e-6f);
        assert (std::abs (lfoValue (LfoShape::triangle, pi) - 1.0f) < 1e-6f);

        // Saw ramps from -1 towards +1 over one cycle.
        assert (std::abs (lfoValue (LfoShape::saw, 0.0f) + 1.0f) < 1e-6f);
        assert (lfoValue (LfoShape::saw, 1.5f * pi) > lfoValue (LfoShape::saw, 0.5f * pi));

        // Square is +1 in the first half cycle and -1 in the second.
        assert (lfoValue (LfoShape::square, 0.5f * pi) == 1.0f);
        assert (lfoValue (LfoShape::square, 1.5f * pi) == -1.0f);

        // Every shape stays in [-1, 1], including phases outside one cycle.
        for (int shape = 0; shape < 4; ++shape)
            for (float p = -10.0f; p < 10.0f; p += 0.05f)
            {
                const float v = lfoValue ((LfoShape) shape, p);
                assert (v >= -1.0f - 1e-6f && v <= 1.0f + 1e-6f);
            }

        std::puts ("[PASS] testLfoShapes");
    }

    void testSpectralFFT()
    {
        // Silence in must give a silent spectrum out.
        {
            DriftEngine engine;
            engine.prepare (sampleRate);

            for (int n = 0; n < 4 * frameSize; ++n)
                engine.processSample (0.0f);

            for (float magnitude : engine.getOutputMagnitudes())
                assert (magnitude < 1e-6f);
        }

        // A pure tone on bin 64 must show its energy at bin 64.
        {
            DriftEngine engine;
            engine.prepare (sampleRate);
            DriftEngine::Settings settings;
            settings.driftDepth = 0.0f;
            engine.setSettings (settings);

            const int bin = 64;
            for (int n = 0; n < 8 * frameSize; ++n)
                engine.processSample (static_cast<float> (std::sin (2.0 * pi * bin * n / frameSize)));

            const auto& spectrum = engine.getOutputMagnitudes();
            assert (peakBin (spectrum) == bin);
            assert (spectrum[(size_t) bin] > 100.0f);
        }

        std::puts ("[PASS] testSpectralFFT");
    }

    void testSpectralReconstruction()
    {
        // With no drift, no feedback, no shaping and full wet mix, the engine is a pure delay line:
        // output(n) must equal input(n - latency) to float precision.
        DriftEngine engine;
        engine.prepare (sampleRate);

        DriftEngine::Settings settings;
        settings.driftDepth = 0.0f;
        settings.mix = 1.0f;
        engine.setSettings (settings);

        const int latency = DriftEngine::getLatencySamples();
        const int total = 6 * frameSize;
        std::vector<float> input ((size_t) total);

        for (int n = 0; n < total; ++n)
            input[(size_t) n] = sine (n, 440.0, 0.5f) + sine (n, 3100.0, 0.25f);

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

    void testSpectralDepthAndStereoPhase()
    {
        const int total = 8 * frameSize;

        auto render = [&] (float depth, float stereoPhase)
        {
            DriftEngine engine;
            engine.prepare (sampleRate);
            DriftEngine::Settings settings;
            settings.driftDepth = depth;
            settings.stereoPhase = stereoPhase;
            settings.driftRateHz = 2.0f;
            engine.setSettings (settings);

            return runEngine (engine, total, [] (int n) { return sine (n, 1000.0, 0.5f); });
        };

        const auto dry = render (0.0f, 0.0f);
        const auto drifted = render (1.0f, 0.0f);
        const auto shiftedLfo = render (1.0f, 1.5f);

        // Depth 0 is a pure delay. Non-zero depth changes the sound.
        assert (rms (dry, frameSize) > 0.1f);

        double difference = 0.0;
        for (int n = total - frameSize; n < total; ++n)
            difference += std::abs (drifted[(size_t) n] - dry[(size_t) n]);

        assert (difference / frameSize > 1.0e-3);

        // A different stereo phase gives a different LFO, so the two channels decorrelate.
        double channelDifference = 0.0;
        for (int n = total - frameSize; n < total; ++n)
            channelDifference += std::abs (drifted[(size_t) n] - shiftedLfo[(size_t) n]);

        assert (channelDifference / frameSize > 1.0e-3);

        std::puts ("[PASS] testSpectralDepthAndStereoPhase");
    }

    void testSpectralShift()
    {
        DriftEngine engine;
        engine.prepare (sampleRate);
        DriftEngine::Settings settings;
        settings.driftDepth = 0.0f;
        settings.shiftBins = 8.0f;
        engine.setSettings (settings);

        runEngine (engine, 8 * frameSize, [] (int n) { return sine (n, 64.0 * sampleRate / frameSize, 0.5f); });

        // Content at bin 64 moves up by eight bins.
        assert (peakBin (engine.getOutputMagnitudes()) == 72);

        // Negative shifts move content down.
        DriftEngine down;
        down.prepare (sampleRate);
        settings.shiftBins = -8.0f;
        down.setSettings (settings);

        runEngine (down, 8 * frameSize, [] (int n) { return sine (n, 64.0 * sampleRate / frameSize, 0.5f); });
        assert (peakBin (down.getOutputMagnitudes()) == 56);

        std::puts ("[PASS] testSpectralShift");
    }

    void testSpectralTilt()
    {
        // Two tones, one low and one high. Tilt changes how strongly the high one sits against the low one.
        auto highToLow = [] (float tilt)
        {
            DriftEngine engine;
            engine.prepare (sampleRate);
            DriftEngine::Settings settings;
            settings.driftDepth = 0.0f;
            settings.tilt = tilt;
            engine.setSettings (settings);

            runEngine (engine, 8 * frameSize, [] (int n)
                       {
                           return sine (n, 16.0 * sampleRate / frameSize, 0.25f)
                                + sine (n, 256.0 * sampleRate / frameSize, 0.25f);
                       });

            const auto& spectrum = engine.getOutputMagnitudes();
            return spectrum[256] / spectrum[16];
        };

        const float bright = highToLow (1.0f);
        const float dark = highToLow (-1.0f);

        assert (bright > dark * 10.0f);

        std::puts ("[PASS] testSpectralTilt");
    }

    void testSpectralGate()
    {
        auto peakOutput = [] (float gateDb)
        {
            DriftEngine engine;
            engine.prepare (sampleRate);
            DriftEngine::Settings settings;
            settings.driftDepth = 0.0f;
            settings.gateDb = gateDb;
            engine.setSettings (settings);

            // A quiet tone, about -60 dB relative to full scale.
            runEngine (engine, 8 * frameSize, [] (int n) { return sine (n, 1000.0, 0.001f); });

            float peak = 0.0f;
            for (float m : engine.getOutputMagnitudes())
                peak = std::max (peak, m);
            return peak;
        };

        // The gate at -20 dB removes it. With the gate off it survives.
        assert (peakOutput (-20.0f) < 1e-3f);
        assert (peakOutput (gateOffDb) > 0.1f);

        std::puts ("[PASS] testSpectralGate");
    }

    void testSpectralFreeze()
    {
        DriftEngine engine;
        engine.prepare (sampleRate);
        DriftEngine::Settings settings;
        settings.driftDepth = 0.0f;
        engine.setSettings (settings);

        const int tone = 64;
        const double toneHz = tone * sampleRate / frameSize;

        runEngine (engine, 8 * frameSize, [toneHz] (int n) { return sine (n, toneHz, 0.5f); });

        // Freeze, then stop the input. The captured spectrum keeps sounding.
        settings.frozen = true;
        engine.setSettings (settings);

        runEngine (engine, 8 * frameSize, [] (int) { return 0.0f; });

        assert (engine.getOutputMagnitudes()[(size_t) tone] > 50.0f);

        // The input spectrum display holds its last captured frame while frozen.
        assert (peakBin (engine.getInputMagnitudes()) == tone);

        std::puts ("[PASS] testSpectralFreeze");
    }

    void testSpectralFeedbackIsBounded()
    {
        DriftEngine engine;
        engine.prepare (sampleRate);
        DriftEngine::Settings settings;
        settings.feedback = 0.95f;
        engine.setSettings (settings);

        float peak = 0.0f;

        for (int n = 0; n < 200000; ++n)
        {
            const float input = n == 0 ? 1.0f : 0.0f;
            const float out = engine.processSample (input);

            assert (std::isfinite (out));
            peak = std::max (peak, std::abs (out));
        }

        // The feedback path is soft-limited, so an impulse rings out instead of blowing up.
        assert (peak < 10.0f);
        assert (engine.getFrameCount() > 0);

        std::puts ("[PASS] testSpectralFeedbackIsBounded");
    }

    void testSpectralMix()
    {
        // Mix 0 is the dry signal, delayed to line up with the wet path.
        DriftEngine engine;
        engine.prepare (sampleRate);
        DriftEngine::Settings settings;
        settings.mix = 0.0f;
        settings.driftDepth = 1.0f;
        engine.setSettings (settings);

        const int total = 4 * frameSize;
        std::vector<float> input ((size_t) total);

        for (int n = 0; n < total; ++n)
            input[(size_t) n] = sine (n, 500.0, 0.5f);

        float maxError = 0.0f;

        for (int n = 0; n < total; ++n)
        {
            const float out = engine.processSample (input[(size_t) n]);

            if (n >= frameSize)
                maxError = std::max (maxError, std::abs (out - input[(size_t) (n - frameSize)]));
        }

        assert (maxError < 1e-5f);

        std::puts ("[PASS] testSpectralMix");
    }
}

    //==============================================================================
    // Sequencer rules (SequencerEngine.h)

    void testSequencerRates()
    {
        using namespace CoreDSP::Sequencer;
        assert (stepBeats (0) == 1.0);
        assert (stepBeats (1) == 0.5);
        assert (stepBeats (2) == 0.25);
        assert (stepBeats (3) == 0.125);
        assert (stepBeats (9) == 0.125);   // out-of-range choices clamp to 1/32

        std::puts ("[PASS] testSequencerRates");
    }

    void testHashIsDeterministicAndBounded()
    {
        using namespace CoreDSP::Sequencer;
        for (std::uint32_t seed = 0; seed < 20; ++seed)
            for (std::uint32_t index = 0; index < 200; ++index)
            {
                const float v = hashUnit (seed, index);
                assert (v >= 0.0f && v < 1.0f);
                assert (v == hashUnit (seed, index));
            }

        // Different seeds give different patterns.
        int differences = 0;
        for (std::uint32_t index = 0; index < 64; ++index)
            differences += hashUnit (1, index) != hashUnit (2, index) ? 1 : 0;
        assert (differences > 60);

        std::puts ("[PASS] testHashIsDeterministicAndBounded");
    }

    void testStepFiresFollowsDensity()
    {
        using namespace CoreDSP::Sequencer;
        for (int step = 0; step < 100; ++step)
        {
            assert (! stepFires (7, step, 0.0f));
            assert (stepFires (7, step, 1.0f));
        }

        int fired = 0;
        for (int step = 0; step < 4000; ++step)
            fired += stepFires (7, step, 0.25f) ? 1 : 0;

        assert (fired > 900 && fired < 1100);   // about 25 % of 4000

        std::puts ("[PASS] testStepFiresFollowsDensity");
    }

    void testScalesAndPitch()
    {
        using namespace CoreDSP::Sequencer;
        assert (scaleDefinition (0).size == 12);   // chromatic
        assert (scaleDefinition (1).size == 7);    // major
        assert (scaleDefinition (3).size == 5);    // minor pentatonic
        assert (scaleDefinition (5).size == 6);    // whole tone
        assert (scaleDefinition (42).size == 6);   // out-of-range choices clamp

        assert (degreeCount (1, 2) == 14);
        assert (degreeFromUnit (-1.0f, 1, 2) == 0);
        assert (degreeFromUnit (1.0f, 1, 2) == 13);
        assert (degreeFromUnit (0.0f, 1, 1) == 3);

        assert (notePitch (48, 1, 0) == 48);        // root
        assert (notePitch (48, 1, 2) == 52);        // major third
        assert (notePitch (48, 1, 7) == 60);        // one octave up
        assert (notePitch (60, 0, 13) == 73);       // chromatic: 13 semitones up
        assert (notePitch (120, 0, 12) == 127);     // clamped to MIDI range

        std::puts ("[PASS] testScalesAndPitch");
    }

    void testBlendDegree()
    {
        using namespace CoreDSP::Sequencer;
        assert (blendDegree (3, 9, 0.0f, 14) == 3);   // no chaos: the fixed melody
        assert (blendDegree (3, 9, 1.0f, 14) == 9);   // full chaos: the attractor's melody
        assert (blendDegree (3, 9, 0.5f, 14) == 6);
        assert (blendDegree (0, 13, 2.0f, 14) == 13); // chaos clamps to [0, 1]

        std::puts ("[PASS] testBlendDegree");
    }

    void testTimingSwingGateVelocity()
    {
        using namespace CoreDSP::Sequencer;

        // Timing: zero chaos gives no shift, and the shift stays within the range.
        assert (timingOffsetMs (0.0f, 40.0f, 1.0f) == 0.0f);
        assert (std::abs (timingOffsetMs (1.0f, 40.0f, 1.0f) - 40.0f) < 1e-4f);
        assert (std::abs (timingOffsetMs (1.0f, 40.0f, -0.5f) + 20.0f) < 1e-4f);

        // Swing moves only the odd steps, by a fraction of the step.
        assert (swingBeats (0, 0.5f, 0.25) == 0.0);
        assert (std::abs (swingBeats (1, 0.5f, 0.25) - 0.125) < 1e-9);
        assert (std::abs (swingBeats (1, -0.5f, 0.25) + 0.125) < 1e-9);   // negative swing moves odd steps early
        assert (std::abs (swingBeats (1, 2.0f, 0.25) - 0.25) < 1e-9);     // clamps to one full step

        // Gate never exceeds one step and never drops below 5 %.
        for (float y = -1.0f; y <= 1.0f; y += 0.25f)
            for (float gate : { 0.0f, 0.5f, 1.0f })
            {
                const double beats = gateBeats (0.25, gate, 1.0f, y);
                assert (beats >= 0.05 * 0.25 - 1e-9 && beats <= 0.25 + 1e-9);
            }

        // Velocity: without velocity chaos the base velocity is kept, and it stays in MIDI range.
        assert (velocityFor (100, 0.0f, 1.0f) == 100);
        assert (velocityFor (100, 1.0f, 1.0f) == 127);   // 100 * 1.5, clamped to the MIDI maximum
        assert (velocityFor (100, 1.0f, -1.0f) == 50);
        assert (velocityFor (127, 1.0f, 1.0f) == 127);
        assert (velocityFor (1, 1.0f, -1.0f) >= 1);

        std::puts ("[PASS] testTimingSwingGateVelocity");
    }

    void testSpectralFeedbackDampingAndDriftStart()
    {
        // Damping changes the echoes: a damped loop differs from a bright one.
        auto echoes = [] (float damping)
        {
            DriftEngine engine;
            engine.prepare (sampleRate);
            DriftEngine::Settings settings;
            settings.feedback = 0.5f;
            settings.feedbackDamping = damping;
            engine.setSettings (settings);
            return runEngine (engine, 6 * frameSize, [] (int n) { return n == 0 ? 1.0f : 0.0f; });
        };

        const auto bright = echoes (0.0f);
        const auto dark = echoes (1.0f);
        float difference = 0.0f;
        for (size_t n = 0; n < bright.size(); ++n)
            difference = std::max (difference, std::abs (bright[n] - dark[n]));
        assert (difference > 1.0e-3f);

        // Drift start at 100 % leaves every bin alone, which equals a drift depth of zero.
        DriftEngine plain, started;
        plain.prepare (sampleRate);
        started.prepare (sampleRate);
        DriftEngine::Settings none, allBinsStill;
        none.driftDepth = 0.0f;
        allBinsStill.driftStart = 1.0f;
        plain.setSettings (none);
        started.setSettings (allBinsStill);

        const int total = 6 * frameSize;
        auto signal = [] (int n) { return sine (n, 700.0, 0.5f); };
        const auto a = runEngine (plain, total, signal);
        const auto b = runEngine (started, total, signal);
        for (size_t n = 0; n < a.size(); ++n)
            assert (std::abs (a[n] - b[n]) < 1e-5f);

        std::puts ("[PASS] testSpectralFeedbackDampingAndDriftStart");
    }

    //==============================================================================
    // Modulation (LFO shapes, sample and hold, modulation maths)

    void testModulationShapes()
    {
        namespace Mod = CoreDSP::Modulation;
        constexpr std::uint32_t seed = 0x51A5;

        // Sine: 0 at the start, +1 a quarter of the way, -1 at three quarters.
        assert (std::abs (Mod::lfoValue (Mod::LfoShape::sine, 0.0, seed)) < 1e-6f);
        assert (std::abs (Mod::lfoValue (Mod::LfoShape::sine, 0.25, seed) - 1.0f) < 1e-5f);
        assert (std::abs (Mod::lfoValue (Mod::LfoShape::sine, 0.75, seed) + 1.0f) < 1e-5f);

        // Triangle: starts at -1, peaks at +1 at the half cycle, linear in between.
        assert (std::abs (Mod::lfoValue (Mod::LfoShape::triangle, 0.0, seed) + 1.0f) < 1e-6f);
        assert (std::abs (Mod::lfoValue (Mod::LfoShape::triangle, 0.5, seed) - 1.0f) < 1e-6f);
        assert (std::abs (Mod::lfoValue (Mod::LfoShape::triangle, 0.25, seed)) < 1e-6f);

        // Saw: rises from -1 towards +1.
        assert (std::abs (Mod::lfoValue (Mod::LfoShape::saw, 0.0, seed) + 1.0f) < 1e-6f);
        assert (Mod::lfoValue (Mod::LfoShape::saw, 0.999, seed) > 0.99f);

        // Square: +1 in the first half cycle, -1 in the second.
        assert (Mod::lfoValue (Mod::LfoShape::square, 0.25, seed) == 1.0f);
        assert (Mod::lfoValue (Mod::LfoShape::square, 0.75, seed) == -1.0f);

        // Every shape stays in [-1, 1], also for negative and large cycle counts.
        for (int shape = 0; shape <= 6; ++shape)
            for (double c = -5.0; c < 5.0; c += 0.01)
            {
                const float v = Mod::lfoValue ((Mod::LfoShape) shape, c, seed);
                assert (v >= -1.0f - 1e-6f && v <= 1.0f + 1e-6f);
            }

        // Whole cycles do not change the waveform (checked for the shapes that do not depend on the cycle index).
        for (int shape = 0; shape <= 3; ++shape)
            for (double c = 0.013; c < 1.0; c += 0.05) // offset away from the square edge at 0.5
                assert (std::abs (Mod::lfoValue ((Mod::LfoShape) shape, c, seed) - Mod::lfoValue ((Mod::LfoShape) shape, c + 3.0, seed)) < 1e-5f);

        // modulatedNormalised clamps to [0, 1] and is the identity for a zero offset.
        assert (Mod::modulatedNormalised (0.3f, 0.0f) == 0.3f);
        assert (Mod::modulatedNormalised (0.5f, 0.3f) == 0.8f);
        assert (Mod::modulatedNormalised (1.5f, -0.5f) == 1.0f);
        assert (std::abs (Mod::modulatedNormalised (-0.5f, 0.6f) - 0.1f) < 1e-6f);
        assert (Mod::modulatedNormalised (-0.5f, -0.6f) == 0.0f);

        // Random and Noise are smooth curves that vary: no jumps between neighbouring samples, and real movement.
        for (const auto shape : { Mod::LfoShape::random, Mod::LfoShape::noise })
        {
            double sum = 0.0, sumSquares = 0.0;
            float previous = Mod::lfoValue (shape, 0.0, seed);
            for (int i = 1; i <= 4000; ++i)
            {
                const float value = Mod::lfoValue (shape, i * 0.001, seed);
                assert (std::abs (value - previous) < 0.05f);   // continuous: no jumps
                previous = value;
                sum += value;
                sumSquares += (double) value * value;
            }
            const double mean = sum / 4000.0;
            assert (std::sqrt (sumSquares / 4000.0 - mean * mean) > 0.2);   // varies across the range
        }

        std::puts ("[PASS] testModulationShapes");
    }

    void testSampleAndHold()
    {
        namespace Mod = CoreDSP::Modulation;
        constexpr std::uint32_t seed = 0x51A5;

        // Constant within a cycle, and deterministic.
        const float first = Mod::lfoValue (Mod::LfoShape::sampleAndHold, 4.1, seed);
        assert (first == Mod::lfoValue (Mod::LfoShape::sampleAndHold, 4.9, seed));
        assert (first == Mod::lfoValue (Mod::LfoShape::sampleAndHold, 4.1, seed));

        // Changes at most cycle boundaries, and stays in range.
        int changes = 0;
        float previous = Mod::lfoValue (Mod::LfoShape::sampleAndHold, 0.5, seed);
        for (int cycle = 1; cycle <= 8; ++cycle)
        {
            const float value = Mod::lfoValue (Mod::LfoShape::sampleAndHold, cycle + 0.5, seed);
            assert (value >= -1.0f && value <= 1.0f);
            if (value != previous)
                ++changes;
            previous = value;
        }
        assert (changes >= 6);

        // A different seed gives a different sequence.
        assert (Mod::lfoValue (Mod::LfoShape::sampleAndHold, 3.5, seed) != Mod::lfoValue (Mod::LfoShape::sampleAndHold, 3.5, 0x7B3D));

        std::puts ("[PASS] testSampleAndHold");
    }

    // The division table keeps its saved order, out-of-range indices clamp, and fractionalPart wraps negatives.
    void testModulationTiming()
    {
        using namespace CoreDSP::Modulation;

        const double expected[7] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25 };
        for (int i = 0; i < 7; ++i)
            assert (divisionBeats[i] == expected[i]);

        assert (divisionToBeats (-3) == 16.0);
        assert (divisionToBeats (4) == 1.0);
        assert (divisionToBeats (99) == 0.25);

        assert (fractionalPart (2.25) == 0.25);
        assert (std::abs (fractionalPart (-0.25) - 0.75) < 1e-12);
        assert (fractionalPart (-2.0) == 0.0);

        assert (cyclesFromSeconds (2.0, 0.5) == 1.0);

        std::puts ("[PASS] testModulationTiming");
    }

int main()
{
    std::puts ("Running CoreDSP unit tests...");

    testSaturationCurves();
    testTubeBias();
    testFold();
    testShapeEnvelope();
    testDynamicDrive();
    testEnvelopeFollower();
    testOnePoleFilter();
    testToneControl();
    testLorenzChaos();
    testLfoShapes();
    testSpectralFFT();
    testSpectralReconstruction();
    testSpectralDepthAndStereoPhase();
    testSpectralShift();
    testSpectralTilt();
    testSpectralGate();
    testSpectralFreeze();
    testSpectralFeedbackIsBounded();
    testSpectralMix();
    testSpectralFeedbackDampingAndDriftStart();
    testSequencerRates();
    testHashIsDeterministicAndBounded();
    testStepFiresFollowsDensity();
    testScalesAndPitch();
    testBlendDegree();
    testTimingSwingGateVelocity();
    testModulationShapes();
    testSampleAndHold();
    testModulationTiming();

    std::puts ("All CoreDSP unit tests passed.");
    return 0;
}
