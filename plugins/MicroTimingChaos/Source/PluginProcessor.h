#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "CoreDSP.h"

/**
    Micro-Timing Chaos.

    A MIDI step sequencer. A fixed pattern decides which steps fire (set by Density and Seed), but
    a Lorenz attractor moves every step: X shifts its timing, Y chooses its pitch and gate length,
    and Z sets its velocity. The melody and timing repeat only when Chaos is 0.

    Incoming MIDI notes pass through with the same latency. Audio passes through unchanged.
*/
class MicroTimingChaosProcessor : public juce::AudioProcessor
{
public:
    // Pattern
    static constexpr const char* rateId = "rate";
    static constexpr const char* lengthId = "length";
    static constexpr const char* densityId = "density";
    static constexpr const char* seedId = "seed";
    static constexpr const char* gateId = "gate";
    static constexpr const char* swingId = "swing";
    static constexpr const char* baseVelocityId = "baseVelocity";

    // Melody
    static constexpr const char* rootId = "root";
    static constexpr const char* scaleId = "scale";
    static constexpr const char* octavesId = "octaves";

    // Chaos
    static constexpr const char* chaosId = "chaos";
    static constexpr const char* timingRangeId = "timingRange";
    static constexpr const char* velocityChaosId = "velocityChaos";
    static constexpr const char* speedId = "speed";

    // Transport
    static constexpr const char* bpmId = "bpm";
    static constexpr const char* syncId = "sync";

    static constexpr int numControls = 16;

    /** Timing range is at most this large, and the plugin reports this much latency. */
    static constexpr double maxTimingRangeMs = 50.0;

    static constexpr int trailLength = 256;
    static constexpr int logLength = 32;

    /** One scheduled step, as shown in the step grid. */
    struct StepEvent
    {
        int patternStep = 0;
        float offsetMs = 0.0f;
        float velocity = 0.0f;
        int note = 0;
    };

    MicroTimingChaosProcessor();
    ~MicroTimingChaosProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Micro-Timing Chaos"; }
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

    /** The most recent scheduled steps, oldest first. */
    std::vector<StepEvent> getRecentSteps() const;

    /** Pattern step at the playhead, or -1 while the transport is stopped. */
    int getPlayheadStep() const noexcept { return playheadStep.load (std::memory_order_relaxed); }

    /** Pattern length currently in use. */
    int getPatternLength() const noexcept { return patternLength.load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct TrailSnapshot
    {
        std::array<std::atomic<float>, trailLength> x {};
        std::array<std::atomic<float>, trailLength> y {};
        std::atomic<int> head { 0 };
    };

    struct LogSlot
    {
        std::atomic<int> patternStep { 0 };
        std::atomic<float> offsetMs { 0.0f };
        std::atomic<float> velocity { 0.0f };
        std::atomic<int> note { 0 };
    };

    using Queue = std::multimap<std::int64_t, juce::MidiMessage>;

    double currentBpm (const juce::Optional<juce::AudioPlayHead::PositionInfo>& position) const;
    void advanceAttractorTo (double beat, double speed);
    void scheduleStep (std::int64_t stepIndex, double stepBeat, double blockStartBeat, double samplesPerBeat,
                       float chaos, float timingRangeMs, float speed);
    std::int64_t queueNoteOn (int note, int velocity, std::int64_t target);
    void queueNoteOff (int note, std::int64_t target);
    void publishTrailPoint() noexcept;
    void publishStepEvent (const StepEvent& event) noexcept;
    void stopAllNotes();

    double sampleRate = 44100.0;
    std::int64_t streamSample = 0;
    std::int64_t latencySamples = 0;

    // Transport and grid
    double beatPosition = 0.0;
    std::int64_t nextStep = 0;
    bool resyncNeeded = true;
    bool wasRunning = false;

    // Attractor
    CoreDSP::Chaos::LorenzAttractor attractor;
    double attractorBeat = 0.0;

    // Event queue. Keys are absolute sample times. pendingOffs remembers each note's queued note-off,
    // so a retriggered note can cut off its earlier copy first and never hangs.
    Queue pending;
    std::map<int, Queue::iterator> pendingOffs;
    std::map<int, std::int64_t> lastNoteOn;
    std::set<int> sounding;

    std::atomic<float>* rate = nullptr;
    std::atomic<float>* length = nullptr;
    std::atomic<float>* density = nullptr;
    std::atomic<float>* seed = nullptr;
    std::atomic<float>* gate = nullptr;
    std::atomic<float>* swing = nullptr;
    std::atomic<float>* baseVelocity = nullptr;
    std::atomic<float>* root = nullptr;
    std::atomic<float>* scale = nullptr;
    std::atomic<float>* octaves = nullptr;
    std::atomic<float>* chaos = nullptr;
    std::atomic<float>* timingRange = nullptr;
    std::atomic<float>* velocityChaos = nullptr;
    std::atomic<float>* speed = nullptr;
    std::atomic<float>* bpm = nullptr;
    std::atomic<float>* sync = nullptr;

    TrailSnapshot trail;
    std::array<LogSlot, logLength> log;
    std::atomic<int> logHead { 0 };
    std::atomic<int> playheadStep { -1 };
    std::atomic<int> patternLength { 8 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicroTimingChaosProcessor)
};
