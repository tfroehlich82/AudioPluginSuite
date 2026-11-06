#pragma once

#include "SuiteModulationRouter.h"

#include <vector>

namespace SuiteModulation
{
    /**
        Shows the modulated value on each routable slider: the base value moved by the offset its target
        currently receives. The router handles Tempo while Host Sync is on and whole-number rounding, so the
        plugin only lists which slider belongs to which target.

        Sliders that the mouse is holding are skipped, so a drag is never overwritten. Values are set
        without notifying listeners, so showing the modulation does not write to the parameters.
    */
    class ControlDisplay
    {
    public:
        struct Entry
        {
            juce::Slider& slider;
            int targetIndex;  // choice index in the router: 1 is the first target, 0 is None
        };

        ControlDisplay (Router& router, std::vector<Entry> entries);

        /** Called from the editor's timer. */
        void update();

    private:
        Router& router;
        std::vector<Entry> entries;
    };

}  // namespace SuiteModulation
