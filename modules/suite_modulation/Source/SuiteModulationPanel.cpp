#include "SuiteModulationPanel.h"

#include "SuiteWidgets.h"

namespace SuiteModulation
{
    namespace
    {
        // Layout constants. These match the Micro-Timing Chaos panel as it was tuned on screen.
        constexpr int lfoBlockHeight = 140;
        constexpr int knobWidth = 110;
        constexpr int scopeHeight = 46;
        constexpr int controlHeight = 26;
        constexpr int headingHeight = 16;
        constexpr int slotRowHeight = 26;
        constexpr int gap = 6;

        // Column split of the routing rows, shared by the headings, the rows and paint()
        constexpr float sourceFraction = 0.22f;
        constexpr float targetFraction = 0.32f;

        // Routing row: source, target, amount and delay, with a gap between each pair
        struct SlotColumns { int source, target, amount, delay; };

        SlotColumns slotColumns (int width)
        {
            SlotColumns columns;
            columns.source = juce::roundToInt ((float) width * sourceFraction);
            columns.target = juce::roundToInt ((float) width * targetFraction);

            const int rest = width - columns.source - columns.target - 3 * gap;
            columns.amount = rest / 2;
            columns.delay = rest - columns.amount;
            return columns;
        }

        const std::array<const char*, Router::lfoCount> lfoRateIds { "lfo1Rate", "lfo2Rate" };
        const std::array<const char*, Router::lfoCount> lfoShapeIds { "lfo1Shape", "lfo2Shape" };
        const std::array<const char*, Router::lfoCount> lfoSyncIds { "lfo1Sync", "lfo2Sync" };
        const std::array<const char*, Router::lfoCount> lfoDivisionIds { "lfo1Division", "lfo2Division" };
        const std::array<const char*, Router::lfoCount> lfoOffsetIds { "lfo1Offset", "lfo2Offset" };

        constexpr int offsetLabelWidth = 64;
        constexpr int offsetTextWidth = 60;

        const std::array<const char*, Router::slotCount> modSourceIds { "mod1Source", "mod2Source", "mod3Source", "mod4Source", "mod5Source", "mod6Source" };
        const std::array<const char*, Router::slotCount> modTargetIds { "mod1Target", "mod2Target", "mod3Target", "mod4Target", "mod5Target", "mod6Target" };
        const std::array<const char*, Router::slotCount> modAmountIds { "mod1Amount", "mod2Amount", "mod3Amount", "mod4Amount", "mod5Amount", "mod6Amount" };
        const std::array<const char*, Router::slotCount> modDelayIds { "mod1Delay", "mod2Delay", "mod3Delay", "mod4Delay", "mod5Delay", "mod6Delay" };

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
              scope (router, index, style.lfoColours[index], style.panel, style.track),
              offset (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight)
        {
            const auto i = (size_t) index;
            auto& state = router.getState();
            const auto name = "LFO " + juce::String (index + 1);

            shape.addItemList (shapeNames(), 1);
            division.addItemList (divisionNames(), 1);

            offsetLabel.setText ("OFFSET", juce::dontSendNotification);
            offsetLabel.setColour (juce::Label::textColourId, style.textDim);
            offsetLabel.setFont (juce::Font (12.0f, juce::Font::bold));
            offset.setComponentID (lfoOffsetIds[i]);
            offset.setWantsKeyboardFocus (true);   // arrow keys nudge the value once the control has focus
            offset.setTextBoxStyle (juce::Slider::TextBoxRight, false, offsetTextWidth, controlHeight);
            offset.setTextValueSuffix (" %");
            offset.setNumDecimalPlacesToDisplay (0);

            rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, lfoRateIds[i], rate.slider);
            shapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, lfoShapeIds[i], shape);
            syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, lfoSyncIds[i], sync);
            divisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, lfoDivisionIds[i], division);
            offsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, lfoOffsetIds[i], offset);

            rate.setHelpText (name + " rate in Hz. Disabled while Sync is on.");
            rate.slider.setHelpText (name + " rate in Hz. Disabled while Sync is on.");
            shape.setHelpText (name + " waveform. Sample & Hold picks a new random level on each cycle.");
            offset.setHelpText (name + " offset, from -100 % to +100 %. It moves the waveform up or down before routing, and the output stops at -1 and +1.");
            sync.setHelpText (name + " follows the host tempo. The division then sets the cycle length.");
            division.setHelpText (name + " cycle length in bars or beats. Used only while Sync is on.");
            scope.setHelpText (name + " shape. The dot shows where the LFO is now.");

            addAndMakeVisible (rate);
            addAndMakeVisible (shape);
            addAndMakeVisible (offsetLabel);
            addAndMakeVisible (offset);
            addAndMakeVisible (sync);
            addAndMakeVisible (division);
            addAndMakeVisible (scope);
        }

        void resized() override
        {
            auto area = getLocalBounds();

            // Rate knob on the left, the scope, then shape with offset, sync and division on the right
            rate.setBounds (area.removeFromLeft (knobWidth));
            area.removeFromLeft (gap);

            scope.setBounds (area.removeFromTop (scopeHeight));
            area.removeFromTop (5);

            shape.setBounds (area.removeFromTop (controlHeight));
            area.removeFromTop (4);

            // Offset gets a full-width row, so its track is long enough to drag
            auto offsetRow = area.removeFromTop (controlHeight);
            offsetLabel.setBounds (offsetRow.removeFromLeft (offsetLabelWidth));
            offset.setBounds (offsetRow);
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
        juce::Label offsetLabel;
        juce::Slider offset;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shapeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> offsetAttachment;
    };

    //==============================================================================
    struct ModulationPanel::SlotRow : public juce::Component
    {
        SlotRow (Router& router, int slot, const juce::StringArray& targetNames)
            : amount (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight),
              delay (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight)
        {
            auto& state = router.getState();
            const auto number = juce::String (slot + 1);

            source.addItemList (sourceNames(), 1);
            target.addItemList (targetNames, 1);
            amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 22);
            amount.setTextValueSuffix (" %");
            amount.setNumDecimalPlacesToDisplay (0);
            delay.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 22);
            delay.setTextValueSuffix (" s");
            delay.setNumDecimalPlacesToDisplay (2);

            sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, modSourceIds[(size_t) slot], source);
            targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, modTargetIds[(size_t) slot], target);
            amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, modAmountIds[(size_t) slot], amount);
            delayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, modDelayIds[(size_t) slot], delay);

            delay.setComponentID (modDelayIds[(size_t) slot]);
            delay.setWantsKeyboardFocus (true);   // arrow keys nudge the delay once the control has focus

            source.setHelpText ("Routing " + number + " source: which LFO drives this slot.");
            target.setHelpText ("Routing " + number + " target: the control this slot moves. None turns the slot off.");
            amount.setHelpText ("Routing " + number + " amount, from -100 % to +100 %. Negative values invert the LFO.");
            delay.setHelpText ("Routing " + number + " delay, from 0 to 10 s. Silent until this time has passed on the delay clock, which starts when playback starts.");

            addAndMakeVisible (source);
            addAndMakeVisible (target);
            addAndMakeVisible (amount);
            addAndMakeVisible (delay);
        }

        void resized() override
        {
            auto row = getLocalBounds().reduced (0, 1);
            const auto columns = slotColumns (getWidth());

            source.setBounds (row.removeFromLeft (columns.source));
            row.removeFromLeft (gap);
            target.setBounds (row.removeFromLeft (columns.target));
            row.removeFromLeft (gap);
            amount.setBounds (row.removeFromLeft (columns.amount));
            row.removeFromLeft (gap);
            delay.setBounds (row);
        }

        juce::ComboBox source;
        juce::ComboBox target;
        juce::Slider amount;
        juce::Slider delay;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
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
        const auto columns = slotColumns (getWidth());
        const int targetX = columns.source + gap;
        const int amountX = targetX + columns.target + gap;
        const int delayX = amountX + columns.amount + gap;

        g.setColour (style.textDim);
        g.setFont (juce::Font (12.0f, juce::Font::bold));

        g.drawText ("SOURCE", juce::Rectangle<int> (0, headingY, columns.source, headingHeight), juce::Justification::centredLeft);
        g.drawText ("TARGET", juce::Rectangle<int> (targetX, headingY, columns.target, headingHeight), juce::Justification::centredLeft);
        g.drawText ("AMOUNT", juce::Rectangle<int> (amountX, headingY, columns.amount, headingHeight), juce::Justification::centredLeft);
        g.drawText ("DELAY", juce::Rectangle<int> (delayX, headingY, columns.delay, headingHeight), juce::Justification::centredLeft);
    }

}  // namespace SuiteModulation
