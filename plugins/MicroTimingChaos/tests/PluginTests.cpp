// Plugin-level tests for Micro-Timing Chaos. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "ModulationTestUtils.h"
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
                             Processor::lerpZMultId, Processor::speedId, Processor::bpmId, Processor::syncId,
                             Processor::lfo1RateId, Processor::lfo1ShapeId, Processor::lfo1SyncId, Processor::lfo1DivisionId,
                             Processor::lfo2RateId, Processor::lfo2ShapeId, Processor::lfo2SyncId, Processor::lfo2DivisionId,
                             Processor::lfo1OffsetId, Processor::lfo2OffsetId,
                             Processor::modSourceIds[0], Processor::modTargetIds[0], Processor::modAmountIds[0],
                             Processor::modSourceIds[1], Processor::modTargetIds[1], Processor::modAmountIds[1],
                             Processor::modSourceIds[2], Processor::modTargetIds[2], Processor::modAmountIds[2],
                             Processor::modSourceIds[3], Processor::modTargetIds[3], Processor::modAmountIds[3],
                             Processor::modSourceIds[4], Processor::modTargetIds[4], Processor::modAmountIds[4],
                             Processor::modSourceIds[5], Processor::modTargetIds[5], Processor::modAmountIds[5] },
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

        // LFOs: rate is logarithmic, shapes are 0 to 4, division is 0 to 6 with 1/4 (index 4) as default
        for (const char* rateId : { Processor::lfo1RateId, Processor::lfo2RateId })
            checkRange (processor.apvts, rateId, 0.02f, 20.0f, 0.25f);
        for (const char* shapeId : { Processor::lfo1ShapeId, Processor::lfo2ShapeId })
            checkRange (processor.apvts, shapeId, 0.0f, 6.0f, 0.0f);
        for (const char* syncId : { Processor::lfo1SyncId, Processor::lfo2SyncId })
            checkRange (processor.apvts, syncId, 0.0f, 1.0f, 0.0f);
        for (const char* divisionId : { Processor::lfo1DivisionId, Processor::lfo2DivisionId })
            checkRange (processor.apvts, divisionId, 0.0f, 6.0f, 4.0f);

        // Routing: each slot has a source (0 or 1), a target (0 to 13, None by default) and a signed amount
        for (int slot = 0; slot < Processor::slotCount; ++slot)
        {
            checkRange (processor.apvts, Processor::modSourceIds[slot], 0.0f, 1.0f, 0.0f);
            checkRange (processor.apvts, Processor::modTargetIds[slot], 0.0f, 14.0f, 0.0f);
            checkRange (processor.apvts, Processor::modAmountIds[slot], -100.0f, 100.0f, 0.0f);
        }

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


    //==============================================================================
    // LFO and modulation

    // The shared helpers (setLfo, setRoute) are in tests/common/ModulationTestUtils.h.
    // Timing offset of every note-on from its 1/16 grid position, in samples.
    std::vector<int64_t> timingOffsets (const std::vector<Delivered>& events)
    {
        return PluginTest::timingOffsets (events, latency, sixteenthSamples);
    }

    // A quiet 1/16 patch with timing chaos at full drift, so Timing Range is audible.
    void setTimingPatch (Processor& p, float timingRangeMs)
    {
        setPlain (p);
        setParameter (p.apvts, Processor::rateId, 2.0f);
        setParameter (p.apvts, Processor::lengthId, 16.0f);
        setParameter (p.apvts, Processor::chaosId, 100.0f);
        setParameter (p.apvts, Processor::timingRangeId, timingRangeMs);
        setParameter (p.apvts, Processor::lerpId, 100.0f);
    }

    // The unmodulated event log from the committed code. Chaos 100, 1/16, Timing Range 25 ms, Lerp 80 %,
    // 1500 blocks. With every LFO and routing parameter at its default, the output must match it exactly.
    void testNoModulationIsIdentical()
    {
        struct Expected { int64_t time; bool noteOn; int note; int velocity; };
        static const Expected expected[] = {
        { 2405, true, 58, 84 },
        { 6005, false, 58, 0 },
        { 26011, true, 56, 100 },
        { 29160, false, 56, 0 },
        { 50027, true, 56, 101 },
        { 53294, false, 56, 0 },
        { 73938, true, 56, 102 },
        { 77060, false, 56, 0 },
        { 98050, true, 56, 100 },
        { 101273, false, 56, 0 },
        { 121973, true, 58, 103 },
        { 125217, false, 58, 0 },
        { 145979, true, 55, 100 },
        { 149082, false, 55, 0 },
        { 170069, true, 58, 101 },
        { 173379, false, 58, 0 },
        { 193899, true, 55, 103 },
        { 196994, false, 55, 0 },
        { 218086, true, 58, 99 },
        { 221323, false, 58, 0 },
        { 241962, true, 56, 104 },
        { 245250, false, 56, 0 },
        { 266012, true, 56, 98 },
        { 269099, false, 56, 0 },
        { 290095, true, 56, 102 },
        { 293502, false, 56, 0 },
        { 313910, true, 55, 100 },
        { 316858, false, 55, 0 },
        { 338187, true, 56, 100 },
        { 341648, false, 56, 0 },
        { 361919, true, 55, 98 },
        { 364804, false, 55, 0 },
        { 386170, true, 58, 103 },
        { 389780, false, 58, 0 },
        { 410482, true, 60, 95 },
        { 414225, false, 60, 0 },
        { 434415, true, 58, 90 },
        { 438043, false, 58, 0 },
        { 457898, true, 55, 101 },
        { 460869, false, 55, 0 },
        { 482170, true, 56, 99 },
        { 485572, false, 56, 0 },
        { 505814, true, 55, 103 },
        { 508732, false, 55, 0 },
        { 530245, true, 56, 98 },
        { 533722, false, 56, 0 },
        { 554007, true, 55, 95 },
        { 556954, false, 55, 0 },
        { 577772, true, 50, 98 },
        { 580331, false, 50, 0 },
        { 602569, true, 60, 99 },
        { 606293, false, 60, 0 },
        { 626818, true, 63, 95 },
        { 631095, false, 63, 0 },
        { 651072, true, 62, 110 },
        { 654989, false, 62, 0 },
        { 674360, true, 56, 97 },
        { 677951, false, 56, 0 },
        { 698638, true, 62, 95 },
        { 702609, false, 62, 0 },
        { 723011, true, 60, 108 },
        { 726970, false, 60, 0 },
        { 746417, true, 58, 99 },
        { 749915, false, 58, 0 }

        };

        Processor processor;
        setParameter (processor.apvts, Processor::chaosId, 100.0f);
        setParameter (processor.apvts, Processor::rateId, 2.0f);
        setParameter (processor.apvts, Processor::timingRangeId, 25.0f);
        setParameter (processor.apvts, Processor::lerpId, 80.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        int64_t sample = 0;
        const auto events = run (processor, sample, 1500);
        assert (events.size() == sizeof (expected) / sizeof (expected[0]));

        for (size_t i = 0; i < events.size(); ++i)
        {
            assert (events[i].time == expected[i].time);
            assert (events[i].message.isNoteOn() == expected[i].noteOn);
            assert (events[i].message.getNoteNumber() == expected[i].note);
            assert (events[i].message.getVelocity() == expected[i].velocity);
        }

        pass ("testNoModulationIsIdentical");
    }

    // An LFO on Timing Range moves the timing. At Amount 0 the LFO has no effect at all.
    // The Offset moves the LFO output before it is routed, and the output stops at -1 and +1.
    void testLfoOffset()
    {
        // The output after one block, for a free 1 Hz sine with the given offset. Each call starts at the same phase.
        auto outputAt = [] (float offsetPercent)
        {
            Processor processor;
            setLfo (processor, 0, 1.0f, 0.0f, false, 4.0f);
            setParameter (processor.apvts, Processor::lfo1OffsetId, offsetPercent);
            processor.prepareToPlay (sampleRate, blockSize);

            int64_t sample = 0;
            run (processor, sample, 1);
            return processor.getLfoValue (0);
        };

        const float base = outputAt (0.0f);
        assert (std::abs (outputAt (50.0f) - std::min (1.0f, base + 0.5f)) < 1e-5f);
        assert (std::abs (outputAt (-100.0f) - std::max (-1.0f, base - 1.0f)) < 1e-5f);
        assert (outputAt (100.0f) <= 1.0f && outputAt (-100.0f) >= -1.0f);

        pass ("testLfoOffset");
    }

    void testSineOnTimingRangeChangesOffsets()
    {
        auto offsetsFor = [] (float amount, float lfoHz)
        {
            Processor processor;
            setTimingPatch (processor, 40.0f);
            setLfo (processor, 0, lfoHz, 0.0f, false, 4.0f);
            setRoute (processor, 0, 0, 2, amount);
            processor.prepareToPlay (sampleRate, blockSize);

            int64_t sample = 0;
            return timingOffsets (run (processor, sample, 500));
        };

        const auto modulated = offsetsFor (100.0f, 1.0f);
        const auto steadyOneHz = offsetsFor (0.0f, 1.0f);
        const auto steadyFiveHz = offsetsFor (0.0f, 5.0f);

        assert (modulated.size() > 40);
        assert (steadyOneHz == steadyFiveHz);       // Amount 0: the LFO rate does not matter
        assert (modulated != steadyOneHz);          // Amount 100: the timing changes

        const auto [lowest, highest] = std::minmax_element (modulated.begin(), modulated.end());
        assert (*highest - *lowest > 480);          // more than 10 ms of spread

        pass ("testSineOnTimingRangeChangesOffsets");
    }

    // Opposite amounts move the timing in opposite directions around the unmodulated value.
    void testBipolarAmountInvertsModulation()
    {
        auto offsetsFor = [] (float amount)
        {
            Processor processor;
            setTimingPatch (processor, 25.0f);
            setLfo (processor, 0, 1.0f, 0.0f, false, 4.0f);
            setRoute (processor, 0, 0, 2, amount);
            processor.prepareToPlay (sampleRate, blockSize);

            int64_t sample = 0;
            return timingOffsets (run (processor, sample, 500));
        };

        const auto plus = offsetsFor (100.0f);
        const auto minus = offsetsFor (-100.0f);
        const auto none = offsetsFor (0.0f);

        assert (plus.size() == none.size() && minus.size() == none.size());

        bool moved = false;
        for (size_t k = 0; k < none.size(); ++k)
        {
            // Timing Range is linear, so the two offsets from the base are equal and opposite.
            const int64_t sum = (plus[k] - none[k]) + (minus[k] - none[k]);
            assert (std::abs (sum) <= 2);
            moved = moved || std::abs (plus[k] - none[k]) > 10;
        }

        assert (moved);
        pass ("testBipolarAmountInvertsModulation");
    }

    // A synced LFO on one bar repeats every four beats, and follows the host position.
    void testSyncedLfoFollowsBeats()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::syncId, 1.0f);
        setLfo (processor, 0, 0.25f, 0.0f, true, 2.0f);             // sine, 1 bar
        processor.prepareToPlay (sampleRate, blockSize);

        FakePlayHead playhead;
        playhead.bpm = 120.0;
        playhead.playing = true;
        processor.setPlayHead (&playhead);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;

        auto valueAt = [&] (double ppq)
        {
            playhead.ppq = ppq;
            audio.clear();
            midi.clear();
            processor.processBlock (audio, midi);
            return processor.getLfoValue (0);
        };

        assert (std::abs (valueAt (0.0) - valueAt (4.0)) < 1e-4f);   // one full cycle apart
        assert (std::abs (valueAt (3.0) + 1.0f) < 1e-3f);            // three quarters: -1
        assert (std::abs (valueAt (1.0) - 1.0f) < 1e-3f);            // one quarter: +1

        processor.setPlayHead (nullptr);
        pass ("testSyncedLfoFollowsBeats");
    }

    // A free LFO advances with the sample clock, at its rate in Hz.
    void testFreeLfoRunsInHertz()
    {
        Processor processor;
        setLfo (processor, 0, 1.0f, 0.0f, false, 4.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;

        const int blocks = 70;
        for (int block = 0; block < blocks; ++block)
        {
            audio.clear();
            midi.clear();
            processor.processBlock (audio, midi);
        }

        // The published values describe the start of the last block.
        const double seconds = (blocks - 1) * (double) blockSize / sampleRate;
        const double phase = seconds - std::floor (seconds);
        assert (std::abs (processor.getLfoPhase (0) - (float) phase) < 1e-3f);
        assert (std::abs (processor.getLfoValue (0) - (float) std::sin (2.0 * pi * phase)) < 1e-3f);

        pass ("testFreeLfoRunsInHertz");
    }

    // Sample and hold keeps one value for each cycle, and the value changes between cycles.
    void testSampleAndHoldHoldsPerCycle()
    {
        Processor processor;
        setLfo (processor, 0, 1.0f, 4.0f, false, 4.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;
        std::map<int, float> heldPerCycle;
        int changes = 0;

        for (int block = 0; block < 750; ++block)                  // 8 s at 1 Hz
        {
            audio.clear();
            midi.clear();
            processor.processBlock (audio, midi);

            const double seconds = block * (double) blockSize / sampleRate;
            const int cycle = (int) std::floor (seconds);
            const float value = processor.getLfoValue (0);

            assert (value >= -1.0f && value <= 1.0f);

            const auto found = heldPerCycle.find (cycle);
            if (found == heldPerCycle.end())
            {
                if (cycle > 0 && value != heldPerCycle[cycle - 1])
                    ++changes;
                heldPerCycle[cycle] = value;
            }
            else
            {
                assert (found->second == value);            // constant within the cycle
            }
        }

        assert (heldPerCycle.size() >= 8);
        assert (changes >= 4);
        pass ("testSampleAndHoldHoldsPerCycle");
    }

    // Every target keeps its value in range, and the Velocity target always yields whole velocities from 1 to 127.
    void testTargetsStayInRange()
    {
        const int targets[] = { 1, 4, 8, 9, 10, 11, 13, 14 };      // Chaos, Lerp, Density, Gate, Swing, Velocity, Tempo, Seed

        for (const int target : targets)
        {
            Processor processor;
            setLfo (processor, 0, 1.0f, 0.0f, false, 4.0f);
            setRoute (processor, 0, 0, target, 100.0f);
            processor.prepareToPlay (sampleRate, blockSize);

            int64_t sample = 0;
            float largest = 0.0f;

            for (int block = 0; block < 600; ++block)
            {
                for (const auto& event : run (processor, sample, 1))
                    if (event.message.isNoteOn())
                        assert (event.message.getVelocity() >= 1 && event.message.getVelocity() <= 127);

                largest = std::max (largest, std::abs (processor.getTargetOffset ((Processor::ModTarget) target)));
            }

            assert (largest > 0.5f);                                // the target really moved
            assert (largest <= 1.0f + 1e-6f);                       // and the offset stays within its range
        }

        pass ("testTargetsStayInRange");
    }

    // The routing and the LFO settings are saved with the session.
    void testModulationStateRoundTrip()
    {
        Processor original;
        setRoute (original, 1, 1, 9, -37.0f);                      // slot 2: LFO 2 to Gate, -37 %
        setLfo (original, 1, 3.5f, 3.0f, false, 6.0f);             // LFO 2: square, 3.5 Hz

        juce::MemoryBlock state;
        original.getStateInformation (state);

        Processor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());

        assert (getParameter (restored.apvts, Processor::modSourceIds[1]) == 1.0f);
        assert (getParameter (restored.apvts, Processor::modTargetIds[1]) == 9.0f);
        assert (std::abs (getParameter (restored.apvts, Processor::modAmountIds[1]) + 37.0f) < 1e-4f);
        assert (getParameter (restored.apvts, Processor::lfo2ShapeId) == 3.0f);
        assert (std::abs (getParameter (restored.apvts, Processor::lfo2RateId) - 3.5f) < 1e-3f);
        assert (getParameter (restored.apvts, Processor::lfo2DivisionId) == 6.0f);

        pass ("testModulationStateRoundTrip");
    }

    // The reported latency is fixed, whatever the routing does.
    void testLatencyIgnoresModulation()
    {
        Processor processor;
        setLfo (processor, 0, 1.0f, 0.0f, false, 4.0f);
        setRoute (processor, 0, 0, 2, 100.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        assert (processor.getLatencySamples() == 2400);
        pass ("testLatencyIgnoresModulation");
    }

    // The editor paints with the modulation controls in use.
    void testEditorWithModulation()
    {
        Processor processor;
        setLfo (processor, 0, 2.0f, 1.0f, false, 4.0f);
        setLfo (processor, 1, 0.5f, 4.0f, true, 2.0f);
        setRoute (processor, 0, 0, 2, 60.0f);
        setRoute (processor, 2, 1, 13, -40.0f);
        processor.prepareToPlay (sampleRate, blockSize);
        checkEditor (processor);

        pass ("testEditorWithModulation");
    }

    // The value a control shows follows its routing in real time, and stays at the base value when nothing routes to it.
    void testDisplayFollowsModulation()
    {
        auto spread = [] (float amount)
        {
            Processor processor;
            setTimingPatch (processor, 40.0f);
            setLfo (processor, 0, 1.0f, 0.0f, false, 4.0f);
            setRoute (processor, 0, 0, 2, amount);
            processor.prepareToPlay (sampleRate, blockSize);

            int64_t sample = 0;
            float lowest = 1.0e9f, highest = -1.0e9f;

            for (int block = 0; block < 200; ++block)
            {
                run (processor, sample, 1);
                const float shown = processor.getModulatedValue (Processor::ModTarget::TimingRange, Processor::timingRangeId);
                assert (shown >= 0.0f && shown <= 50.0f);
                lowest = std::min (lowest, shown);
                highest = std::max (highest, shown);
            }

            assert (getParameter (processor.apvts, Processor::timingRangeId) == 40.0f);   // the base value is untouched
            return highest - lowest;
        };

        assert (spread (100.0f) > 10.0f);   // the display moves with the LFO
        assert (spread (0.0f) < 1e-4f);     // an idle route leaves the display at the base value

        pass ("testDisplayFollowsModulation");
    }

    void testEditor()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        checkEditor (processor);

        checkEditor (processor);

        checkEditor (processor);

        checkEditor (processor);

        // The window shows at two thirds of the design size, and the layout keeps its design size.
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor->getWidth() == 1695 && editor->getHeight() == 1380);
        assert (std::abs (editor->getTransform().mat00 - 2.0f / 3.0f) < 1e-4f);
        processor.editorBeingDeleted (editor.get());

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
    testNoModulationIsIdentical();
    testLfoOffset();
    testSineOnTimingRangeChangesOffsets();
    testBipolarAmountInvertsModulation();
    testSyncedLfoFollowsBeats();
    testFreeLfoRunsInHertz();
    testSampleAndHoldHoldsPerCycle();
    testTargetsStayInRange();
    testModulationStateRoundTrip();
    testLatencyIgnoresModulation();
    testEditorWithModulation();
    testDisplayFollowsModulation();
    testEditor();

    std::puts ("All Micro-Timing Chaos plugin tests passed.");
    return 0;
}
