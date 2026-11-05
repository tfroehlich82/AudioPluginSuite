#include "SuiteModulationControls.h"

namespace SuiteModulation
{
    ControlDisplay::ControlDisplay (Router& r, std::vector<Entry> e)
        : router (r), entries (std::move (e))
    {
    }

    void ControlDisplay::update()
    {
        for (const auto& entry : entries)
        {
            // While the user drags a control, the displayed value is theirs; the modulation resumes on release.
            if (entry.slider.isMouseButtonDown())
                continue;

            entry.slider.setValue (router.getModulatedValue (entry.targetIndex), juce::dontSendNotification);
        }
    }

}  // namespace SuiteModulation
