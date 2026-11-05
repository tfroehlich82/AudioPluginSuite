#pragma once

#include "SuiteModulationRouter.h"
#include "SuiteModulationScope.h"

#include <memory>

namespace SuiteModulation
{
    /**
        The modulation controls: two LFO strips at the top (rate, shape, sync, division and a scope), then
        the routing slots (source, target, signed amount), in a slot container below them.

        Every control is attached to its parameter through the router's state, so the settings save with
        the session. The panel does not own the state.
    */
    class ModulationPanel : public juce::Component
    {
    public:
        struct Style
        {
            juce::Colour lfoColours[Router::lfoCount];
            juce::Colour text, textDim, panel, track;
        };

        ModulationPanel(Router& router, const Style& style);
        ~ModulationPanel() override;

        void resized() override;
        void paint(juce::Graphics&) override;

        /** Called from the editor's timer. Enables the rate or division that applies, and refreshes the scopes. */
        void refresh();

    private:
        struct LfoStrip;
        struct SlotRow;

        Router& router;
        Style style;

        std::unique_ptr<LfoStrip> lfoStrips[Router::lfoCount];
        juce::Component slotContainer;
        std::unique_ptr<SlotRow> slotRows[Router::slotCount];

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationPanel)
    };

}  // namespace SuiteModulation
