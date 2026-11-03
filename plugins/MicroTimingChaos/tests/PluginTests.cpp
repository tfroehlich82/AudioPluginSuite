// Plugin-level tests for Micro-Timing Chaos. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginProcessor.h"

#include <map>
#include <set>

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr int64_t latency = 2400;             // 50 ms at 48 kHz
    constexpr int64_t sixteenthSamples = 6000;    // 1/16 at 120 BPM: 0.125 s
    constexpr int64_t quarterSamples = 24000;     // 1/4 at 120 BPM: 0.5 s

    using Processor = MicroTimingChaosProcessor;

    struct Delivered
    {
        int64_t time;
        juce::MidiMessage message;
    };

    // A host playhead whose transport and tempo the test controls.
    struct FakePlayHead : juce::AudioPlayHead
    {
        bool playing = false;
        double ppq = 0.0;
        double bpm = 120.0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (playing);
            info.setPpqPosition (ppq);
            info.setBpm (bpm);
            return info;
        }
    };

    // Runs the processor for a number of blocks, starting at globalSample, and returns every MIDI message with its time.
    std::vector<Delivered> run (Processor& processor, int64_t& globalSample, int blocks, const juce::MidiBuffer& input = {})
    {
        std::vector<Delivered> out;
        juce::AudioBuffer<float> audio (2, blockSize);

        for (int block = 0; block < blocks; ++block)
        {
            juce::MidiBuffer midi = block == 0 ? input : juce::MidiBuffer();
            audio.clear();
            const int64_t start = globalSample;

            processor.processBlock (audio, midi);

            for (const auto meta : midi)
                out.push_back ({ start + meta.samplePosition, meta.getMessage() });

            globalSample += blockSize;
        }

        return out;
    }

    std::vector<Delivered> onsOnly (const std::vector<Delivered>& events)
    {
        std::vector<Delivered> result;

        for (const auto& event : events)
            if (event.message.isNoteOn())
                result.push_back (event);

        return result;
    }

    // Sets every parameter needed for a quiet, predictable start: no chaos, no swing, dense pattern.
    void setPlain (Processor& p)
    {
        setParameter (p.apvts, Processor::densityId, 100.0f);
        setParameter (p.apvts, Processor::chaosId, 0.0f);
        setParameter (p.apvts, Processor::timingRangeId, 0.0f);
        setParameter (p.apvts, Processor::swingId, 0.0f);
        setParameter (p.apvts, Processor::velocityChaosId, 0.0f);
        setParameter (p.apvts, Processor::gateId, 50.0f);
    }

    void testParameters()
    {
        Processor processor;

        checkParameterIds (processor, processor.apvts,
                           { Processor::rateId, Processor::lengthId, Processor::densityId, Processor::seedId,
                             Processor::gateId, Processor::swingId, Processor::baseVelocityId, Processor::rootId,
                             Processor::scaleId, Processor::octavesId, Processor::chaosId, Processor::timingRangeId,
                             Processor::velocityChaosId, Processor::lerpId, Processor::lerpXMultId, Processor::lerpYMultId,
                             Processor::lerpZMultId, Processor::speedId, Processor::bpmId, Processor::syncId },
                           Processor::numControls);

        checkRange (processor.apvts, Processor::rateId, 0.0f, 3.0f, 2.0f);
        checkRange (processor.apvts, Processor::lengthId, 2.0f, 16.0f, 8.0f);
        checkRange (processor.apvts, Processor::densityId, 0.0f, 100.0f, 70.0f);
        checkRange (processor.apvts, Processor::seedId, 0.0f, 999.0f, 42.0f);
        checkRange (processor.apvts, Processor::gateId, 10.0f, 100.0f, 60.0f);
        checkRange (processor.apvts, Processor::swingId, -20.0f, 20.0f, 0.0f);
        checkRange (processor.apvts, Processor::baseVelocityId, 30.0f, 127.0f, 100.0f);
        checkRange (processor.apvts, Processor::rootId, 24.0f, 72.0f, 48.0f);
        checkRange (processor.apvts, Processor::scaleId, 0.0f, 5.0f, 2.0f);
        checkRange (processor.apvts, Processor::octavesId, 1.0f, 3.0f, 2.0f);
        checkRange (processor.apvts, Processor::chaosId, 0.0f, 100.0f, 50.0f);
        checkRange (processor.apvts, Processor::timingRangeId, 0.0f, 50.0f, 20.0f);
        checkRange (processor.apvts, Processor::velocityChaosId, 0.0f, 100.0f, 40.0f);
        checkRange (processor.apvts, Processor::lerpId, 0.0f, 100.0f, 50.0f);
        checkRange (processor.apvts, Processor::lerpXMultId, 0.0f, 2.0f, 1.0f);
        checkRange (processor.apvts, Processor::lerpYMultId, 0.0f, 2.0f, 1.0f);
        checkRange (processor.apvts, Processor::lerpZMultId, 0.0f, 2.0f, 1.0f);
        checkRange (processor.apvts, Processor::speedId, 0.5f, 2.0f, 1.0f);
        checkRange (processor.apvts, Processor::bpmId, 40.0f, 240.0f, 120.0f);
        checkRange (processor.apvts, Processor::syncId, 0.0f, 1.0f, 0.0f);

        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;
        checkStateRoundTrip (processor, processor.apvts, Processor::lengthId, 12.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::chaosId, 80.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::lerpId, 60.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::lerpXMultId, 1.5f);

        pass ("testStateRoundTrip");
    }

    void testLatencyIs50Milliseconds()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        assert (processor.getLatencySamples() == 2400);

        Processor atOtherRate;
        atOtherRate.prepareToPlay (44100.0, blockSize);
        assert (atOtherRate.getLatencySamples() == 2205);

        pass ("testLatencyIs50Milliseconds");
    }

    void testAudioPassesThroughUnchanged()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        addSine (audio, 0, sampleRate, 220.0, 0.6f, 0);
        addSine (audio, 1, sampleRate, 330.0, 0.3f, 0);

        const juce::AudioBuffer<float> reference (audio);
        juce::MidiBuffer midi;
        processor.processBlock (audio, midi);

        for (int channel = 0; channel < 2; ++channel)
            for (int n = 0; n < blockSize; ++n)
                assert (audio.getSample (channel, n) == reference.getSample (channel, n));

        pass ("testAudioPassesThroughUnchanged");
    }

    void testZeroDensityIsSilent()
    {
        Processor processor;
        setPlain (processor);
        setParameter (processor.apvts, Processor::densityId, 0.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        int64_t sample = 0;
        const auto events = run (processor, sample, 200);

        assert (onsOnly (events).empty());
        assert (processor.getRecentSteps().size() > 0);

        pass ("testZeroDensityIsSilent");
    }

    void testStepTimingIsExactWithoutChaos()
    {
        Processor processor;
        setPlain (processor);
        setParameter (processor.apvts, Processor::rateId, 0.0f);        // 1/4
        setParameter (processor.apvts, Processor::lengthId, 4.0f);
        setParameter (processor.apvts, Processor::bpmId, 120.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        int64_t sample = 0;
        const auto ons = onsOnly (run (processor, sample, 400));   // about 4.3 s, 8 quarter notes

        assert (ons.size() == 9 || ons.size() == 8);

        for (size_t k = 0; k < ons.size(); ++k)
            assert (std::abs (ons[k].time - (latency + (int64_t) k * quarterSamples)) <= 1);

        pass ("testStepTimingIsExactWithoutChaos");
    }

    void testSwingDelaysOddSteps()
    {
        Processor processor;
        setPlain (processor);
        setParameter (processor.apvts, Processor::rateId, 0.0f);
        setParameter (processor.apvts, Processor::lengthId, 4.0f);
        setParameter (processor.apvts, Processor::swingId, 20.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        int64_t sample = 0;
        const auto ons = onsOnly (run (processor, sample, 200));

        // Odd steps move back by 20 % of a step: 4800 samples at 1/4 and 120 BPM.
        for (size_t k = 0; k < 4; ++k)
        {
            const int64_t swing = (k % 2 == 1) ? quarterSamples / 5 : 0;
            assert (std::abs (ons[k].time - (latency + (int64_t) k * quarterSamples + swing)) <= 1);
        }

        pass ("testSwingDelaysOddSteps");
    }

    void testChaosMovesTiming()
    {
        Processor processor;
        setPlain (processor);
        setParameter (processor.apvts, Processor::rateId, 2.0f);        // 1/16
        setParameter (processor.apvts, Processor::lengthId, 16.0f);
        setParameter (processor.apvts, Processor::chaosId, 100.0f);
        setParameter (processor.apvts, Processor::timingRangeId, 40.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        int64_t sample = 0;
        const auto ons = onsOnly (run (processor, sample, 1000));
        assert (ons.size() > 40);

        const int64_t limit = (int64_t) std::llround (40.0 * sampleRate / 1000.0) + 2;
        bool moved = false;

        for (size_t k = 0; k < ons.size(); ++k)
        {
            const int64_t offset = ons[k].time - (latency + (int64_t) k * sixteenthSamples);
            assert (std::abs (offset) <= limit);
            moved = moved || std::abs (offset) > 10;
        }

        assert (moved);

        pass ("testChaosMovesTiming");
    }

    void testPatternRepeatsWithoutChaos()
    {
        Processor processor;
        setPlain (processor);
        setParameter (processor.apvts, Processor::rateId, 2.0f);
        setParameter (processor.apvts, Processor::lengthId, 4.0f);
        setParameter (processor.apvts, Processor::scaleId, 1.0f);       // major
        processor.prepareToPlay (sampleRate, blockSize);

        int64_t sample = 0;
        const auto ons = onsOnly (run (processor, sample, 1200));
        assert (ons.size() >= 12);

        for (size_t k = 0; k + 4 < ons.size(); ++k)
            assert (ons[k].message.getNoteNumber() == ons[k + 4].message.getNoteNumber());

        pass ("testPatternRepeatsWithoutChaos");
    }

    void testChaosChangesMelody()
    {
        Processor processor;
        setPlain (processor);
        setParameter (processor.apvts, Processor::rateId, 2.0f);
        setParameter (processor.apvts, Processor::lengthId, 4.0f);
        setParameter (processor.apvts, Processor::chaosId, 100.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        int64_t sample = 0;
        const auto ons = onsOnly (run (processor, sample, 1200));

        std::set<int> distinct;
        for (const auto& event : ons)
            distinct.insert (event.message.getNoteNumber());

        assert (distinct.size() > 3);

        pass ("testChaosChangesMelody");
    }

    void testPitchesStayInScale()
    {
        Processor processor;
        setPlain (processor);
        setParameter (processor.apvts, Processor::rateId, 2.0f);
        setParameter (processor.apvts, Processor::chaosId, 100.0f);
        setParameter (processor.apvts, Processor::scaleId, 1.0f);       // major
        setParameter (processor.apvts, Processor::rootId, 48.0f);
        setParameter (processor.apvts, Processor::octavesId, 2.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        const std::set<int> major { 0, 2, 4, 5, 7, 9, 11 };
        int64_t sample = 0;

        for (const auto& event : onsOnly (run (processor, sample, 800)))
        {
            const int note = event.message.getNoteNumber();
            assert (major.count ((note - 48) % 12) == 1);
            assert (note >= 48 && note <= 71);
        }

        pass ("testPitchesStayInScale");
    }

    void testVelocity()
    {
        auto velocities = [] (float velocityChaos)
        {
            Processor processor;
            setPlain (processor);
            setParameter (processor.apvts, Processor::rateId, 2.0f);
            setParameter (processor.apvts, Processor::chaosId, 100.0f);
            setParameter (processor.apvts, Processor::velocityChaosId, velocityChaos);
            setParameter (processor.apvts, Processor::baseVelocityId, 100.0f);
            processor.prepareToPlay (sampleRate, blockSize);

            int64_t sample = 0;
            std::vector<int> out;

            for (const auto& event : onsOnly (run (processor, sample, 800)))
                out.push_back (event.message.getVelocity());

            return out;
        };

        for (int velocity : velocities (0.0f))
            assert (velocity == 100);

        const auto varied = velocities (100.0f);
        std::set<int> distinct (varied.begin(), varied.end());

        for (int velocity : varied)
        {
            assert (velocity >= 50 && velocity <= 127);
        }

        assert (distinct.size() > 3);

        pass ("testVelocity");
    }

    void testGateEndsNotes()
    {
        Processor processor;
        setPlain (processor);
        setParameter (processor.apvts, Processor::rateId, 2.0f);
        setParameter (processor.apvts, Processor::gateId, 100.0f);
        setParameter (processor.apvts, Processor::chaosId, 100.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        int64_t sample = 0;
        const auto events = run (processor, sample, 600);

        std::map<int, int64_t> onTime;
        int pairs = 0;

        for (const auto& event : events)
        {
            if (event.message.isNoteOn())
                onTime[event.message.getNoteNumber()] = event.time;
            else if (event.message.isNoteOff())
            {
                const auto found = onTime.find (event.message.getNoteNumber());
                if (found != onTime.end())
                {
                    // A note never lasts longer than one step.
                    assert (event.time - found->second <= sixteenthSamples + 1);
                    assert (event.time > found->second);
                    onTime.erase (found);
                    ++pairs;
                }
            }
        }

        assert (pairs > 20);

        pass ("testGateEndsNotes");
    }

    // Retriggering the same note while it sounds must never leave it hanging.
    void testNoStuckNotes()
    {
        Processor processor;
        setPlain (processor);
        setParameter (processor.apvts, Processor::rateId, 3.0f);         // 1/32, the densest grid
        setParameter (processor.apvts, Processor::lengthId, 2.0f);       // the same note comes back every other step
        setParameter (processor.apvts, Processor::octavesId, 1.0f);
        setParameter (processor.apvts, Processor::gateId, 100.0f);
        setParameter (processor.apvts, Processor::chaosId, 100.0f);
        setParameter (processor.apvts, Processor::timingRangeId, 50.0f);
        setParameter (processor.apvts, Processor::speedId, 2.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        int64_t sample = 0;
        std::vector<Delivered> events = run (processor, sample, 500);

        // Stop generating and let everything queued finish.
        setParameter (processor.apvts, Processor::densityId, 0.0f);
        const auto tail = run (processor, sample, 200);
        events.insert (events.end(), tail.begin(), tail.end());

        std::map<int, bool> on;
        int ons = 0;
        int offs = 0;

        for (const auto& event : events)
        {
            const int note = event.message.getNoteNumber();

            if (event.message.isNoteOn())
            {
                assert (! on[note]);       // never a second note-on while the first still sounds
                on[note] = true;
                ++ons;
            }
            else if (event.message.isNoteOff())
            {
                on[note] = false;
                ++offs;
            }
        }

        // About 85 steps in 5.3 s at 1/32, all firing, so well over 60 notes.
        assert (ons > 60);
        assert (ons == offs);

        pass ("testNoStuckNotes");
    }

    void testHostSyncFollowsTempo()
    {
        Processor processor;
        setPlain (processor);
        setParameter (processor.apvts, Processor::rateId, 0.0f);         // 1/4
        setParameter (processor.apvts, Processor::syncId, 1.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        FakePlayHead playhead;
        playhead.bpm = 60.0;                                             // one beat per second
        processor.setPlayHead (&playhead);

        int64_t sample = 0;
        juce::AudioBuffer<float> audio (2, blockSize);
        std::vector<int64_t> onTimes;

        playhead.playing = true;

        for (int block = 0; block < 400; ++block)                        // about 4.3 s
        {
            juce::MidiBuffer midi;
            audio.clear();
            const int64_t start = sample;

            processor.processBlock (audio, midi);

            for (const auto meta : midi)
                if (meta.getMessage().isNoteOn())
                    onTimes.push_back (start + meta.samplePosition);

            sample += blockSize;
            playhead.ppq += blockSize / sampleRate;                      // beats advance with the sample clock
        }

        processor.setPlayHead (nullptr);

        // One quarter note per second, so four or five steps in 4.3 s, each one second apart.
        assert (onTimes.size() == 4 || onTimes.size() == 5);

        for (size_t k = 1; k < onTimes.size(); ++k)
            assert (std::abs ((onTimes[k] - onTimes[k - 1]) - 48000) <= 2);

        pass ("testHostSyncFollowsTempo");
    }

    void testTransportStopSilencesNotes()
    {
        Processor processor;
        setPlain (processor);
        setParameter (processor.apvts, Processor::rateId, 2.0f);
        setParameter (processor.apvts, Processor::syncId, 1.0f);
        setParameter (processor.apvts, Processor::gateId, 100.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        FakePlayHead playhead;
        processor.setPlayHead (&playhead);

        int64_t sample = 0;
        juce::AudioBuffer<float> audio (2, blockSize);
        std::vector<Delivered> events;

        playhead.playing = true;
        for (int block = 0; block < 200; ++block)
        {
            juce::MidiBuffer midi;
            audio.clear();
            const int64_t start = sample;
            processor.processBlock (audio, midi);
            for (const auto meta : midi)
                events.push_back ({ start + meta.samplePosition, meta.getMessage() });

            sample += blockSize;
            playhead.ppq += blockSize / sampleRate * 2.0;
        }

        // Stop the transport. Every note still sounding must end, and nothing new may start.
        playhead.playing = false;
        const int64_t stopTime = sample;

        for (int block = 0; block < 100; ++block)
        {
            juce::MidiBuffer midi;
            audio.clear();
            const int64_t start = sample;
            processor.processBlock (audio, midi);
            for (const auto meta : midi)
                events.push_back ({ start + meta.samplePosition, meta.getMessage() });
            sample += blockSize;
        }

        // The playhead reports stopped once the stopped transport has been processed.
        assert (processor.getPlayheadStep() == -1);

        processor.setPlayHead (nullptr);

        std::map<int, int> balance;
        for (const auto& event : events)
        {
            if (event.message.isNoteOn())
            {
                assert (event.time < stopTime);
                ++balance[event.message.getNoteNumber()];
            }
            else if (event.message.isNoteOff())
                --balance[event.message.getNoteNumber()];
        }

        for (const auto& entry : balance)
            assert (entry.second == 0);

        pass ("testTransportStopSilencesNotes");
    }

    void testPassesInputNotesWithLatency()
    {
        Processor processor;
        setPlain (processor);
        setParameter (processor.apvts, Processor::densityId, 0.0f);      // only the input plays
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer input;
        input.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 90), 100);
        input.addEvent (juce::MidiMessage::noteOff (1, 64), 4000);

        int64_t sample = 0;
        const auto events = run (processor, sample, 30, input);

        bool sawOn = false, sawOff = false;
        for (const auto& event : events)
        {
            if (event.message.isNoteOn() && event.message.getNoteNumber() == 64)
            {
                assert (event.time == 100 + latency);
                sawOn = true;
            }
            if (event.message.isNoteOff() && event.message.getNoteNumber() == 64)
            {
                assert (event.time == 4000 + latency);
                sawOff = true;
            }
        }

        assert (sawOn && sawOff);

        pass ("testPassesInputNotesWithLatency");
    }

    void testPlayheadAndLog()
    {
        Processor processor;
        setPlain (processor);
        setParameter (processor.apvts, Processor::rateId, 2.0f);
        setParameter (processor.apvts, Processor::lengthId, 4.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        int64_t sample = 0;
        run (processor, sample, 100);

        // Free running, so the playhead moves and the step log fills up.
        assert (processor.getPlayheadStep() >= 0 && processor.getPlayheadStep() < 4);
        assert (processor.getPatternLength() == 4);

        int logged = 0;
        for (const auto& step : processor.getRecentSteps())
        {
            if (step.velocity > 0.0f)
            {
                assert (step.patternStep >= 0 && step.patternStep < 4);
                assert (step.note >= 0 && step.note <= 127);
                ++logged;
            }
        }

        assert (logged > 4);

        pass ("testPlayheadAndLog");
    }

    void testLerpCurveDefaults()
    {
        Processor processor;
        const auto points = processor.getLerpCurve();

        assert (points.size() == (size_t) Processor::curvePointCount);
        for (int i = 0; i < Processor::curvePointCount; ++i)
            assert (std::abs (points[(size_t) i].x - (float) i / 4.0f) < 1e-6f);

        // Natural default: logarithmic, so it rises quickly and then flattens
        assert (processor.evaluateLerpCurve (0.0f) < 1e-6f);
        assert (std::abs (processor.evaluateLerpCurve (1.0f) - 1.0f) < 1e-6f);
        assert (std::abs (processor.evaluateLerpCurve (0.5f) - std::log10 (5.5f)) < 1e-5f);
        assert (processor.evaluateLerpCurve (0.5f) > 0.5f);

        pass ("testLerpCurveDefaults");
    }

    void testLerpCurveEditing()
    {
        Processor processor;

        // Interior points stay between their neighbours
        processor.setLerpCurvePoint (2, { 0.9f, 0.1f });
        assert (processor.getLerpCurve()[2].x < processor.getLerpCurve()[3].x);
        assert (std::abs (processor.getLerpCurve()[2].y - 0.1f) < 1e-6f);

        // End points keep x fixed, and y stays in [0, 1]
        processor.setLerpCurvePoint (0, { 0.5f, 0.7f });
        processor.setLerpCurvePoint (4, { 0.2f, 2.0f });
        assert (processor.getLerpCurve()[0].x == 0.0f);
        assert (std::abs (processor.getLerpCurve()[0].y - 0.7f) < 1e-6f);
        assert (processor.getLerpCurve()[4].x == 1.0f);
        assert (processor.getLerpCurve()[4].y == 1.0f);

        processor.resetLerpCurve();
        assert (std::abs (processor.getLerpCurve()[2].y - std::log10 (5.5f)) < 1e-5f);

        pass ("testLerpCurveEditing");
    }

    void testLerpCurveSurvivesStateRoundTrip()
    {
        Processor original;
        original.setLerpCurvePoint (1, { 0.3f, 0.85f });

        juce::MemoryBlock state;
        original.getStateInformation (state);

        Processor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());

        const auto before = original.getLerpCurve();
        const auto after = restored.getLerpCurve();
        for (size_t i = 0; i < before.size(); ++i)
        {
            assert (std::abs (before[i].x - after[i].x) < 1e-3f);
            assert (std::abs (before[i].y - after[i].y) < 1e-3f);
        }

        pass ("testLerpCurveSurvivesStateRoundTrip");
    }

    // Drift is curve(chaos) * multiplier, scaled by Lerp. At Lerp 0 nothing drifts, and a zero multiplier switches its axis off.
    void testLerpDriftIsHybrid()
    {
        {
            Processor processor;
            setPlain (processor);
            setParameter (processor.apvts, Processor::rateId, 2.0f);
            setParameter (processor.apvts, Processor::lengthId, 16.0f);
            setParameter (processor.apvts, Processor::chaosId, 100.0f);
            setParameter (processor.apvts, Processor::timingRangeId, 40.0f);
            setParameter (processor.apvts, Processor::lerpId, 0.0f);
            processor.prepareToPlay (sampleRate, blockSize);

            int64_t sample = 0;
            const auto ons = onsOnly (run (processor, sample, 500));
            assert (ons.size() > 20);

            for (size_t k = 0; k < ons.size(); ++k)
                assert (std::abs (ons[k].time - (latency + (int64_t) k * sixteenthSamples)) <= 1);

            assert (processor.getDriftAmounts().timing == 0.0f);
        }

        {
            Processor processor;
            setPlain (processor);
            setParameter (processor.apvts, Processor::rateId, 2.0f);
            setParameter (processor.apvts, Processor::lengthId, 16.0f);
            setParameter (processor.apvts, Processor::chaosId, 100.0f);
            setParameter (processor.apvts, Processor::timingRangeId, 40.0f);
            setParameter (processor.apvts, Processor::lerpId, 100.0f);
            setParameter (processor.apvts, Processor::lerpXMultId, 0.0f);
            processor.prepareToPlay (sampleRate, blockSize);

            int64_t sample = 0;
            const auto ons = onsOnly (run (processor, sample, 500));
            assert (ons.size() > 20);

            for (size_t k = 0; k < ons.size(); ++k)
                assert (std::abs (ons[k].time - (latency + (int64_t) k * sixteenthSamples)) <= 1);

            assert (processor.getDriftAmounts().timing == 0.0f);
            assert (processor.getDriftAmounts().pitch > 0.5f);
        }

        pass ("testLerpDriftIsHybrid");
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

    std::puts ("Running Micro-Timing Chaos plugin tests...");

    testParameters();
    testStateRoundTrip();
    testLatencyIs50Milliseconds();
    testAudioPassesThroughUnchanged();
    testZeroDensityIsSilent();
    testStepTimingIsExactWithoutChaos();
    testSwingDelaysOddSteps();
    testChaosMovesTiming();
    testPatternRepeatsWithoutChaos();
    testChaosChangesMelody();
    testPitchesStayInScale();
    testVelocity();
    testGateEndsNotes();
    testNoStuckNotes();
    testHostSyncFollowsTempo();
    testTransportStopSilencesNotes();
    testPassesInputNotesWithLatency();
    testPlayheadAndLog();
    testLerpCurveDefaults();
    testLerpCurveEditing();
    testLerpCurveSurvivesStateRoundTrip();
    testLerpDriftIsHybrid();
    testEditor();

    std::puts ("All Micro-Timing Chaos plugin tests passed.");
    return 0;
}
