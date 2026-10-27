#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <map>
#include <vector>

#include "CoreDSP.h"

/**
    Micro-Timing Chaos Sequencer.

    A MIDI effect. Note-ons are shifted in time and velocity by a Lorenz attractor:
    X sets the timing offset and Z sets the velocity drift. Events are queued and released at their
    shifted time, so early notes are delayed by a fixed latency that the plugin reports to the host.
*/
class MicroTimingSeqProcessor : public juce::AudioProcessor
{
public:
    static constexpr const char* chaosIntensityId = "chaosIntensity";
    static constexpr const char* timingOffsetId = "timingOffsetMax";
    static constexpr const char* velocityDriftId = "velocityDrift";
    static constexpr const char* bpmSyncId = "bpmSync";

    /** Timing offsets are at most this large, and the plugin reports this much latency to the host. */
    static constexpr double maxTimingOffsetMs = 50.0;

    static constexpr int trailLength = 512;

    MicroTimingSeqProcessor();
    ~MicroTimingSeqProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Micro-Timing Chaos Sequencer"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    /** Oldest-to-newest (x, y) attractor points in [-1, 1], for the phase-space plot. */
    std::vector<juce::Point<float>> getTrail() const;

    /** Timing offset applied to the most recent note-on, in milliseconds. */
    float getLastTimingOffsetMs() const noexcept { return lastOffsetMs.load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    int maxOffsetSamples() const noexcept;
    void advanceChaos (int numSamples);
    void publishTrailPoint() noexcept;

    CoreDSP::Chaos::LorenzAttractor chaos;
    juce::Optional<double> lastPpq;

    double sampleRate = 44100.0;
    int64_t streamPosition = 0;

    // Note-ons are queued by their release time. Note-offs never leave before their note-on.
    std::multimap<int64_t, juce::MidiMessage> pending;
    std::array<std::array<int64_t, 128>, 16> lastNoteOnTarget {};

    std::atomic<float>* chaosIntensity = nullptr;
    std::atomic<float>* timingOffset = nullptr;
    std::atomic<float>* velocityDrift = nullptr;
    std::atomic<float>* bpmSync = nullptr;

    struct TrailSnapshot
    {
        std::array<std::atomic<float>, trailLength> x {};
        std::array<std::atomic<float>, trailLength> y {};
        std::atomic<int> head { 0 };
    };

    TrailSnapshot trail;
    std::atomic<float> lastOffsetMs { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicroTimingSeqProcessor)
};
