#include "SuiteModulationPanel.h"

#include "SuiteWidgets.h"

namespace SuiteModulation
{
    namespace
    {
        // Layout constants. These match the Micro-Timing Chaos panel as it was tuned on screen.
        constexpr int lfoBlockHeight = 120;
        constexpr int knobWidth = 110;
        constexpr int scopeHeight = 46;
        constexpr int controlHeight = 26;
        constexpr int headingHeight = 16;
        constexpr int slotRowHeight = 26;
        constexpr int gap = 6;

        // Column split of the routing rows, shared by the headings, the rows and paint()
        constexpr float sourceFraction = 0.26f;
        constexpr float targetFraction = 0.40f;

        const std::array<const char*, Router::lfoCount> lfoRateIds { "lfo1Rate", "lfo2Rate" };
        const std::array<const char*, Router::lfoCount> lfoShapeIds { "lfo1Shape", "lfo2Shape" };
        const std::array<const char*, Router::lfoCount> lfoSyncIds { "lfo1Sync", "lfo2Sync" };
        const std::array<const char*, Router::lfoCount> lfoDivisionIds { "lfo1Division", "lfo2Division" };

        const std::array<const char*, Router::slotCount> modSourceIds { "mod1Source", "mod2Source", "mod3Source", "mod4Source", "mod5Source", "mod6Source" };
        const std::array<const char*, Router::slotCount> modTargetIds { "mod1Target", "mod2Target", "mod3Target", "mod4Target", "mod5Target", "mod6Target" };
        const std::array<const char*, Router::slotCount> modAmountIds { "mod1Amount", "mod2Amount", "mod3Amount", "mod4Amount", "mod5Amount", "mod6Amount" };

        juce::StringArray shapeNames() { return { "Sine", "Triangle", "Saw", "Square", "Sample & Hold", "Random", "Noise" }; }
        juce::StringArray divisionNames() { return { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16" }; }
        juce::StringArray sourceNames() { return { "LFO 1", "LFO 2" }; }

        juce::StringArray choiceNames (juce::AudioProcessorParameter* parameter)
        {
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
                return choice->choices;

            return {};
        }
    }

    //==============================================================================
    struct ModulationPanel::LfoStrip : public juce::Component
    {
        LfoStrip (Router& router, int index, const ModulationPanel::Style& style)
            : rate ("RATE", juce::Slider::RotaryVerticalDrag),
              scope (router, index, style.lfoColours[index], style.panel, style.track)
        {
            const auto i = (size_t) index;
            auto& state = router.getState();
            const auto name = "LFO " + juce::String (index + 1);

            shape.addItemList (shapeNames(), 1);
            division.addItemList (divisionNames(), 1);

            rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, lfoRateIds[i], rate.slider);
            shapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, lfoShapeIds[i], shape);
            syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, lfoSyncIds[i], sync);
            divisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, lfoDivisionIds[i], division);

            rate.setHelpText (name + " rate in Hz. Disabled while Sync is on.");
            rate.slider.setHelpText (name + " rate in Hz. Disabled while Sync is on.");
            shape.setHelpText (name + " waveform. Sample & Hold picks a new random level on each cycle.");
            sync.setHelpText (name + " follows the host tempo. The division then sets the cycle length.");
            division.setHelpText (name + " cycle length in bars or beats. Used only while Sync is on.");
            scope.setHelpText (name + " shape. The dot shows where the LFO is now.");

            addAndMakeVisible (rate);
            addAndMakeVisible (shape);
            addAndMakeVisible (sync);
            addAndMakeVisible (division);
            addAndMakeVisible (scope);
        }

        void resized() override
        {
            auto area = getLocalBounds();

            // Rate knob on the left, the scope, shape, sync and division on the right
            rate.setBounds (area.removeFromLeft (knobWidth));
            area.removeFromLeft (gap);

            scope.setBounds (area.removeFromTop (scopeHeight));
            area.removeFromTop (5);
            shape.setBounds (area.removeFromTop (controlHeight));
            area.removeFromTop (4);

            auto row = area.removeFromTop (controlHeight);
            sync.setBounds (row.removeFromLeft (row.getWidth() / 2));
            row.removeFromLeft (4);
            division.setBounds (row);
        }

        SuiteModulation::LfoScope& getScope() noexcept { return scope; }

        SuiteUI::LabelledSlider rate;
        juce::ComboBox shape;
        juce::ToggleButton sync { "SYNC" };
        juce::ComboBox division;
        SuiteModulation::LfoScope scope;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shapeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionAttachment;
    };

    //==============================================================================
    struct ModulationPanel::SlotRow : public juce::Component
    {
        SlotRow (Router& router, int slot, const juce::StringArray& targetNames)
            : amount (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight)
        {
            auto& state = router.getState();
            const auto number = juce::String (slot + 1);

            source.addItemList (sourceNames(), 1);
            target.addItemList (targetNames, 1);
            amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 22);
            amount.setTextValueSuffix (" %");
            amount.setNumDecimalPlacesToDisplay (0);

            sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, modSourceIds[(size_t) slot], source);
            targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, modTargetIds[(size_t) slot], target);
            amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, modAmountIds[(size_t) slot], amount);

            source.setHelpText ("Routing " + number + " source: which LFO drives this slot.");
            target.setHelpText ("Routing " + number + " target: the control this slot moves. None turns the slot off.");
            amount.setHelpText ("Routing " + number + " amount, from -100 % to +100 %. Negative values invert the LFO.");

            addAndMakeVisible (source);
            addAndMakeVisible (target);
            addAndMakeVisible (amount);
        }

        void resized() override
        {
            auto row = getLocalBounds().reduced (0, 1);
            const int sourceWidth = juce::roundToInt ((float) getWidth() * sourceFraction);
            const int targetWidth = juce::roundToInt ((float) getWidth() * targetFraction);

            source.setBounds (row.removeFromLeft (sourceWidth));
            row.removeFromLeft (gap);
            target.setBounds (row.removeFromLeft (targetWidth));
            row.removeFromLeft (gap);
            amount.setBounds (row);
        }

        juce::ComboBox source;
        juce::ComboBox target;
        juce::Slider amount;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    };

    //==============================================================================
    ModulationPanel::ModulationPanel (Router& r, const Style& s)
        : router (r), style (s)
    {
        setOpaque (false);

        for (int lfo = 0; lfo < Router::lfoCount; ++lfo)
        {
            lfoStrips[(size_t) lfo] = std::make_unique<LfoStrip> (router, lfo, style);
            addAndMakeVisible (*lfoStrips[(size_t) lfo]);
        }

        const auto targetNames = choiceNames (router.getState().getParameter (modTargetIds[0]));

        for (int slot = 0; slot < Router::slotCount; ++slot)
        {
            slotRows[(size_t) slot] = std::make_unique<SlotRow> (router, slot, targetNames);
            slotContainer.addAndMakeVisible (*slotRows[(size_t) slot]);
        }

        addAndMakeVisible (slotContainer);
        refresh();
    }

    ModulationPanel::~ModulationPanel() = default;

    void ModulationPanel::refresh()
    {
        for (int lfo = 0; lfo < Router::lfoCount; ++lfo)
        {
            auto& strip = *lfoStrips[(size_t) lfo];
            const bool synced = router.getState().getRawParameterValue (lfoSyncIds[(size_t) lfo])->load() > 0.5f;

            strip.rate.setEnabled (! synced);
            strip.division.setEnabled (synced);
            strip.getScope().refresh();
        }
    }

    void ModulationPanel::resized()
    {
        auto area = getLocalBounds();

        // The two LFO strips side by side
        auto lfoRow = area.removeFromTop (lfoBlockHeight);
        const int halfWidth = lfoRow.getWidth() / 2;

        lfoStrips[0]->setBounds (lfoRow.removeFromLeft (halfWidth));
        lfoStrips[1]->setBounds (lfoRow);

        // Routing: a gap, a heading row (drawn in paint), then the slot rows
        area.removeFromTop (gap);
        area.removeFromTop (headingHeight);

        slotContainer.setBounds (area);

        int y = 0;
        for (auto& row : slotRows)
        {
            row->setBounds (0, y, slotContainer.getWidth(), slotRowHeight);
            y += slotRowHeight;
        }
    }

    void ModulationPanel::paint (juce::Graphics& g)
    {
        // The headings sit just above the routing rows and line up with their columns.
        const int headingY = lfoBlockHeight + gap;
        const int sourceWidth = juce::roundToInt ((float) getWidth() * sourceFraction);
        const int targetWidth = juce::roundToInt ((float) getWidth() * targetFraction);
        const int amountX = sourceWidth + targetWidth + 2 * gap;

        g.setColour (style.textDim);
        g.setFont (juce::Font (12.0f, juce::Font::bold));

        g.drawText ("SOURCE", juce::Rectangle<int> (0, headingY, sourceWidth, headingHeight), juce::Justification::centredLeft);
        g.drawText ("TARGET", juce::Rectangle<int> (sourceWidth + gap, headingY, targetWidth, headingHeight), juce::Justification::centredLeft);
        g.drawText ("AMOUNT", juce::Rectangle<int> (amountX, headingY, getWidth() - amountX, headingHeight), juce::Justification::centredLeft);
    }

}  // namespace SuiteModulation
