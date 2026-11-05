#pragma once

#include "SuiteModulationRouter.h"

namespace SuiteModulation
{
    /**
        One cycle of an LFO's shape, with a dot at the position and level the audio is using right now.

        The dot reads the published phase and value, not the parameters, so it shows what the routing
        actually receives.
    */
    class LfoScope : public juce::Component
    {
    public:
        LfoScope(Router& router, int lfoIndex, juce::Colour colour, juce::Colour background, juce::Colour track);

        /** Repaints only when the published position has changed. Call from the editor's timer. */
        void refresh();

        void paint(juce::Graphics&) override;

    private:
        Router& router;
        const int lfoIndex;
        const juce::Colour colour, background, track;

        float shownPhase = -1.0f;
        float shownValue = 2.0f;
    };

}  // namespace SuiteModulation
