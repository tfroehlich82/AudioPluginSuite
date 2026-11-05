#include "ModulationPanel.h"

#include "MicroTimingChaosLookAndFeel.h"

namespace
{
    namespace Palette = MicroTimingChaos::Palette;

    constexpr int lfoKnobWidth = 84;
    constexpr int scopeHeight = 38;
    constexpr int controlHeight = 20;
    constexpr int lfoBlockHeight = 96;
    constexpr int headingHeight = 14;
    constexpr int slotRowHeight = 22;

    const juce::StringArray shapeNames { "Sine", "Triangle", "Saw", "Square", "Sample & Hold", "Random", "Noise" };
    const juce::StringArray divisionNames { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16" };
    const juce::StringArray sourceNames { "LFO 1", "LFO 2" };

    void describe (juce::Component& component, const juce::String& help)
    {
        component.setHelpText (help);
    }
}

//==============================================================================
struct ModulationPanel::LfoStrip
{
    LfoStrip (MicroTimingChaosProcessor& processor, int index, const char* rateId, const char* shapeId,
                const char* syncId, const char* divisionId, juce::Colour colour)
        : rate ("RATE", juce::Slider::RotaryVerticalDrag),
          scope (processor, index, shapeId, colour)
    {
        shape.addItemList (shapeNames, 1);
        division.addItemList (divisionNames, 1);

        rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, rateId, rate.slider);
        shapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, shapeId, shape);
        syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, syncId, sync);
        divisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, divisionId, division);

        const auto name = juce::String ("LFO ") + juce::String (index + 1);
        describe (rate, name + " rate in Hz. Disabled while Sync is on.");
        describe (rate.slider, name + " rate in Hz. Disabled while Sync is on.");
        describe (shape, name + " waveform. Sample & Hold picks a new random level on each cycle.");
        describe (sync, name + " follows the host tempo. The division then sets the cycle length.");
        describe (division, name + " cycle length in bars or beats. Used only while Sync is on.");
        describe (scope, name + " shape. The dot shows where the LFO is now.");
    }

    SuiteUI::LabelledSlider rate;
    juce::ComboBox shape;
    juce::ToggleButton sync { "SYNC" };
    juce::ComboBox division;
    LfoScope scope;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shapeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionAttachment;
};

//==============================================================================
struct ModulationPanel::SlotRow
{
    SlotRow (MicroTimingChaosProcessor& processor, int slot)
    {
        source.addItemList (sourceNames, 1);
        target.addItemList (MicroTimingChaosProcessor::modTargetNames(), 1);
        amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
        amount.setTextValueSuffix (" %");
        amount.setNumDecimalPlacesToDisplay (0);

        sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            processor.apvts, MicroTimingChaosProcessor::modSourceIds[slot], source);
        targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            processor.apvts, MicroTimingChaosProcessor::modTargetIds[slot], target);
        amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, MicroTimingChaosProcessor::modAmountIds[slot], amount);

        const auto number = juce::String (slot + 1);
        describe (source, "Routing " + number + " source: which LFO drives this slot.");
        describe (target, "Routing " + number + " target: the control this slot moves. None turns the slot off.");
        describe (amount, "Routing " + number + " amount, from -100 % to +100 %. Negative values invert the LFO.");
    }

    juce::ComboBox source;
    juce::ComboBox target;
    juce::Slider amount { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
};

//==============================================================================
ModulationPanel::ModulationPanel (MicroTimingChaosProcessor& p)
    : processor (p)
{
    setOpaque (false);

    lfos[0] = std::make_unique<LfoStrip> (processor, 0, MicroTimingChaosProcessor::lfo1RateId,
                                            MicroTimingChaosProcessor::lfo1ShapeId, MicroTimingChaosProcessor::lfo1SyncId,
                                            MicroTimingChaosProcessor::lfo1DivisionId, juce::Colours::purple);
    lfos[1] = std::make_unique<LfoStrip> (processor, 1, MicroTimingChaosProcessor::lfo2RateId,
                                            MicroTimingChaosProcessor::lfo2ShapeId, MicroTimingChaosProcessor::lfo2SyncId,
                                            MicroTimingChaosProcessor::lfo2DivisionId, juce::Colours::orange);

    for (int slot = 0; slot < MicroTimingChaosProcessor::slotCount; ++slot)
        slots[slot] = std::make_unique<SlotRow> (processor, slot);

    for (auto& lfo : lfos)
    {
        addAndMakeVisible (lfo->rate);
        addAndMakeVisible (lfo->shape);
        addAndMakeVisible (lfo->sync);
        addAndMakeVisible (lfo->division);
        addAndMakeVisible (lfo->scope);
    }

    for (auto& slot : slots)
    {
        addAndMakeVisible (slot->source);
        addAndMakeVisible (slot->target);
        addAndMakeVisible (slot->amount);
    }

    refresh();
}

ModulationPanel::~ModulationPanel() = default;

void ModulationPanel::refresh()
{
    for (int i = 0; i < MicroTimingChaosProcessor::lfoCount; ++i)
    {
        auto& lfo = *lfos[i];
        const bool synced = processor.apvts.getRawParameterValue (i == 0 ? MicroTimingChaosProcessor::lfo1SyncId
                                                                     : MicroTimingChaosProcessor::lfo2SyncId)->load() > 0.5f;

        lfo.rate.setEnabled (! synced);
        lfo.division.setEnabled (synced);
        lfo.scope.refresh();
    }
}

void ModulationPanel::resized()
{
    auto area = getLocalBounds();

    // The two LFOs side by side: a rate knob on the left, the scope, shape, sync and division on the right
    auto lfoRow = area.removeFromTop (lfoBlockHeight);
    const int halfWidth = lfoRow.getWidth() / 2;

    for (int i = 0; i < MicroTimingChaosProcessor::lfoCount; ++i)
    {
        auto cell = i == 0 ? lfoRow.removeFromLeft (halfWidth) : lfoRow;
        auto& lfo = *lfos[i];

        lfo.rate.setBounds (cell.removeFromLeft (lfoKnobWidth));
        cell.removeFromLeft (6);

        lfo.scope.setBounds (cell.removeFromTop (scopeHeight));
        cell.removeFromTop (5);
        lfo.shape.setBounds (cell.removeFromTop (controlHeight));
        cell.removeFromTop (4);

        auto row = cell.removeFromTop (controlHeight);
        lfo.sync.setBounds (row.removeFromLeft (row.getWidth() / 3));
        row.removeFromLeft (4);
        lfo.division.setBounds (row);
    }

    area.removeFromTop (6);

    // Routing: column headings, then one row per slot
    auto heading = area.removeFromTop (headingHeight);
    const int sourceWidth = juce::roundToInt ((float) area.getWidth() * 0.26f);
    const int targetWidth = juce::roundToInt ((float) area.getWidth() * 0.40f);
    heading.removeFromLeft (sourceWidth + 6);
    heading.removeFromLeft (targetWidth + 6);

    for (auto& slot : slots)
    {
        auto row = area.removeFromTop (slotRowHeight).reduced (0, 1);
        slot->source.setBounds (row.removeFromLeft (sourceWidth));
        row.removeFromLeft (6);
        slot->target.setBounds (row.removeFromLeft (targetWidth));
        row.removeFromLeft (6);
        slot->amount.setBounds (row);
    }
}

void ModulationPanel::paint (juce::Graphics& g)
{
    // The column headings sit in the heading row under the LFO strips, and line up with the routing rows.
    const int headingY = lfoBlockHeight + 6;
    const int sourceWidth = juce::roundToInt ((float) getWidth() * 0.26f);
    const int targetWidth = juce::roundToInt ((float) getWidth() * 0.40f);

    g.setColour (Palette::textDim);
    g.setFont (juce::Font (9.0f, juce::Font::bold));

    g.drawText ("SOURCE", juce::Rectangle<int> (0, headingY, sourceWidth, headingHeight), juce::Justification::centredLeft);
    g.drawText ("TARGET", juce::Rectangle<int> (sourceWidth + 6, headingY, targetWidth, headingHeight), juce::Justification::centredLeft);
    g.drawText ("AMOUNT", juce::Rectangle<int> (sourceWidth + targetWidth + 12, headingY, getWidth(), headingHeight), juce::Justification::centredLeft);
}
