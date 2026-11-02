#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr int midiChannel = 1;
}

MicroTimingChaosProcessor::MicroTimingChaosProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    rate = apvts.getRawParameterValue (rateId);
    length = apvts.getRawParameterValue (lengthId);
    density = apvts.getRawParameterValue (densityId);
    seed = apvts.getRawParameterValue (seedId);
    gate = apvts.getRawParameterValue (gateId);
    swing = apvts.getRawParameterValue (swingId);
    baseVelocity = apvts.getRawParameterValue (baseVelocityId);
    root = apvts.getRawParameterValue (rootId);
    scale = apvts.getRawParameterValue (scaleId);
    octaves = apvts.getRawParameterValue (octavesId);
    chaos = apvts.getRawParameterValue (chaosId);
    timingRange = apvts.getRawParameterValue (timingRangeId);
    velocityChaos = apvts.getRawParameterValue (velocityChaosId);
    speed = apvts.getRawParameterValue (speedId);
    bpm = apvts.getRawParameterValue (bpmId);
    sync = apvts.getRawParameterValue (syncId);
}

juce::AudioProcessorValueTreeState::ParameterLayout MicroTimingChaosProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Pattern
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { rateId, 1 }, "Rate", StringArray { "1/4", "1/8", "1/16", "1/32" }, 2));
    params.push_back (std::make_unique<AudioParameterInt> (ParameterID { lengthId, 1 }, "Pattern Length", 2, 16, 8));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { densityId, 1 }, "Density", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 70.0f, "%"));
    params.push_back (std::make_unique<AudioParameterInt> (ParameterID { seedId, 1 }, "Seed", 0, 999, 42));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { gateId, 1 }, "Gate", NormalisableRange<float> (10.0f, 100.0f, 0.1f), 60.0f, "%"));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { swingId, 1 }, "Swing", NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, "%"));
    params.push_back (std::make_unique<AudioParameterInt> (ParameterID { baseVelocityId, 1 }, "Velocity", 30, 127, 100));

    // Melody
    params.push_back (std::make_unique<AudioParameterInt> (ParameterID { rootId, 1 }, "Root Note", 24, 72, 48));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { scaleId, 1 }, "Scale",
        StringArray { "Chromatic", "Major", "Minor", "Minor Pentatonic", "Dorian", "Whole Tone" }, 2));
    params.push_back (std::make_unique<AudioParameterInt> (ParameterID { octavesId, 1 }, "Octaves", 1, 3, 2));

    // Chaos
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { chaosId, 1 }, "Chaos", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { timingRangeId, 1 }, "Timing Range", NormalisableRange<float> (0.0f, (float) maxTimingRangeMs, 0.01f), 20.0f, "ms"));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { velocityChaosId, 1 }, "Velocity Chaos", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, "%"));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { speedId, 1 }, "Chaos Speed", NormalisableRange<float> (0.0f, 0.5f, 0.001f, 0.5f), 0.5f, "/beat"));

    // Transport
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { bpmId, 1 }, "Tempo", NormalisableRange<float> (40.0f, 240.0f, 0.01f), 120.0f, "BPM"));
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { syncId, 1 }, "Host Sync", false));

    return { params.begin(), params.end() };
}

void MicroTimingChaosProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    streamSample = 0;
    beatPosition = 0.0;
    nextStep = 0;
    resyncNeeded = true;
    wasRunning = false;

    pending.clear();
    pendingOffs.clear();
    lastNoteOn.clear();
    sounding.clear();

    attractor.reset();
    attractorBeat = 0.0;

    latencySamples = (std::int64_t) std::llround (maxTimingRangeMs * sampleRate / 1000.0);
    setLatencySamples ((int) latencySamples);
}

bool MicroTimingChaosProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    const auto& input = layouts.getMainInputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}

double MicroTimingChaosProcessor::currentBpm (const juce::Optional<juce::AudioPlayHead::PositionInfo>& position) const
{
    if (sync->load() > 0.5f && position.hasValue())
    {
        const auto hostBpm = position->getBpm();

        if (hostBpm.hasValue() && *hostBpm > 1.0)
            return *hostBpm;
    }

    return (double) bpm->load();
}

void MicroTimingChaosProcessor::advanceAttractorTo (double beat, double speedValue)
{
    const double delta = beat - attractorBeat;

    if (delta > 0.0)
    {
        double time = delta * speedValue;

        // Advance in chunks, because the attractor caps a single call to keep a host jump from stalling the thread.
        while (time > 0.0)
        {
            const double chunk = std::min (time, CoreDSP::Chaos::LorenzAttractor::maxAdvance);
            attractor.advance (chunk);
            time -= chunk;
        }
    }

    attractorBeat = beat;
}

void MicroTimingChaosProcessor::publishTrailPoint() noexcept
{
    const int slot = trail.head.load (std::memory_order_relaxed);

    trail.x[(size_t) slot].store (attractor.normalisedX(), std::memory_order_relaxed);
    trail.y[(size_t) slot].store (attractor.normalisedY(), std::memory_order_relaxed);
    trail.head.store ((slot + 1) % trailLength, std::memory_order_release);
}

void MicroTimingChaosProcessor::publishStepEvent (const StepEvent& event) noexcept
{
    const int slot = logHead.load (std::memory_order_relaxed);
    auto& entry = log[(size_t) slot];

    entry.patternStep.store (event.patternStep, std::memory_order_relaxed);
    entry.offsetMs.store (event.offsetMs, std::memory_order_relaxed);
    entry.velocity.store (event.velocity, std::memory_order_relaxed);
    entry.note.store (event.note, std::memory_order_relaxed);

    logHead.store ((slot + 1) % logLength, std::memory_order_release);
}

std::vector<juce::Point<float>> MicroTimingChaosProcessor::getTrail() const
{
    std::vector<juce::Point<float>> points;
    points.reserve (trailLength);

    const int head = trail.head.load (std::memory_order_acquire);

    for (int i = 0; i < trailLength; ++i)
    {
        const auto slot = (size_t) ((head + i) % trailLength);
        points.emplace_back (trail.x[slot].load (std::memory_order_relaxed),
                             trail.y[slot].load (std::memory_order_relaxed));
    }

    return points;
}

std::vector<MicroTimingChaosProcessor::StepEvent> MicroTimingChaosProcessor::getRecentSteps() const
{
    std::vector<StepEvent> events;
    events.reserve (logLength);

    const int head = logHead.load (std::memory_order_acquire);

    for (int i = 0; i < logLength; ++i)
    {
        const auto& entry = log[(size_t) ((head + i) % logLength)];
        events.push_back ({ entry.patternStep.load (std::memory_order_relaxed),
                            entry.offsetMs.load (std::memory_order_relaxed),
                            entry.velocity.load (std::memory_order_relaxed),
                            entry.note.load (std::memory_order_relaxed) });
    }

    return events;
}

std::int64_t MicroTimingChaosProcessor::queueNoteOn (int note, int velocity, std::int64_t target)
{
    const auto found = pendingOffs.find (note);
    const auto earlier = lastNoteOn.find (note);

    // A note that is still queued to end must end before this onset. If its onset is later than the new one,
    // the new onset moves to just after it, so the pairs stay in order and nothing hangs.
    if (found != pendingOffs.end() && found->second->first >= target && earlier != lastNoteOn.end())
    {
        target = std::max (target, earlier->second + 1);
        const std::int64_t cutAt = std::max (target - 1, earlier->second);

        pending.erase (found->second);
        pendingOffs.erase (found);
        pendingOffs[note] = pending.emplace (cutAt, juce::MidiMessage::noteOff (midiChannel, note));
    }

    lastNoteOn[note] = target;
    pending.emplace (target, juce::MidiMessage::noteOn (midiChannel, note, (juce::uint8) velocity));
    return target;
}

void MicroTimingChaosProcessor::queueNoteOff (int note, std::int64_t target)
{
    pendingOffs[note] = pending.emplace (target, juce::MidiMessage::noteOff (midiChannel, note));
}

void MicroTimingChaosProcessor::scheduleStep (std::int64_t stepIndex, double stepBeat, double blockStartBeat,
                                              double samplesPerBeat, float chaosValue, float rangeMs, float speedValue)
{
    using namespace CoreDSP::Sequencer;

    const int lengthValue = patternLength.load (std::memory_order_relaxed);
    const int patternIndex = (int) (((stepIndex % lengthValue) + lengthValue) % lengthValue);
    const auto seedValue = (std::uint32_t) juce::roundToInt (seed->load());
    const float densityValue = density->load() / 100.0f;
    const double stepLength = stepBeats (juce::roundToInt (rate->load()));

    // The attractor evolves on the grid, whether or not the step fires, so chaos never pauses.
    advanceAttractorTo (stepBeat, speedValue);
    publishTrailPoint();

    const float x = attractor.normalisedX();
    const float y = attractor.normalisedY();
    const float z = attractor.normalisedZ();

    if (! stepFires (seedValue, patternIndex, densityValue))
        return;

    const int scaleValue = juce::roundToInt (scale->load());
    const int octavesValue = juce::roundToInt (octaves->load());
    const int rootValue = juce::roundToInt (root->load());
    const int count = degreeCount (scaleValue, octavesValue);

    // The fixed melody comes from the seed. Chaos blends it towards the attractor's melody.
    const int hashDegree = degreeFromUnit (2.0f * hashUnit (seedValue, 1000u + (std::uint32_t) patternIndex) - 1.0f,
                                           scaleValue, octavesValue);
    const int lorenzDegree = degreeFromUnit (y, scaleValue, octavesValue);
    const int degree = blendDegree (hashDegree, lorenzDegree, chaosValue, count);
    const int note = notePitch (rootValue, scaleValue, degree);

    const int velocity = velocityFor (juce::roundToInt (baseVelocity->load()), velocityChaos->load() / 100.0f, z);

    const double offsetMs = timingOffsetMs (chaosValue, rangeMs, x);
    const auto offsetSamples = (std::int64_t) std::llround (offsetMs * sampleRate / 1000.0);
    const double swingSamples = swingBeats (stepIndex, swing->load() / 100.0f, stepLength) * samplesPerBeat;

    const double gridSample = (double) streamSample + (stepBeat - blockStartBeat) * samplesPerBeat;
    const auto onTarget = std::max<std::int64_t> (streamSample,
                                                  (std::int64_t) std::llround (gridSample + swingSamples)
                                                      + latencySamples + offsetSamples);

    const double gateSamples = gateBeats (stepLength, gate->load() / 100.0f, chaosValue, y) * samplesPerBeat;
    const auto actualOn = queueNoteOn (note, velocity, onTarget);
    queueNoteOff (note, actualOn + std::max<std::int64_t> (1, (std::int64_t) std::llround (gateSamples)));

    publishStepEvent ({ patternIndex, (float) offsetMs, (float) velocity, note });
}

void MicroTimingChaosProcessor::stopAllNotes()
{
    // Drop everything still queued, then end every note that is sounding right now.
    pending.clear();
    pendingOffs.clear();
    lastNoteOn.clear();

    for (const int note : sounding)
        pending.emplace (streamSample, juce::MidiMessage::noteOff (midiChannel, note));
}

void MicroTimingChaosProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // Audio is passed through unchanged. The sequencer only writes MIDI.
    const int numSamples = buffer.getNumSamples();

    const auto* playHead = getPlayHead();
    const auto position = playHead != nullptr ? playHead->getPosition()
                                              : juce::Optional<juce::AudioPlayHead::PositionInfo>();
    const bool followHost = sync->load() > 0.5f && position.hasValue();
    const bool running = ! followHost || position->getIsPlaying();

    const int lengthValue = juce::roundToInt (length->load());
    patternLength.store (lengthValue, std::memory_order_relaxed);

    const int rateIndex = juce::roundToInt (rate->load());
    const double stepLength = CoreDSP::Sequencer::stepBeats (rateIndex);
    const double samplesPerBeat = 60.0 / currentBpm (position) * sampleRate;
    const float chaosValue = chaos->load() / 100.0f;
    const float rangeMs = timingRange->load();
    const float speedValue = speed->load();

    if (! running)
    {
        if (wasRunning)
            stopAllNotes();

        wasRunning = false;
        playheadStep.store (-1, std::memory_order_relaxed);
    }
    else
    {
        if (followHost)
        {
            // Follow the host's position. A seek or a restart shows up as a jump and resyncs the grid.
            const auto ppq = position->getPpqPosition();

            if (ppq.hasValue() && (! wasRunning || std::abs (*ppq - beatPosition) > 1.0e-3))
            {
                beatPosition = *ppq;
                resyncNeeded = true;
            }
        }

        wasRunning = true;

        const double blockStart = beatPosition;
        const double blockEnd = blockStart + numSamples / samplesPerBeat;

        const double expected = (double) nextStep * stepLength;
        if (resyncNeeded || expected > blockEnd + stepLength || expected < blockStart - stepLength)
        {
            nextStep = (std::int64_t) std::ceil (blockStart / stepLength - 1.0e-9);
            resyncNeeded = false;
        }

        while ((double) nextStep * stepLength < blockEnd)
        {
            const double stepBeat = (double) nextStep * stepLength;

            if (stepBeat >= blockStart - 1.0e-9)
                scheduleStep (nextStep, stepBeat, blockStart, samplesPerBeat, chaosValue, rangeMs, speedValue);

            ++nextStep;
        }

        const auto playStep = (std::int64_t) std::floor (blockStart / stepLength + 1.0e-9);
        playheadStep.store ((int) (((playStep % lengthValue) + lengthValue) % lengthValue), std::memory_order_relaxed);

        beatPosition = blockEnd;
    }

    // Incoming notes are passed through, delayed by the same latency as the generated ones.
    for (const auto metadata : midi)
        pending.emplace (streamSample + metadata.samplePosition + latencySamples, metadata.getMessage());

    midi.clear();

    // Release everything due in this block, and keep track of the notes that are sounding.
    const std::int64_t blockEndSample = streamSample + numSamples;

    for (auto it = pending.begin(); it != pending.end() && it->first < blockEndSample;)
    {
        const auto& message = it->second;
        const auto offset = (int) std::max<std::int64_t> (0, it->first - streamSample);

        if (message.isNoteOn())
        {
            sounding.insert (message.getNoteNumber());
        }
        else if (message.isNoteOff())
        {
            sounding.erase (message.getNoteNumber());

            const auto found = pendingOffs.find (message.getNoteNumber());
            if (found != pendingOffs.end() && found->second == it)
                pendingOffs.erase (found);
        }

        midi.addEvent (message, offset);
        it = pending.erase (it);
    }

    streamSample = blockEndSample;
}

juce::AudioProcessorEditor* MicroTimingChaosProcessor::createEditor()
{
    return new MicroTimingChaosEditor (*this);
}

void MicroTimingChaosProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MicroTimingChaosProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MicroTimingChaosProcessor();
}
