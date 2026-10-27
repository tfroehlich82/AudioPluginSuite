#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    /** Lorenz time per second when the sequencer runs freely. */
    constexpr double freeRunUnitsPerSecond = 2.0;

    /** Lorenz time per quarter note when the sequencer follows the host tempo. */
    constexpr double syncUnitsPerBeat = 0.5;

    /** Playhead jumps larger than this (seeks, loops) do not advance the attractor. */
    constexpr double maxBeatsPerAdvance = 4.0;

    constexpr int midiChannels = 16;
}

MicroTimingSeqProcessor::MicroTimingSeqProcessor()
    : AudioProcessor (BusesProperties()),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    chaosIntensity = apvts.getRawParameterValue (chaosIntensityId);
    timingOffset = apvts.getRawParameterValue (timingOffsetId);
    velocityDrift = apvts.getRawParameterValue (velocityDriftId);
    bpmSync = apvts.getRawParameterValue (bpmSyncId);
}

juce::AudioProcessorValueTreeState::ParameterLayout MicroTimingSeqProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { chaosIntensityId, 1 }, "Chaos Intensity", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { timingOffsetId, 1 }, "Timing Offset Max", juce::NormalisableRange<float> (0.0f, (float) maxTimingOffsetMs, 0.01f), 15.0f, "ms"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { velocityDriftId, 1 }, "Velocity Drift", juce::NormalisableRange<float> (0.0f, 30.0f, 0.1f), 10.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { bpmSyncId, 1 }, "BPM Sync", juce::StringArray { "Free", "Host Sync" }, 0));

    return { params.begin(), params.end() };
}

int MicroTimingSeqProcessor::maxOffsetSamples() const noexcept
{
    return juce::roundToInt (maxTimingOffsetMs * sampleRate / 1000.0);
}

void MicroTimingSeqProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    streamPosition = 0;
    pending.clear();
    lastPpq.reset();
    chaos.reset();

    for (auto& channel : lastNoteOnTarget)
        channel.fill (0);

    setLatencySamples (maxOffsetSamples());
}

bool MicroTimingSeqProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // MIDI only: the plugin has no audio buses.
    return layouts.getMainInputChannelSet().isDisabled() && layouts.getMainOutputChannelSet().isDisabled();
}

void MicroTimingSeqProcessor::advanceChaos (int numSamples)
{
    const bool followHost = bpmSync->load() > 0.5f;

    if (followHost)
    {
        auto* playHead = getPlayHead();
        const auto position = playHead != nullptr ? playHead->getPosition()
                                                  : juce::Optional<juce::AudioPlayHead::PositionInfo>();

        if (position.hasValue())
        {
            // Stopped transport: the attractor holds its place.
            if (! position->getIsPlaying())
            {
                lastPpq.reset();
                return;
            }

            const auto ppq = position->getPpqPosition();

            if (ppq.hasValue())
            {
                if (lastPpq.hasValue())
                {
                    const double beats = *ppq - *lastPpq;

                    if (beats > 0.0 && beats <= maxBeatsPerAdvance)
                        chaos.advance (beats * syncUnitsPerBeat);
                }

                lastPpq = *ppq;
                return;
            }
        }
    }

    // Free running, also used as the fallback when the host gives no position.
    lastPpq.reset();
    chaos.advance (numSamples / sampleRate * freeRunUnitsPerSecond);
}

void MicroTimingSeqProcessor::publishTrailPoint() noexcept
{
    const int slot = trail.head.load (std::memory_order_relaxed);

    trail.x[(size_t) slot].store (chaos.normalisedX(), std::memory_order_relaxed);
    trail.y[(size_t) slot].store (chaos.normalisedY(), std::memory_order_relaxed);
    trail.head.store ((slot + 1) % trailLength, std::memory_order_release);
}

std::vector<juce::Point<float>> MicroTimingSeqProcessor::getTrail() const
{
    std::vector<juce::Point<float>> points;
    points.reserve (trailLength);

    const int head = trail.head.load (std::memory_order_acquire);

    for (int i = 0; i < trailLength; ++i)
    {
        const size_t slot = (size_t) ((head + i) % trailLength);
        points.emplace_back (trail.x[slot].load (std::memory_order_relaxed),
                             trail.y[slot].load (std::memory_order_relaxed));
    }

    return points;
}

void MicroTimingSeqProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // A MIDI effect has no audio channels, but the buffer still carries the block length.
    const int numSamples = buffer.getNumSamples();

    advanceChaos (numSamples);

    const double intensity = chaosIntensity->load() / 100.0;
    const double maxOffsetMs = timingOffset->load();
    const double drift = velocityDrift->load() / 100.0;

    const float x = chaos.normalisedX();
    const float z = chaos.normalisedZ();
    const int64_t latency = maxOffsetSamples();

    for (const auto metadata : midi)
    {
        const auto& source = metadata.getMessage();
        const int64_t eventTime = streamPosition + metadata.samplePosition;
        int64_t target = eventTime + latency;
        juce::MidiMessage message = source;

        if (source.isNoteOn())
        {
            const double offsetMs = intensity * (double) x * maxOffsetMs;
            const auto offsetSamples = (int64_t) juce::roundToInt (offsetMs * sampleRate / 1000.0);

            // The offset never exceeds the reported latency, so the target cannot fall before the event.
            target = juce::jmax (eventTime, target + offsetSamples);

            const double scaled = juce::jlimit (1.0, 127.0, source.getVelocity() * (1.0 + drift * (double) z));
            message = juce::MidiMessage::noteOn (source.getChannel(), source.getNoteNumber(),
                                                 (juce::uint8) juce::roundToInt (scaled));

            const int channelIndex = juce::jlimit (0, midiChannels - 1, source.getChannel() - 1);
            lastNoteOnTarget[(size_t) channelIndex][(size_t) source.getNoteNumber()] = target;
            lastOffsetMs.store ((float) offsetMs, std::memory_order_relaxed);
        }
        else if (source.isNoteOff())
        {
            const int channelIndex = juce::jlimit (0, midiChannels - 1, source.getChannel() - 1);
            const int64_t noteOnTarget = lastNoteOnTarget[(size_t) channelIndex][(size_t) source.getNoteNumber()];
            target = juce::jmax (target, noteOnTarget);
        }

        pending.emplace (target, message);
    }

    midi.clear();

    // Release everything whose time has come, relative to the start of this block.
    const int64_t blockEnd = streamPosition + numSamples;

    for (auto it = pending.begin(); it != pending.end() && it->first < blockEnd;)
    {
        const auto position = (int) juce::jmax<int64_t> (0, it->first - streamPosition);
        midi.addEvent (it->second, position);
        it = pending.erase (it);
    }

    streamPosition = blockEnd;
    publishTrailPoint();
}

juce::AudioProcessorEditor* MicroTimingSeqProcessor::createEditor()
{
    return new MicroTimingSeqEditor (*this);
}

void MicroTimingSeqProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MicroTimingSeqProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MicroTimingSeqProcessor();
}
