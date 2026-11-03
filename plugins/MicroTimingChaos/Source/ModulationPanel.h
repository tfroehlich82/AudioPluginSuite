#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "LfoScope.h"
#include "PluginProcessor.h"
#include "SuiteWidgets.h"

/**
    The modulation controls: two LFOs at the top (rate, shape, sync, division and a scope), then four
    routing slots (source, target, signed amount).

    Each control is attached to its parameter, so the routing is saved with the session.
*/
class ModulationPanel : public juce::Component
{
public:
    explicit ModulationPanel (MicroTimingChaosProcessor& processor);
    ~ModulationPanel() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    /** Called from the editor's timer. Enables the rate or division that applies, and refreshes the scopes. */
    void refresh();

private:
    struct LfoStrip;
    struct SlotRow;

    MicroTimingChaosProcessor& processor;

    std::unique_ptr<LfoStrip> lfos[MicroTimingChaosProcessor::lfoCount];
    std::unique_ptr<SlotRow> slots[MicroTimingChaosProcessor::slotCount];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationPanel)
};
