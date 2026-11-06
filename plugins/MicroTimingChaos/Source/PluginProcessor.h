#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "CoreDSP.h"
#include "SuiteModulationRouter.h"

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


    // Lerp Curve
    static constexpr const char* lerpId = "lerp";
    static constexpr const char* lerpXMultId = "lerpXMult";
    static constexpr const char* lerpYMultId = "lerpYMult";
    static constexpr const char* lerpZMultId = "lerpZMult";

    // Transport
    static constexpr const char* bpmId = "bpm";
    static constexpr const char* syncId = "sync";
    static constexpr const char* speedId = "speed";

    // LFO 1 and LFO 2
    static constexpr const char* lfo1RateId = "lfo1Rate";
    static constexpr const char* lfo1ShapeId = "lfo1Shape";
    static constexpr const char* lfo1SyncId = "lfo1Sync";
    static constexpr const char* lfo1DivisionId = "lfo1Division";
    static constexpr const char* lfo2RateId = "lfo2Rate";
    static constexpr const char* lfo2ShapeId = "lfo2Shape";
    static constexpr const char* lfo2SyncId = "lfo2Sync";
    static constexpr const char* lfo2DivisionId = "lfo2Division";

    // Modulation routing: four slots, each with a source, a target and a signed amount
    static constexpr const char* modSourceIds[6] = { "mod1Source", "mod2Source", "mod3Source", "mod4Source", "mod5Source", "mod6Source" };
    static constexpr const char* modTargetIds[6] = { "mod1Target", "mod2Target", "mod3Target", "mod4Target", "mod5Target", "mod6Target" };
    static constexpr const char* modAmountIds[6] = { "mod1Amount", "mod2Amount", "mod3Amount", "mod4Amount", "mod5Amount", "mod6Amount" };

    static constexpr int numControls = 46;
    static constexpr int lfoCount = 2;
    static constexpr int slotCount = 6;

    /** Fixed seeds for the sample and hold shape, one per LFO, so the random sequence is the same every time. */
    static constexpr std::array<std::uint32_t, lfoCount> lfoSeeds { 0x51A5u, 0x7B3Du };

    /**
        The parameters that an LFO can move. The order is part of the saved session: append new
        targets at the end, and never reorder or remove existing ones. Names come from modTargetNames().
    */
    enum class ModTarget
    {
        None = 0,
        Chaos,
        TimingRange,
        VelocityChaos,
        Lerp,
        LerpX,
        LerpY,
        LerpZ,
        Density,
        Gate,
        Swing,
        Velocity,
        Speed,
        Tempo,
        Seed,
        count
    };

    static juce::StringArray modTargetNames();

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

    /** The LFOs and routing, for the editor's panel and control display. */
    SuiteModulation::Router& getRouter() noexcept { return modulation; }

    //==============================================================================
    // Lerp curve: five points that map Chaos (x, 0 to 1) to drift (y, 0 to 1).
    // The first and last points keep x at 0 and 1. The points are saved with the session state.

    static constexpr int curvePointCount = 5;

    /** The natural default curve: logarithmic, with a gentle slope. */
    static float naturalCurveValue (float t) noexcept;
    static juce::Point<float> naturalCurvePoint (int index) noexcept;

    std::array<juce::Point<float>, curvePointCount> getLerpCurve() const;

    /** Moves one point (UI thread). X stays between the neighbouring points, and y is kept in [0, 1]. */
    void setLerpCurvePoint (int index, juce::Point<float> point);

    /** Restores the natural curve (UI thread). */
    void resetLerpCurve();

    /** The curve value for a chaos amount in [0, 1]. Safe to call on the audio thread. */
    float evaluateLerpCurve (float chaos) const noexcept;

    /** Per-axis drift amounts (0 to 1) of the most recent step, after the curve, multipliers and Lerp. */
    struct DriftAmounts
    {
        float timing = 0.0f;
        float pitch = 0.0f;
        float velocity = 0.0f;
    };

    DriftAmounts getDriftAmounts() const noexcept;

    /** The host tempo, if the host reports one. */
    juce::Optional<double> getHostBpm() const;

    /** The tempo in use: the host tempo when Host Sync is on and the host reports one, otherwise the Tempo parameter. */
    double getDisplayBpm() const;

    /** The LFO output (-1 to 1) at the most recent block start. */
    float getLfoValue (int lfoIndex) const noexcept;

    /** The LFO position within its cycle (0 to 1) at the most recent block start. */
    float getLfoPhase (int lfoIndex) const noexcept;

    /** The offset (normalised, -1 to 1) that the routing currently adds to a target. */
    float getTargetOffset (ModTarget target) const noexcept;

    /** The value a control shows right now: its base value, moved by the offset currently published for its target.
        Tempo is shown unmodulated while Host Sync is on, as the sequencer ignores it then. */
    float getModulatedValue (ModTarget target, const char* parameterId) const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** Applies an offset to a parameter's base value, in the parameter's own normalised range. Exact when the offset is zero. */
    float modulatedValue (ModTarget target, float base, const SuiteModulation::Router::Offsets& offsets) const
    {
        return modulation.modulate ((int) target, base, offsets);
    }

    void loadLerpCurveFromState();
    void storeLerpCurveToState();

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

    double currentBpm (const juce::Optional<juce::AudioPlayHead::PositionInfo>& position, float freeTempo) const;
    void advanceAttractorTo (double beat, double speed);
    void scheduleStep (std::int64_t stepIndex, double stepBeat, double blockStartBeat, double samplesPerBeat);
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
    std::atomic<float>* lerp = nullptr;
    std::atomic<float>* lerpXMult = nullptr;
    std::atomic<float>* lerpYMult = nullptr;
    std::atomic<float>* lerpZMult = nullptr;
    std::atomic<float>* bpm = nullptr;
    std::atomic<float>* sync = nullptr;
    std::atomic<float>* speed = nullptr;

    // The two LFOs and the six routing slots, shared with the other plugins (modules/suite_modulation).
    // Declared after apvts, which it reads from.
    SuiteModulation::Router modulation;

    TrailSnapshot trail;
    std::array<LogSlot, logLength> log;
    std::atomic<int> logHead { 0 };
    std::atomic<int> playheadStep { -1 };
    std::atomic<int> patternLength { 8 };

    std::array<std::atomic<float>, curvePointCount> curveX {};
    std::array<std::atomic<float>, curvePointCount> curveY {};
    std::atomic<float> driftTiming { 0.0f };
    std::atomic<float> driftPitch { 0.0f };
    std::atomic<float> driftVelocity { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicroTimingChaosProcessor)
};