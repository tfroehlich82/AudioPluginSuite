#pragma once

/**
    Helpers for tests of LFOs and modulation routing. They work with any processor that uses the shared
    LFO and routing parameter IDs (lfo1Rate ... mod6Amount, see modules/suite_modulation), so every plugin
    can use them. The processor is a template parameter, so this header does not depend on any plugin.

    Include PluginTestUtils.h first (for setParameter).
*/

#include <cstdint>
#include <vector>

namespace PluginTest
{
    /** Sets one LFO. Shape is the choice index (0 sine ... 6 noise). Division is the choice index (4 is 1/4). */
    template <typename Processor>
    void setLfo (Processor& p, int lfo, float rateHz, float shape, bool synced, float division)
    {
        setParameter (p.apvts, lfo == 0 ? Processor::lfo1RateId : Processor::lfo2RateId, rateHz);
        setParameter (p.apvts, lfo == 0 ? Processor::lfo1ShapeId : Processor::lfo2ShapeId, shape);
        setParameter (p.apvts, lfo == 0 ? Processor::lfo1SyncId : Processor::lfo2SyncId, synced ? 1.0f : 0.0f);
        setParameter (p.apvts, lfo == 0 ? Processor::lfo1DivisionId : Processor::lfo2DivisionId, division);
    }

    /** Routes one slot. Source 0 is LFO 1 and 1 is LFO 2. Target is the choice index (0 is None). Amount is -100 to +100. */
    template <typename Processor>
    void setRoute (Processor& p, int slot, int source, int target, float amount)
    {
        setParameter (p.apvts, Processor::modSourceIds[slot], (float) source);
        setParameter (p.apvts, Processor::modTargetIds[slot], (float) target);
        setParameter (p.apvts, Processor::modAmountIds[slot], amount);
    }

    /**
        The offset of each note-on from its grid position, in samples. Grid position k is latency + k * step.
        Events are any list with a time and a MIDI message (for example the test's delivered events).
    */
    template <typename Events>
    std::vector<int64_t> timingOffsets (const Events& events, int64_t latency, int64_t step)
    {
        std::vector<int64_t> offsets;
        int64_t k = 0;

        for (const auto& event : events)
            if (event.message.isNoteOn())
                offsets.push_back (event.time - (latency + k++ * step));

        return offsets;
    }
}  // namespace PluginTest
