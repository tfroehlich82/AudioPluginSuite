#pragma once

#include "SuiteModulationRouter.h"

#include <array>

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

        /**
            The drawn curve's value at a phase within one cycle, with the current shape and Offset. The dot is
            drawn here, so it always lies on the curve, even when the audio has not published new values.
        */
        float curveValue(float phase) const;

        void paint(juce::Graphics&) override;

    private:
        Router& router;
        const int lfoIndex;
        const juce::Colour colour, background, track;

        float shownPhase = -1.0f;
        float shownValue = 2.0f;
        float shownOffset = 2.0f;
        int shownShape = -1;

        // Shapes that change from cycle to cycle (sample and hold, random, noise) are drawn as a scrolling
        // history of the output, because one cycle of them does not show anything.
        static constexpr int historyLength = 128;
        std::array<float, historyLength> history {};
        int historyNext = 0;
    };

}  // namespace SuiteModulation
