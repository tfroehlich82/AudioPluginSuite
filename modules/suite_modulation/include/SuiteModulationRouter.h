#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace SuiteModulation
{
    /** One control the routing can move. */
    struct TargetInfo
    {
        const char* parameterId;                 // the plugin's own parameter, e.g. "chaos"
        const char* name;                        // shown in the Target combo box, e.g. "Chaos"
        bool wholeNumber = false;                // round the modulated result to an integer (e.g. Velocity, Seed)
        bool ignoredWhileHostSync = false;       // e.g. Tempo: only moves while the host does not sync
    };

    /**
        Two LFOs and six routing slots, shared by the plugins that use them.

        Choice indices: a target choice of 0 means None, and choice n (n >= 1) is targets[n - 1].
        The parameter IDs are fixed (lfo1Rate ... mod6Amount), so the same names save and restore the
        same way in every plugin that adds them. The routing math matches the Micro-Timing Chaos
        plugin's own copy, so moving that plugin over does not change its output.
    */
    class Router
    {
    public:
        static constexpr int lfoCount = 2;
        static constexpr int slotCount = 6;
        static constexpr int maxTargets = 32;  // compile-time bound on the number of target choices

        /** Adds the LFO and routing parameters. targetNames[0] must be "None", then one name per target. */
        static void addParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
                                  const juce::StringArray& targetNames);

        /** Seeds for the sample and hold and random shapes, one per LFO. */
        static constexpr std::array<std::uint32_t, lfoCount> lfoSeeds { 0x51A5u, 0x7B3Du };

        /** The targets must outlive the router. hostSyncId names the plugin's host sync switch, or nullptr. */
        Router(juce::AudioProcessorValueTreeState& apvts, const TargetInfo* targets, int numTargets,
               const char* hostSyncId = nullptr);

        /** Number of targets, not counting None. */
        int getNumTargets() const noexcept;

        /** The state the parameters live in. UI code attaches controls through this. */
        juce::AudioProcessorValueTreeState& getState() const noexcept { return apvts; }

        /** Stores the sample rate and resets the free LFOs. Call from prepareToPlay. */
        void prepare(double sampleRate);

        /** Offsets (normalised, summed per target choice) for a sample time and beat. Audio thread. */
        struct Offsets
        {
            std::array<float, maxTargets> value{};
        };

        /**
            The offsets for one sample time. Free LFOs count from blockStartSample, synced LFOs use the beat.
            Audio thread.
        */
        Offsets computeOffsets(double timeSamples, double beat, double blockStartSample) const;

        /** The base value moved by the offset of target choice targetIndex. Exact base when the offset is zero. */
        float modulate(int targetIndex, float base, const Offsets& offsets) const;

        /** Advances the free LFOs by one block. Call once per block, after the steps are scheduled. */
        void advanceBlock(int numSamples);

        /**
            Publishes the offsets and the LFO positions for the UI. Call once per block, before advanceBlock,
            with the block start values.
        */
        void publish(const Offsets& offsets, double blockStartSample, double beatAtBlockStart);

        /** UI thread: what a control shows right now. The base value moved by the published offset. */
        float getModulatedValue(int targetIndex) const;

        /** UI thread: the published offset (normalised) for a target choice. */
        float getTargetOffset(int targetIndex) const noexcept;

        /** UI thread: the published LFO output, -1 to 1. */
        float getLfoValue(int lfo) const noexcept;

        /** UI thread: the published LFO position within its cycle, 0 to 1. */
        float getLfoPhase(int lfo) const noexcept;

        /** The shape choice index of an LFO, read from the state. */
        int getLfoShape(int lfo) const noexcept;

        /** The routing parameter for a target choice (nullptr for None). */
        juce::RangedAudioParameter* targetParameter(int targetIndex) const noexcept;

    private:
        juce::AudioProcessorValueTreeState& apvts;
        const TargetInfo* targets;
        int targetCount;
        const char* hostSyncId;

        std::array<juce::RangedAudioParameter*, maxTargets> targetParams {};
        std::array<std::atomic<float>*, maxTargets> targetRaw {};  // the base values, read by the UI

        // Read-only raw values, looked up once in the constructor
        std::array<std::atomic<float>*, lfoCount> lfoRate {}, lfoShape {}, lfoSync {}, lfoDivision {};
        std::array<std::atomic<float>*, slotCount> modSource {}, modTarget {}, modAmount {};
        std::atomic<float>* hostSync = nullptr;

        double sampleRate = 44100.0;
        std::array<double, lfoCount> lfoCycles {};  // free LFOs: cycles counted at the start of the block

        double lfoCyclesAt(int lfo, double timeSamples, double beat, double blockStartSample) const;

        std::array<std::atomic<float>, maxTargets> publishedOffset {};
        std::array<std::atomic<float>, lfoCount> publishedLfoValue {};
        std::array<std::atomic<float>, lfoCount> publishedLfoPhase {};
    };
}  // namespace SuiteModulation
