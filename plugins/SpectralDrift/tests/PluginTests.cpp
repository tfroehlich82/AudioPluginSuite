// Plugin-level tests for Spectral Drift Shifter. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginProcessor.h"

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = SpectralDriftProcessor;

    void testParameters()
    {
        SpectralDriftProcessor processor;

        checkParameterIds (processor, processor.apvts,
                           { SpectralDriftProcessor::driftRateId, SpectralDriftProcessor::depthId,
                             SpectralDriftProcessor::spreadId, SpectralDriftProcessor::stereoPhaseId,
                             SpectralDriftProcessor::lfoShapeId, SpectralDriftProcessor::shiftId,
                             SpectralDriftProcessor::tiltId, SpectralDriftProcessor::gateId,
                             SpectralDriftProcessor::feedbackId, SpectralDriftProcessor::mixId,
                             SpectralDriftProcessor::freezeId, SpectralDriftProcessor::feedbackDampingId,
                             SpectralDriftProcessor::driftStartId, SpectralDriftProcessor::outputGainId },
                           SpectralDriftProcessor::numControls);

        checkRange (processor.apvts, SpectralDriftProcessor::driftRateId, 0.1f, 10.0f, 1.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::depthId, 0.0f, 100.0f, 19.1f);
        checkRange (processor.apvts, SpectralDriftProcessor::spreadId, 0.0f, 1.0f, 0.05f);
        checkRange (processor.apvts, SpectralDriftProcessor::stereoPhaseId, 0.0f, 180.0f, 90.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::lfoShapeId, 0.0f, 3.0f, 0.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::shiftId, -32.0f, 32.0f, 0.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::tiltId, -2.0f, 2.0f, 0.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::gateId, -90.0f, 0.0f, -90.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::feedbackId, 0.0f, 20.0f, 0.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::feedbackDampingId, 0.0f, 100.0f, 30.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::driftStartId, 0.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::outputGainId, -12.0f, 12.0f, 0.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::mixId, 0.0f, 100.0f, 100.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::freezeId, 0.0f, 1.0f, 0.0f);

        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        SpectralDriftProcessor processor;
        checkStateRoundTrip (processor, processor.apvts, SpectralDriftProcessor::driftRateId, 5.0f);
        checkStateRoundTrip (processor, processor.apvts, SpectralDriftProcessor::lfoShapeId, 2.0f);

        pass ("testStateRoundTrip");
    }

    void testLatencyReported()
    {
        SpectralDriftProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        assert (processor.getLatencySamples() == 2048);

        pass ("testLatencyReported");
    }

    void testBusLayouts()
    {
        SpectralDriftProcessor processor;

        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add (juce::AudioChannelSet::stereo());
        stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        assert (processor.isBusesLayoutSupported (stereo));

        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        assert (processor.isBusesLayoutSupported (mono));

        juce::AudioProcessor::BusesLayout mismatched;
        mismatched.inputBuses.add (juce::AudioChannelSet::mono());
        mismatched.outputBuses.add (juce::AudioChannelSet::stereo());
        assert (! processor.isBusesLayoutSupported (mismatched));

        pass ("testBusLayouts");
    }

    // Regression test for the empty preview: once audio flows, the spectrum must show the tone.
    void testSignalIsPublishedToSpectrum()
    {
        SpectralDriftProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;
        int64_t first = 0;

        for (int block = 0; block < 40; ++block)
        {
            audio.clear();
            addSine (audio, 0, sampleRate, 1000.0, 0.5f, first);
            addSine (audio, 1, sampleRate, 1000.0, 0.5f, first);
            processor.processBlock (audio, midi);
            first += blockSize;
        }

        const auto& snapshot = processor.getSpectrumSnapshot();
        assert (snapshot.frames.load() > 0);

        // 1 kHz sits in bin 1000 / (48000 / 2048), which is about 43.
        int peakBin = 1;
        float peakValue = 0.0f;

        for (int k = 1; k < SpectralDriftProcessor::numBins; ++k)
        {
            const float value = snapshot.input[(size_t) k].load();
            if (value > peakValue)
            {
                peakValue = value;
                peakBin = k;
            }
        }

        assert (peakValue > 10.0f);
        assert (std::abs (peakBin - 43) <= 1);

        pass ("testSignalIsPublishedToSpectrum");
    }

    void testSilenceIsNotShownAsSignal()
    {
        SpectralDriftProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;

        for (int block = 0; block < 20; ++block)
        {
            audio.clear();
            processor.processBlock (audio, midi);
        }

        const auto& snapshot = processor.getSpectrumSnapshot();
        float peakValue = 0.0f;

        for (int k = 0; k < SpectralDriftProcessor::numBins; ++k)
            peakValue = juce::jmax (peakValue, snapshot.input[(size_t) k].load());

        assert (peakValue < 1.0e-6f);

        pass ("testSilenceIsNotShownAsSignal");
    }

    void testFreezeSustainsSpectrum()
    {
        SpectralDriftProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;
        int64_t first = 0;

        for (int block = 0; block < 30; ++block)
        {
            audio.clear();
            addSine (audio, 0, sampleRate, 1000.0, 0.5f, first);
            addSine (audio, 1, sampleRate, 1000.0, 0.5f, first);
            processor.processBlock (audio, midi);
            first += blockSize;
        }

        setParameter (processor.apvts, SpectralDriftProcessor::freezeId, 1.0f);

        for (int block = 0; block < 30; ++block)
        {
            audio.clear();
            processor.processBlock (audio, midi);
        }

        // Input is silent, yet the frozen spectrum keeps the output sounding.
        assert (peak (audio) > 0.05f);
        assert (allFinite (audio));

        pass ("testFreezeSustainsSpectrum");
    }

    void testStereoPhaseDecorrelatesChannels()
    {
        auto channelDifference = [] (float stereoPhase)
        {
            SpectralDriftProcessor processor;
            setParameter (processor.apvts, SpectralDriftProcessor::stereoPhaseId, stereoPhase);
            processor.prepareToPlay (sampleRate, blockSize);

            juce::AudioBuffer<float> audio (2, blockSize);
            juce::MidiBuffer midi;
            int64_t first = 0;
            float difference = 0.0f;

            for (int block = 0; block < 30; ++block)
            {
                audio.clear();
                addSine (audio, 0, sampleRate, 700.0, 0.5f, first);
                addSine (audio, 1, sampleRate, 700.0, 0.5f, first);
                processor.processBlock (audio, midi);
                first += blockSize;

                for (int n = 0; n < blockSize; ++n)
                    difference = juce::jmax (difference, std::abs (audio.getSample (0, n) - audio.getSample (1, n)));
            }

            return difference;
        };

        // Identical channels stay identical when the LFOs are in phase, and separate when they are not.
        assert (channelDifference (0.0f) < 1.0e-5f);
        assert (channelDifference (90.0f) > 1.0e-3f);

        pass ("testStereoPhaseDecorrelatesChannels");
    }

    void testMixZeroIsDelayedDry()
    {
        SpectralDriftProcessor processor;
        setParameter (processor.apvts, SpectralDriftProcessor::mixId, 0.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        const int latency = processor.getLatencySamples();
        const int blocks = 24;
        std::vector<float> input, output;

        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, 333.0, 0.4f, (int64_t) block * blockSize);
            juce::MidiBuffer midi;

            for (int n = 0; n < blockSize; ++n)
                input.push_back (audio.getSample (0, n));

            processor.processBlock (audio, midi);

            for (int n = 0; n < blockSize; ++n)
                output.push_back (audio.getSample (0, n));
        }

        float maxError = 0.0f;

        for (size_t n = (size_t) latency; n < output.size(); ++n)
            maxError = juce::jmax (maxError, std::abs (output[n] - input[n - (size_t) latency]));

        assert (maxError < 1.0e-5f);

        pass ("testMixZeroIsDelayedDry");
    }


    // Renders the left channel of a processor fed with a 1 kHz sine, for a number of blocks.
    std::vector<float> renderLeft (Processor& processor, int blocks, float amplitude = 0.5f)
    {
        processor.prepareToPlay (sampleRate, blockSize);
        juce::MidiBuffer midi;
        int64_t first = 0;
        std::vector<float> out;

        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, 1000.0, amplitude, first);
            addSine (audio, 1, sampleRate, 1000.0, amplitude, first);
            processor.processBlock (audio, midi);
            first += blockSize;

            for (int n = 0; n < blockSize; ++n)
                out.push_back (audio.getSample (0, n));
        }

        return out;
    }

    float rmsOfTail (const std::vector<float>& signal, int count)
    {
        double sum = 0.0;
        for (size_t i = signal.size() - (size_t) count; i < signal.size(); ++i)
            sum += (double) signal[i] * signal[i];
        return (float) std::sqrt (sum / count);
    }

    void testOutputGainScalesSignal()
    {
        Processor quiet, loud;
        setParameter (quiet.apvts, Processor::mixId, 0.0f);
        setParameter (loud.apvts, Processor::mixId, 0.0f);
        setParameter (loud.apvts, Processor::outputGainId, 6.0f);

        const float ratio = rmsOfTail (renderLeft (loud, 40), 8192) / rmsOfTail (renderLeft (quiet, 40), 8192);

        // +6 dB is a factor of about 2.
        assert (std::abs (ratio - 1.995f) < 0.02f);

        pass ("testOutputGainScalesSignal");
    }

    void testFeedbackDampingChangesEcho()
    {
        // Feedback at its maximum in the plugin. A single click is repeated by the feedback loop.
        auto echoes = [] (float damping)
        {
            Processor processor;
            setParameter (processor.apvts, Processor::feedbackId, 12.0f);
            setParameter (processor.apvts, Processor::feedbackDampingId, damping);
            processor.prepareToPlay (sampleRate, blockSize);
            juce::MidiBuffer midi;
            std::vector<float> out;

            for (int block = 0; block < 60; ++block)
            {
                juce::AudioBuffer<float> audio (2, blockSize);
                audio.clear();
                if (block == 0)
                    audio.setSample (0, 0, 1.0f);
                processor.processBlock (audio, midi);
                for (int n = 0; n < blockSize; ++n)
                    out.push_back (audio.getSample (0, n));
            }

            return out;
        };

        const auto bright = echoes (0.0f);
        const auto dark = echoes (100.0f);

        float difference = 0.0f;
        for (size_t n = 0; n < bright.size(); ++n)
            difference = juce::jmax (difference, std::abs (bright[n] - dark[n]));

        assert (difference > 1.0e-3f);

        pass ("testFeedbackDampingChangesEcho");
    }

    void testDriftStartLeavesLowBinsAlone()
    {
        // With drift starting at 100 % no bin is drifted, which matches a drift depth of zero.
        Processor undrifted, startedAtTop;
        setParameter (undrifted.apvts, Processor::depthId, 0.0f);
        setParameter (startedAtTop.apvts, Processor::driftStartId, 100.0f);

        const auto a = renderLeft (undrifted, 20);
        const auto b = renderLeft (startedAtTop, 20);

        float difference = 0.0f;
        for (size_t n = 0; n < a.size(); ++n)
            difference = juce::jmax (difference, std::abs (a[n] - b[n]));

        assert (difference < 1.0e-5f);

        // With drift starting at 0 % the same settings do change the sound.
        Processor drifted;
        const auto c = renderLeft (drifted, 20);
        float driftDifference = 0.0f;
        for (size_t n = 0; n < a.size(); ++n)
            driftDifference = juce::jmax (driftDifference, std::abs (a[n] - c[n]));

        assert (driftDifference > 1.0e-3f);

        pass ("testDriftStartLeavesLowBinsAlone");
    }

    void testEditor()
    {
        SpectralDriftProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        checkEditor (processor);

        pass ("testEditor");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Spectral Drift plugin tests...");

    testParameters();
    testStateRoundTrip();
    testLatencyReported();
    testBusLayouts();
    testSignalIsPublishedToSpectrum();
    testSilenceIsNotShownAsSignal();
    testFreezeSustainsSpectrum();
    testStereoPhaseDecorrelatesChannels();
    testMixZeroIsDelayedDry();
    testOutputGainScalesSignal();
    testFeedbackDampingChangesEcho();
    testDriftStartLeavesLowBinsAlone();
    testEditor();

    std::puts ("All Spectral Drift plugin tests passed.");
    return 0;
}
