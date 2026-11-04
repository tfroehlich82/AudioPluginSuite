// Plugin-level tests for Dynamic Saturation Companion. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginProcessor.h"

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = DynamicSaturationProcessor;

    // Runs a processor over a sine for a number of blocks and returns the output of the left channel.
    std::vector<float> renderSine (Processor& processor, double hz, float amplitude, int blocks)
    {
        processor.prepareToPlay (sampleRate, blockSize);

        std::vector<float> output;
        juce::MidiBuffer midi;

        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, hz, amplitude, (int64_t) block * blockSize);
            addSine (audio, 1, sampleRate, hz, amplitude, (int64_t) block * blockSize);

            processor.processBlock (audio, midi);

            for (int n = 0; n < blockSize; ++n)
                output.push_back (audio.getSample (0, n));
        }

        return output;
    }

    void testParameters()
    {
        Processor processor;

        checkParameterIds (processor, processor.apvts,
                           { Processor::driveId, Processor::sensitivityId, Processor::modeId, Processor::mixId,
                             Processor::outputLevelId, Processor::attackId, Processor::releaseId,
                             Processor::detectHpfId, Processor::stereoLinkId, Processor::biasId,
                             Processor::foldId, Processor::toneId, Processor::curveId },
                           Processor::numControls);

        checkRange (processor.apvts, Processor::driveId, 0.0f, 24.0f, 6.0f);
        checkRange (processor.apvts, Processor::sensitivityId, 0.0f, 1.0f, 0.5f);
        checkRange (processor.apvts, Processor::modeId, 0.0f, 2.0f, 0.0f);
        checkRange (processor.apvts, Processor::mixId, 0.0f, 100.0f, 100.0f);
        checkRange (processor.apvts, Processor::outputLevelId, -12.0f, 6.0f, 0.0f);
        checkRange (processor.apvts, Processor::attackId, 1.0f, 100.0f, 5.0f);
        checkRange (processor.apvts, Processor::releaseId, 50.0f, 2000.0f, 150.0f);
        checkRange (processor.apvts, Processor::detectHpfId, 20.0f, 800.0f, 20.0f);
        checkRange (processor.apvts, Processor::stereoLinkId, 0.0f, 100.0f, 100.0f);
        checkRange (processor.apvts, Processor::biasId, 0.0f, 0.25f, 0.2f);
        checkRange (processor.apvts, Processor::foldId, 0.0f, 50.0f, 0.0f);
        checkRange (processor.apvts, Processor::toneId, -1.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::curveId, 0.5f, 3.0f, 1.0f);

        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;
        checkStateRoundTrip (processor, processor.apvts, Processor::driveId, 18.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::foldId, 50.0f);

        pass ("testStateRoundTrip");
    }

    void testLatencyMatchesOversampler()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        assert (processor.getLatencySamples() == processor.getOversamplingLatency());
        assert (processor.getLatencySamples() > 0);

        pass ("testLatencyMatchesOversampler");
    }

    void testSilenceStaysSilent()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midi;

        for (int block = 0; block < 10; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            processor.processBlock (audio, midi);
            assert (peak (audio) < 1.0e-9f);
        }

        pass ("testSilenceStaysSilent");
    }

    void testOutputIsBounded()
    {
        // Extreme settings: maximum drive, full sensitivity and maximum output level (+6 dB, about 2.0).
        for (int mode = 0; mode < 3; ++mode)
        {
            Processor processor;
            setParameter (processor.apvts, Processor::driveId, 24.0f);
            setParameter (processor.apvts, Processor::sensitivityId, 1.0f);
            setParameter (processor.apvts, Processor::outputLevelId, 6.0f);
            setParameter (processor.apvts, Processor::modeId, (float) mode);
            setParameter (processor.apvts, Processor::foldId, 100.0f);
            setParameter (processor.apvts, Processor::biasId, 0.5f);

            const auto output = renderSine (processor, 440.0, 1.0f, 30);

            for (float sample : output)
            {
                assert (std::isfinite (sample));
                assert (std::abs (sample) <= 2.0f + 1.0e-3f);
            }
        }

        pass ("testOutputIsBounded");
    }

    void testModesDiffer()
    {
        Processor tube, diode;
        setParameter (tube.apvts, Processor::modeId, 0.0f);
        setParameter (diode.apvts, Processor::modeId, 2.0f);

        const auto a = renderSine (tube, 440.0, 0.8f, 20);
        const auto b = renderSine (diode, 440.0, 0.8f, 20);

        double difference = 0.0;
        for (size_t n = a.size() - 2048; n < a.size(); ++n)
            difference += std::abs (a[n] - b[n]);

        assert (difference / 2048.0 > 1.0e-3);

        pass ("testModesDiffer");
    }

    void testMixZeroIsDelayedDry()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::mixId, 0.0f);
        setParameter (processor.apvts, Processor::driveId, 18.0f);
        setParameter (processor.apvts, Processor::outputLevelId, 0.0f);

        const int blocks = 24;
        std::vector<float> input, output;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::MidiBuffer midi;

        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, 250.0, 0.5f, (int64_t) block * blockSize);
            addSine (audio, 1, sampleRate, 250.0, 0.5f, (int64_t) block * blockSize);

            for (int n = 0; n < blockSize; ++n)
                input.push_back (audio.getSample (0, n));

            processor.processBlock (audio, midi);

            for (int n = 0; n < blockSize; ++n)
                output.push_back (audio.getSample (0, n));
        }

        const auto latency = (size_t) processor.getOversamplingLatency();
        float maxError = 0.0f;

        for (size_t n = latency; n < output.size(); ++n)
            maxError = juce::jmax (maxError, std::abs (output[n] - input[n - latency]));

        assert (maxError < 1.0e-4f);

        pass ("testMixZeroIsDelayedDry");
    }

    void testStereoLinkDrivesQuietChannelHarder()
    {
        // Left is loud, right is quiet. With a linked detector the quiet side is pushed by the loud one.
        auto rightRms = [] (float link)
        {
            Processor processor;
            setParameter (processor.apvts, Processor::stereoLinkId, link);
            setParameter (processor.apvts, Processor::sensitivityId, 1.0f);
            processor.prepareToPlay (sampleRate, blockSize);

            juce::MidiBuffer midi;
            double sum = 0.0;
            int count = 0;

            for (int block = 0; block < 40; ++block)
            {
                juce::AudioBuffer<float> audio (2, blockSize);
                audio.clear();
                addSine (audio, 0, sampleRate, 300.0, 0.9f, (int64_t) block * blockSize);
                addSine (audio, 1, sampleRate, 300.0, 0.05f, (int64_t) block * blockSize);
                processor.processBlock (audio, midi);

                if (block >= 20)
                    for (int n = 0; n < blockSize; ++n, ++count)
                        sum += (double) audio.getSample (1, n) * audio.getSample (1, n);
            }

            return (float) std::sqrt (sum / count);
        };

        assert (rightRms (100.0f) > rightRms (0.0f) * 1.05f);

        pass ("testStereoLinkDrivesQuietChannelHarder");
    }

    void testFoldChangesSound()
    {
        Processor plain, folded;
        setParameter (folded.apvts, Processor::foldId, 100.0f);

        const auto a = renderSine (plain, 400.0, 0.8f, 20);
        const auto b = renderSine (folded, 400.0, 0.8f, 20);

        double difference = 0.0;
        for (size_t n = a.size() - 2048; n < a.size(); ++n)
            difference += std::abs (a[n] - b[n]);

        assert (difference / 2048.0 > 1.0e-3);

        pass ("testFoldChangesSound");
    }

    void testEditor()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        checkEditor (processor);

        pass ("testEditor");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Dynamic Saturation plugin tests...");

    testParameters();
    testStateRoundTrip();
    testLatencyMatchesOversampler();
    testSilenceStaysSilent();
    testOutputIsBounded();
    testModesDiffer();
    testMixZeroIsDelayedDry();
    testStereoLinkDrivesQuietChannelHarder();
    testFoldChangesSound();
    testEditor();

    std::puts ("All Dynamic Saturation plugin tests passed.");
    return 0;
}
