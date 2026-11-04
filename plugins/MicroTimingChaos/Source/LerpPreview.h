#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

#include "PluginProcessor.h"

/**
    The live view of the lerp curve's effect: how far each axis is drifting right now, and the
    timing, pitch and velocity of the most recent steps.
*/
class LerpPreview : public juce::Component
{
public:
    LerpPreview();

    /** Called from the editor's timer with the latest data from the processor. */
    void refresh (std::vector<MicroTimingChaosProcessor::StepEvent> recentSteps,
                  MicroTimingChaosProcessor::DriftAmounts amounts,
                  int rootNote, float timingRangeMs);

    void paint (juce::Graphics&) override;

private:
    std::vector<MicroTimingChaosProcessor::StepEvent> steps;
    MicroTimingChaosProcessor::DriftAmounts drift;
    int root = 48;
    float rangeMs = 20.0f;
};
