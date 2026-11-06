#undef NDEBUG
#include <cassert>

// Tests for the shared LFO and routing module. The router works on any processor, so these tests use a
// small stub processor with three routable parameters, a Host Sync switch, and the shared LFO and slot
// parameters. Nothing here depends on a plugin.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>

#include "SuiteModulationControls.h"
#include "SuiteModulationRouter.h"

namespace
{
    constexpr double sampleRate = 48000.0;

    // The routable targets, in table order. Choice index n is entry n - 1.
    const SuiteModulation::TargetInfo targets[] {
        { "tA", "A" },                              // choice 1: a plain float, 0 to 100, default 50
        { "tB", "B", true },                        // choice 2: a whole number, 0 to 10, default 5
        { "tC", "C", false, true },                 // choice 3: ignored while Host Sync is on
    };

    constexpr int numTargets = 3;

    // A minimal processor, so an APVTS can be built. It has no audio behaviour.
    struct StubProcessor : public juce::AudioProcessor
    {
        const juce::String getName() const override { return "Stub"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tA", 1 }, "A",
                                                                        juce::NormalisableRange<float> (0.0f, 100.0f), 50.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tB", 1 }, "B",
                                                                        juce::NormalisableRange<float> (0.0f, 10.0f), 5.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tC", 1 }, "C",
                                                                        juce::NormalisableRange<float> (0.0f, 100.0f), 20.0f));
        params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "sync", 1 }, "Host Sync", false));

        SuiteModulation::Router::addParameters (params, { "None", "A", "B", "C" });

        return { params.begin(), params.end() };
    }

    void set (juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
    {
        apvts.getParameter (id)->setValueNotifyingHost (apvts.getParameter (id)->convertTo0to1 (value));
    }

    // Sets one LFO. Shape is the choice index (0 sine ... 4 sample and hold ... 6 noise). Division 2 is 1 bar.
    void setLfo (juce::AudioProcessorValueTreeState& apvts, int lfo, float rateHz, float shape, bool synced, float division)
    {
        const auto n = juce::String (lfo + 1);
        set (apvts, ("lfo" + n + "Rate").toRawUTF8(), rateHz);
        set (apvts, ("lfo" + n + "Shape").toRawUTF8(), shape);
        set (apvts, ("lfo" + n + "Sync").toRawUTF8(), synced ? 1.0f : 0.0f);
        set (apvts, ("lfo" + n + "Division").toRawUTF8(), division);
    }

    // Routes slot 0 (zero based) to a target choice, from LFO source (0 or 1), at a signed amount.
    void setRoute (juce::AudioProcessorValueTreeState& apvts, int slot, int source, int target, float amount)
    {
        const auto n = juce::String (slot + 1);
        set (apvts, ("mod" + n + "Source").toRawUTF8(), (float) source);
        set (apvts, ("mod" + n + "Target").toRawUTF8(), (float) target);
        set (apvts, ("mod" + n + "Amount").toRawUTF8(), amount);
    }

    void pass (const char* name)
    {
        std::printf ("[PASS] %s\n", name);
    }

    //==============================================================================
    void testRoutedLfoMovesTheBaseAndAmountZeroDoesNot()
    {
        StubProcessor stub;
        juce::AudioProcessorValueTreeState apvts (stub, nullptr, "PARAMETERS", makeLayout());
        SuiteModulation::Router router (apvts, targets, numTargets, "sync");
        router.prepare (sampleRate);

        // A 1 Hz sine. At a quarter of a second it is at its peak, +1.
        setLfo (apvts, 0, 1.0f, 0.0f, false, 4.0f);
        setRoute (apvts, 0, 0, 1, 100.0f);

        const auto peak = router.computeOffsets (0.25 * sampleRate, 0.0, 0.0);
        const float moved = router.modulate (1, 50.0f, peak);
        assert (std::abs (moved - 100.0f) < 1e-4f);  // 50 + 100 % of the range, clamped to 100

        // The same routing at 0 % changes nothing: the offset is zero, so the base comes back exactly.
        setRoute (apvts, 0, 0, 1, 0.0f);
        const auto silent = router.computeOffsets (0.25 * sampleRate, 0.0, 0.0);
        assert (router.modulate (1, 50.0f, silent) == 50.0f);

        pass ("testRoutedLfoMovesTheBaseAndAmountZeroDoesNot");
    }

    void testZeroOffsetReturnsBaseBitForBit()
    {
        StubProcessor stub;
        juce::AudioProcessorValueTreeState apvts (stub, nullptr, "PARAMETERS", makeLayout());
        SuiteModulation::Router router (apvts, targets, numTargets, "sync");
        router.prepare (sampleRate);

        const SuiteModulation::Router::Offsets none {};
        const float base = 37.123f;

        for (int target = 0; target <= numTargets; ++target)
            assert (router.modulate (target, base, none) == base);

        pass ("testZeroOffsetReturnsBaseBitForBit");
    }

    void testBipolarAmountGivesEqualOpposingOffsets()
    {
        StubProcessor stub;
        juce::AudioProcessorValueTreeState apvts (stub, nullptr, "PARAMETERS", makeLayout());
        SuiteModulation::Router router (apvts, targets, numTargets, "sync");
        router.prepare (sampleRate);

        setLfo (apvts, 0, 1.0f, 0.0f, false, 4.0f);
        const double time = 0.1 * sampleRate;  // not at a peak, so the value is neither 0 nor 1

        setRoute (apvts, 0, 0, 1, 100.0f);
        const float plus = router.computeOffsets (time, 0.0, 0.0).value[1];

        setRoute (apvts, 0, 0, 1, -100.0f);
        const float minus = router.computeOffsets (time, 0.0, 0.0).value[1];

        assert (plus != 0.0f);
        assert (plus == -minus);

        pass ("testBipolarAmountGivesEqualOpposingOffsets");
    }

    void testWholeNumberTargetsReturnIntegers()
    {
        StubProcessor stub;
        juce::AudioProcessorValueTreeState apvts (stub, nullptr, "PARAMETERS", makeLayout());
        SuiteModulation::Router router (apvts, targets, numTargets, "sync");
        router.prepare (sampleRate);

        setLfo (apvts, 0, 1.0f, 0.0f, false, 4.0f);
        setRoute (apvts, 0, 0, 2, 37.0f);  // target B, a whole number

        for (int n = 0; n < 40; ++n)
        {
            const auto offsets = router.computeOffsets (n * 0.025 * sampleRate, 0.0, 0.0);
            const float value = router.modulate (2, 5.0f, offsets);
            assert (value == std::round (value));
            assert (value >= 0.0f && value <= 10.0f);

            // The display agrees: it is whole as well.
            router.publish (offsets, n * 0.025 * sampleRate, 0.0);
            const float shown = router.getModulatedValue (2);
            assert (shown == std::round (shown));
        }

        pass ("testWholeNumberTargetsReturnIntegers");
    }

    void testIgnoredWhileHostSyncReturnsBase()
    {
        StubProcessor stub;
        juce::AudioProcessorValueTreeState apvts (stub, nullptr, "PARAMETERS", makeLayout());
        SuiteModulation::Router router (apvts, targets, numTargets, "sync");
        router.prepare (sampleRate);

        setLfo (apvts, 0, 1.0f, 0.0f, false, 4.0f);
        setRoute (apvts, 0, 0, 3, 100.0f);  // target C, ignored while Host Sync is on
        const double time = 0.25 * sampleRate;

        set (apvts, "sync", 0.0f);
        router.publish (router.computeOffsets (time, 0.0, 0.0), time, 0.0);
        assert (router.getModulatedValue (3) != 20.0f);

        set (apvts, "sync", 1.0f);
        router.publish (router.computeOffsets (time, 0.0, 0.0), time, 0.0);
        assert (router.getModulatedValue (3) == 20.0f);

        pass ("testIgnoredWhileHostSyncReturnsBase");
    }

    void testSyncedPeriodIsOneBarEveryFourBeats()
    {
        StubProcessor stub;
        juce::AudioProcessorValueTreeState apvts (stub, nullptr, "PARAMETERS", makeLayout());
        SuiteModulation::Router router (apvts, targets, numTargets, "sync");
        router.prepare (sampleRate);

        setLfo (apvts, 0, 1.0f, 0.0f, true, 2.0f);  // sine, synced to 1 bar
        setRoute (apvts, 0, 0, 1, 100.0f);

        const auto a = router.computeOffsets (0.0, 1.0, 0.0);
        const auto b = router.computeOffsets (0.0, 5.0, 0.0);
        const auto c = router.computeOffsets (0.0, 1.5, 0.0);

        assert (a.value[1] == b.value[1]);  // one bar later: the same point in the cycle
        assert (a.value[1] != c.value[1]);  // half a beat later: a different point

        pass ("testSyncedPeriodIsOneBarEveryFourBeats");
    }

    void testSampleAndHoldHoldsOneValuePerCycle()
    {
        StubProcessor stub;
        juce::AudioProcessorValueTreeState apvts (stub, nullptr, "PARAMETERS", makeLayout());
        SuiteModulation::Router router (apvts, targets, numTargets, "sync");
        router.prepare (sampleRate);

        setLfo (apvts, 0, 1.0f, 4.0f, false, 4.0f);  // sample and hold, 1 Hz
        setRoute (apvts, 0, 0, 1, 100.0f);

        const float first = router.computeOffsets (0.1 * sampleRate, 0.0, 0.0).value[1];
        const float later = router.computeOffsets (0.9 * sampleRate, 0.0, 0.0).value[1];
        const float next = router.computeOffsets (1.1 * sampleRate, 0.0, 0.0).value[1];

        assert (first == later);  // same cycle, same level
        assert (first != next);   // the next cycle picks a new level

        pass ("testSampleAndHoldHoldsOneValuePerCycle");
    }

    void testOutOfRangeIndicesDoNothing()
    {
        StubProcessor stub;
        juce::AudioProcessorValueTreeState apvts (stub, nullptr, "PARAMETERS", makeLayout());
        SuiteModulation::Router router (apvts, targets, numTargets, "sync");
        router.prepare (sampleRate);

        const SuiteModulation::Router::Offsets none {};
        assert (router.modulate (-1, 12.0f, none) == 12.0f);
        assert (router.modulate (99, 12.0f, none) == 12.0f);
        assert (router.targetParameter (-1) == nullptr);
        assert (router.targetParameter (99) == nullptr);
        assert (router.targetParameter (0) == nullptr);  // None has no parameter
        assert (router.getModulatedValue (-1) == 0.0f);
        assert (router.getModulatedValue (99) == 0.0f);
        assert (router.getTargetOffset (-1) == 0.0f);
        assert (router.getTargetOffset (999) == 0.0f);
        assert (router.getLfoValue (-1) == 0.0f);
        assert (router.getLfoPhase (7) == 0.0f);

        pass ("testOutOfRangeIndicesDoNothing");
    }

    void testControlDisplayShowsValueWithoutNotifying()
    {
        StubProcessor stub;
        juce::AudioProcessorValueTreeState apvts (stub, nullptr, "PARAMETERS", makeLayout());
        SuiteModulation::Router router (apvts, targets, numTargets, "sync");
        router.prepare (sampleRate);

        // Count the listener calls: showing the modulation must not notify.
        struct Counter : juce::Slider::Listener
        {
            int calls = 0;
            void sliderValueChanged (juce::Slider*) override { ++calls; }
        } counter;

        juce::Slider slider;
        slider.setRange (0.0, 100.0, 0.0);
        slider.setValue (50.0, juce::dontSendNotification);
        slider.addListener (&counter);

        setLfo (apvts, 0, 1.0f, 0.0f, false, 4.0f);
        setRoute (apvts, 0, 0, 1, 100.0f);
        router.publish (router.computeOffsets (0.25 * sampleRate, 0.0, 0.0), 0.25 * sampleRate, 0.0);

        SuiteModulation::ControlDisplay display (router, { { slider, 1 } });
        display.update();

        assert (std::abs (slider.getValue() - 100.0) < 1e-4);
        assert (counter.calls == 0);

        slider.removeListener (&counter);

        pass ("testControlDisplayShowsValueWithoutNotifying");
    }
}  // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running SuiteModulation tests...");

    testRoutedLfoMovesTheBaseAndAmountZeroDoesNot();
    testZeroOffsetReturnsBaseBitForBit();
    testBipolarAmountGivesEqualOpposingOffsets();
    testWholeNumberTargetsReturnIntegers();
    testIgnoredWhileHostSyncReturnsBase();
    testSyncedPeriodIsOneBarEveryFourBeats();
    testSampleAndHoldHoldsOneValuePerCycle();
    testOutOfRangeIndicesDoNothing();
    testControlDisplayShowsValueWithoutNotifying();

    std::puts ("All SuiteModulation tests passed.");
    return 0;
}
