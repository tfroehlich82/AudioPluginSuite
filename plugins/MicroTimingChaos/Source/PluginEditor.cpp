#include "PluginEditor.h"

#include "SequencerEngine.h"

namespace
{
    constexpr int editorWidth = 1040;
    constexpr int editorHeight = 680;
    constexpr int knobColumns = 4;

    // Places components in a grid, left to right and top to bottom.
    void layoutGrid (juce::Rectangle<int> area, const std::vector<juce::Component*>& cells, int columns)
    {
        const int rows = ((int) cells.size() + columns - 1) / columns;
        const int cellWidth = area.getWidth() / columns;
        const int cellHeight = area.getHeight() / juce::jmax (1, rows);

        for (int i = 0; i < (int) cells.size(); ++i)
        {
            const int column = i % columns;
            const int row = i / columns;
            cells[(size_t) i]->setBounds (juce::Rectangle<int> (area.getX() + column * cellWidth, area.getY() + row * cellHeight,
                                                                 cellWidth, cellHeight).reduced (3));
        }
    }
}

//==============================================================================
StepGridView::StepGridView (juce::Colour accentColour)
    : accent (accentColour)
{
}

void StepGridView::setState (int patternLength, int playhead, float timingRangeMs, std::vector<bool> fires,
                             std::vector<MicroTimingChaosProcessor::StepEvent> events)
{
    length = juce::jlimit (CoreDSP::Sequencer::minSteps, CoreDSP::Sequencer::maxSteps, patternLength);
    playheadStep = playhead;
    rangeMs = juce::jmax (1.0f, timingRangeMs);
    firing = std::move (fires);
    recent = std::move (events);
    repaint();
}

void StepGridView::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (4.0f);
    const float columnWidth = area.getWidth() / (float) length;
    const float centreY = area.getCentreY();

    g.setColour (SuiteUI::Colours::panel);
    g.fillRoundedRectangle (area, 6.0f);

    // Timing centre line and the range limits.
    g.setColour (SuiteUI::Colours::track);
    g.drawHorizontalLine ((int) centreY, area.getX(), area.getRight());

    for (int step = 0; step < length; ++step)
    {
        const auto column = juce::Rectangle<float> (area.getX() + (float) step * columnWidth, area.getY(), columnWidth, area.getHeight()).reduced (3.0f);
        const bool fires = step < (int) firing.size() && firing[(size_t) step];

        // Steps that fire are filled, steps that rest stay dim. The playhead outlines its column.
        g.setColour (fires ? accent.withAlpha (0.18f) : SuiteUI::Colours::panelLight);
        g.fillRoundedRectangle (column.withTrimmedBottom (column.getHeight() * 0.7f), 4.0f);

        if (step == playheadStep)
        {
            g.setColour (accent);
            g.drawRoundedRectangle (column, 4.0f, 2.0f);
        }

        g.setColour (SuiteUI::Colours::textDim);
        g.setFont (juce::Font (10.0f));
        g.drawText (juce::String (step + 1), column.withY (column.getBottom() - 14.0f).withHeight (12.0f),
                    juce::Justification::centred);
    }

    // Recent notes: horizontal position is the step, vertical offset is the timing shift, dot size is the velocity.
    for (const auto& event : recent)
    {
        if (event.velocity <= 0.0f || event.patternStep < 0 || event.patternStep >= length)
            continue;

        const float x = area.getX() + ((float) event.patternStep + 0.5f) * columnWidth;
        const float y = centreY - juce::jlimit (-1.0f, 1.0f, event.offsetMs / rangeMs) * area.getHeight() * 0.35f;
        const float radius = 3.0f + event.velocity / 127.0f * 5.0f;

        g.setColour (accent);
        g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);
    }
}

//==============================================================================
MicroTimingChaosEditor::MicroTimingChaosEditor (MicroTimingChaosProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (820, 580, 1600, 1100);

    readout.setJustificationType (juce::Justification::centredLeft);
    readout.setColour (juce::Label::textColourId, SuiteUI::Colours::text);
    readout.setFont (juce::Font (13.0f));

    for (auto* component : { static_cast<juce::Component*> (&attractor), static_cast<juce::Component*> (&grid),
                             static_cast<juce::Component*> (&readout), static_cast<juce::Component*> (&rateSelector),
                             static_cast<juce::Component*> (&scaleBox), static_cast<juce::Component*> (&syncButton),
                             static_cast<juce::Component*> (&lengthSlider), static_cast<juce::Component*> (&densitySlider),
                             static_cast<juce::Component*> (&seedSlider), static_cast<juce::Component*> (&gateSlider),
                             static_cast<juce::Component*> (&swingSlider), static_cast<juce::Component*> (&velocitySlider),
                             static_cast<juce::Component*> (&rootSlider), static_cast<juce::Component*> (&octavesSlider),
                             static_cast<juce::Component*> (&chaosSlider), static_cast<juce::Component*> (&timingSlider),
                             static_cast<juce::Component*> (&velocityChaosSlider), static_cast<juce::Component*> (&speedSlider),
                             static_cast<juce::Component*> (&tempoSlider) })
        addAndMakeVisible (component);

    // Rate: a choice parameter shown as a segmented switch.
    rateSelector.setOptions ({ "1/4", "1/8", "1/16", "1/32" });
    rateAttachment = std::make_unique<juce::ParameterAttachment> (
        *processor.apvts.getParameter (MicroTimingChaosProcessor::rateId),
        [this] (float value) { rateSelector.setSelectedIndex (juce::roundToInt (value)); });
    rateSelector.onChange = [this] (int index) { rateAttachment->setValueAsCompleteGesture ((float) index); };
    rateAttachment->sendInitialUpdate();

    scaleBox.addItemList ({ "Chromatic", "Major", "Minor", "Minor Pentatonic", "Dorian", "Whole Tone" }, 1);
    scaleBox.setTextWhenNothingSelected ("Scale");

    // Units and formatting.
    lengthSlider.slider.setNumDecimalPlacesToDisplay (0);
    densitySlider.slider.setTextValueSuffix (" %");
    seedSlider.slider.setNumDecimalPlacesToDisplay (0);
    gateSlider.slider.setTextValueSuffix (" %");
    swingSlider.slider.setTextValueSuffix (" %");
    velocitySlider.slider.setNumDecimalPlacesToDisplay (0);
    rootSlider.slider.setNumDecimalPlacesToDisplay (0);
    octavesSlider.slider.setNumDecimalPlacesToDisplay (0);
    chaosSlider.slider.setTextValueSuffix (" %");
    timingSlider.slider.setTextValueSuffix (" ms");
    velocityChaosSlider.slider.setTextValueSuffix (" %");
    speedSlider.slider.setNumDecimalPlacesToDisplay (2);
    tempoSlider.slider.setTextValueSuffix (" BPM");

    auto& apvts = processor.apvts;

    auto bind = [this, &apvts] (const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider));
    };

    bind (MicroTimingChaosProcessor::lengthId, lengthSlider.slider);
    bind (MicroTimingChaosProcessor::densityId, densitySlider.slider);
    bind (MicroTimingChaosProcessor::seedId, seedSlider.slider);
    bind (MicroTimingChaosProcessor::gateId, gateSlider.slider);
    bind (MicroTimingChaosProcessor::swingId, swingSlider.slider);
    bind (MicroTimingChaosProcessor::baseVelocityId, velocitySlider.slider);
    bind (MicroTimingChaosProcessor::rootId, rootSlider.slider);
    bind (MicroTimingChaosProcessor::octavesId, octavesSlider.slider);
    bind (MicroTimingChaosProcessor::chaosId, chaosSlider.slider);
    bind (MicroTimingChaosProcessor::timingRangeId, timingSlider.slider);
    bind (MicroTimingChaosProcessor::velocityChaosId, velocityChaosSlider.slider);
    bind (MicroTimingChaosProcessor::speedId, speedSlider.slider);
    bind (MicroTimingChaosProcessor::bpmId, tempoSlider.slider);

    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, MicroTimingChaosProcessor::scaleId, scaleBox);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, MicroTimingChaosProcessor::syncId, syncButton);

    startTimerHz (60);
}

MicroTimingChaosEditor::~MicroTimingChaosEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MicroTimingChaosEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (SuiteUI::Colours::panelLight, 0.0f, 0.0f,
                                     SuiteUI::Colours::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    g.setColour (lookAndFeel.getAccent());
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText ("MICRO-TIMING CHAOS", getLocalBounds().reduced (16, 0).removeFromTop (30),
                juce::Justification::centredLeft);

    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (juce::Font (12.0f));
    g.drawText ("Lorenz X shifts the timing  |  Y picks the pitch and gate  |  Z sets the velocity",
                getLocalBounds().reduced (16, 0).removeFromTop (30), juce::Justification::centredRight);
}

void MicroTimingChaosEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (34);

    // Top strip: rate, scale and host sync.
    auto strip = area.removeFromTop (30);
    rateSelector.setBounds (strip.removeFromLeft (360).reduced (0, 2));
    strip.removeFromLeft (12);
    scaleBox.setBounds (strip.removeFromLeft (220).reduced (0, 3));
    strip.removeFromLeft (12);
    syncButton.setBounds (strip.removeFromLeft (150).reduced (0, 2));
    area.removeFromTop (10);

    // Left: attractor, readout and step grid. Right: all knobs.
    auto left = area.removeFromLeft ((int) ((float) area.getWidth() * 0.5f));
    area.removeFromLeft (12);

    attractor.setBounds (left.removeFromTop ((int) ((float) left.getHeight() * 0.36f)));
    readout.setBounds (left.removeFromTop (22));
    left.removeFromTop (6);
    grid.setBounds (left);

    layoutGrid (area,
                { &lengthSlider, &densitySlider, &seedSlider, &gateSlider,
                  &swingSlider, &velocitySlider, &rootSlider, &octavesSlider,
                  &chaosSlider, &timingSlider, &velocityChaosSlider, &speedSlider,
                  &tempoSlider },
                knobColumns);
}

void MicroTimingChaosEditor::timerCallback()
{
    attractor.setTrail (processor.getTrail());

    const auto& apvts = processor.apvts;
    const int lengthValue = juce::roundToInt (apvts.getRawParameterValue (MicroTimingChaosProcessor::lengthId)->load());
    const auto seedValue = (std::uint32_t) juce::roundToInt (apvts.getRawParameterValue (MicroTimingChaosProcessor::seedId)->load());
    const float densityValue = apvts.getRawParameterValue (MicroTimingChaosProcessor::densityId)->load() / 100.0f;
    const float timingValue = apvts.getRawParameterValue (MicroTimingChaosProcessor::timingRangeId)->load();

    std::vector<bool> fires;
    for (int step = 0; step < CoreDSP::Sequencer::maxSteps; ++step)
        fires.push_back (step < lengthValue && CoreDSP::Sequencer::stepFires (seedValue, step, densityValue));

    const int playhead = processor.getPlayheadStep();
    grid.setState (lengthValue, playhead, timingValue, fires, processor.getRecentSteps());

    readout.setText (playhead >= 0 ? "Playhead: step " + juce::String (playhead + 1) + " of " + juce::String (lengthValue)
                                   : juce::String ("Transport stopped. Press play, or turn off Host Sync."),
                     juce::dontSendNotification);
}
