#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

/**
    One cycle of an LFO's shape, with a dot at the position and level the audio is using right now.

    The dot reads the published phase and value, not the parameters, so it shows what the sequencer
    actually receives.
*/
class LfoScope : public juce::Component
{
public:
    LfoScope (MicroTimingChaosProcessor& processor, int lfoIndex, const char* shapeParameterId, juce::Colour colour);

    /** Repaints only when the published position has changed. Call from the editor's timer. */
    void refresh();

    void paint (juce::Graphics&) override;

private:
    MicroTimingChaosProcessor& processor;
    const int lfoIndex;
    const char* shapeId;
    const juce::Colour colour;

    float shownPhase = -1.0f;
    float shownValue = 2.0f;
};
