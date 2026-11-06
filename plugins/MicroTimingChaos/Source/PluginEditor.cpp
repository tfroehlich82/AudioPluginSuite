#include "PluginEditor.h"

namespace
{
    namespace Palette = MicroTimingChaos::Palette;

    // The layout is designed at this size. The window is shown at two thirds of it (see setScaleFactor).
    constexpr float designScale = 2.0f / 3.0f;
    constexpr int groupTitleHeight = 26;
    constexpr int titleBandHeight = 44;
    constexpr int helpHeight = 26;
    constexpr int readoutHeight = 24;

    // One gap for everything: the outer margin, and the space between every pair of groups.
    constexpr int groupGap = 14;

    // Column widths. Left: lerp, pattern, melody and chaos. Right: attractor, LFO and transport.
    constexpr int leftColumnWidth = 830;
    constexpr int rightColumnWidth = 823;
    constexpr int editorWidth = 3 * groupGap + leftColumnWidth + rightColumnWidth;   // 1695

    // Rows: 1 lerp and attractor, 2 pattern and LFO, 3 melody, chaos and transport, 4 sequencer.
    constexpr int lerpGroupHeight = 310;
    constexpr int rowTwoHeight = 340;
    constexpr int lowerGroupHeight = 180;
    constexpr int gridMinHeight = 300;
    constexpr int editorHeight = 1380;
    constexpr int minEditorHeight = titleBandHeight + 2 * groupGap + lerpGroupHeight + rowTwoHeight
                                    + lowerGroupHeight + gridMinHeight + helpHeight + 4 * groupGap;

    // Melody and chaos share the left column's lower row.
    constexpr int melodyGroupWidth = 398;
    constexpr int chaosGroupWidth = leftColumnWidth - melodyGroupWidth - groupGap;   // 418

    // Knobs keep at most this size. They are centred in their cells, so large cells do not enlarge them.
    constexpr int knobMaxWidth = 110;
    constexpr int knobMaxHeight = 150;

    /** Centres a knob in its cell, no larger than knobMaxWidth x knobMaxHeight. */
    void placeKnob (juce::Component& knob, juce::Rectangle<int> cell)
    {
        knob.setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), knobMaxWidth),
                                                    juce::jmin (cell.getHeight(), knobMaxHeight)));
    }

    /** Emphasised caption above a knob: bold and centred. */
    void emphasiseCaption (juce::Label& label)
    {
        label.setFont (juce::Font (13.0f, juce::Font::bold));
        label.setJustificationType (juce::Justification::centred);
    }

    /** Splits an area into a grid of cells, with a little room around each one. */
    std::vector<juce::Rectangle<int>> gridCells (juce::Rectangle<int> area, int count, int columns)
    {
        std::vector<juce::Rectangle<int>> cells;
        const int rows = (count + columns - 1) / columns;
        const int cellWidth = area.getWidth() / columns;
        const int cellHeight = area.getHeight() / juce::jmax (1, rows);

        for (int i = 0; i < count; ++i)
        {
            const int column = i % columns;
            const int row = i / columns;
            cells.push_back (juce::Rectangle<int> (area.getX() + column * cellWidth, area.getY() + row * cellHeight,
                                                   cellWidth, cellHeight).reduced (3));
        }

        return cells;
    }

    /** The area inside a group frame, below its title. */
    juce::Rectangle<int> innerOf (juce::Rectangle<int> group)
    {
        return group.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
    }

    void describe (juce::Component& component, const juce::String& help)
    {
        component.setHelpText (help);
    }

    /** Sets the same help text on a knob, its caption and its slider, so hovering any of them shows it. */
    void describe (SuiteUI::LabelledSlider& control, const juce::String& help)
    {
        control.setHelpText (help);
        control.label.setHelpText (help);
        control.slider.setHelpText (help);
        control.slider.setTooltip (help);
    }

    void styleCaption (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (13.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, Palette::textDim);
        label.setJustificationType (juce::Justification::centred);
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
    const auto bounds = getLocalBounds().toFloat();
    constexpr float axisWidth = 58.0f;
    constexpr float numberHeight = 26.0f;

    // Panel: a soft vertical gradient
    g.setGradientFill (juce::ColourGradient (Palette::panelLight, 0.0f, bounds.getY(),
                                             Palette::panel, 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 8.0f);

    const auto plot = bounds.withTrimmedLeft (axisWidth).withTrimmedBottom (numberHeight).reduced (8.0f, 10.0f);
    const float columnWidth = plot.getWidth() / (float) juce::jmax (1, length);
    const float halfHeight = plot.getHeight() * 0.5f;
    const float range = juce::jmax (1.0f, rangeMs);
    const float centreY = plot.getCentreY();

    auto yFor = [=] (float offsetMs) { return centreY - juce::jlimit (-1.0f, 1.0f, offsetMs / range) * halfHeight; };

    // Timing guides, with labels on the left: +range, 0 and -range
    g.setFont (juce::Font (13.0f));
    const float guides[] = { range, 0.0f, -range };
    for (const float value : guides)
    {
        const float y = yFor (value);
        g.setColour (value == 0.0f ? Palette::track.brighter (0.35f) : Palette::track.withAlpha (0.55f));
        g.drawLine (plot.getX(), y, plot.getRight(), y, value == 0.0f ? 1.5f : 1.0f);

        const juce::String text = value == 0.0f
            ? juce::String ("0")
            : juce::String (value > 0.0f ? "+" : "-") + juce::String (juce::roundToInt (std::abs (value))) + " ms";
        g.setColour (Palette::textDim);
        g.drawText (text, juce::Rectangle<float> (bounds.getX() + 4.0f, y - 10.0f, axisWidth - 8.0f, 20.0f),
                    juce::Justification::centredRight);
    }

    // Steps: separators, the lit playhead column, the firing bars, and the step numbers
    for (int step = 0; step < length; ++step)
    {
        const auto column = juce::Rectangle<float> (plot.getX() + (float) step * columnWidth, plot.getY(),
                                                    columnWidth, plot.getHeight());
        const bool isPlayhead = step == playheadStep;
        const bool fires = step < (int) firing.size() && firing[(size_t) step];

        g.setColour (Palette::track.withAlpha (0.35f));
        g.drawLine (column.getX(), plot.getY(), column.getX(), plot.getBottom(), 1.0f);

        if (isPlayhead)
        {
            const auto lit = column.reduced (2.0f, 0.0f);
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.22f), lit.getCentreX(), plot.getY(),
                                                     accent.withAlpha (0.02f), lit.getCentreX(), plot.getBottom(), false));
            g.fillRoundedRectangle (lit, 6.0f);
            g.setColour (accent.withAlpha (0.8f));
            g.drawRoundedRectangle (lit, 6.0f, 2.0f);
        }

        if (fires)
        {
            const auto body = column.reduced (columnWidth * 0.18f, 0.0f);
            const float height = plot.getHeight() * 0.20f;
            const juce::Rectangle<float> bar (body.getX(), plot.getBottom() - height, body.getWidth(), height);
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.55f), 0.0f, bar.getY(),
                                                     accent.withAlpha (0.10f), 0.0f, bar.getBottom(), false));
            g.fillRoundedRectangle (bar, 5.0f);
        }
        else
        {
            g.setColour (Palette::track.brighter (0.2f));
            g.fillEllipse (column.getCentreX() - 3.0f, plot.getBottom() - 9.0f, 6.0f, 6.0f);
        }

        g.setColour (isPlayhead ? accent : Palette::textDim);
        g.setFont (juce::Font (14.0f, isPlayhead ? juce::Font::bold : juce::Font::plain));
        g.drawText (juce::String (step + 1),
                    juce::Rectangle<float> (column.getX(), bounds.getBottom() - numberHeight, column.getWidth(), numberHeight),
                    juce::Justification::centred);
    }

    // Recent notes, oldest first: a trail through the notes, then each note as a glowing dot that fades with age
    const int count = (int) recent.size();
    juce::Point<float> previous;
    bool hasPrevious = false;

    for (int i = 0; i < count; ++i)
    {
        const auto& event = recent[(size_t) i];
        if (event.velocity <= 0.0f || event.patternStep < 0 || event.patternStep >= length)
        {
            hasPrevious = false;
            continue;
        }

        const float age = count > 1 ? (float) i / (float) (count - 1) : 1.0f;
        const float alpha = 0.2f + 0.8f * age;
        const juce::Point<float> point (plot.getX() + ((float) event.patternStep + 0.5f) * columnWidth, yFor (event.offsetMs));
        const float radius = 3.5f + event.velocity / 127.0f * 5.5f;

        if (hasPrevious)
        {
            g.setColour (accent.withAlpha (0.3f * alpha));
            g.drawLine (previous.x, previous.y, point.x, point.y, 1.5f);
        }

        g.setColour (accent.withAlpha (0.15f * alpha));
        g.fillEllipse (point.x - radius * 2.2f, point.y - radius * 2.2f, radius * 4.4f, radius * 4.4f);
        g.setColour (accent.withAlpha (alpha));
        g.fillEllipse (point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f * alpha));
        g.fillEllipse (point.x - radius * 0.35f, point.y - radius * 0.55f, radius * 0.5f, radius * 0.5f);

        previous = point;
        hasPrevious = true;
    }
}

//==============================================================================
MicroTimingChaosEditor::MicroTimingChaosEditor (MicroTimingChaosProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      curveEditor (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (editorWidth, editorHeight);
    setScaleFactor (1.0f);   // applies designScale, as the host's scale is 1 at start
    setResizable (true, true);
    // The smallest size is the fixed right column plus the smallest sequencer and display.
    // The smallest size fits the widest row (the melody, chaos and transport groups side by side).
    setResizeLimits (editorWidth, minEditorHeight, 2400, 1600);

    helpTracker.onMove = [this] (const juce::MouseEvent& e) { showHelp (e); };
    addMouseListener (&helpTracker, true);

    helpLabel.setFont (juce::Font (14.0f));
    helpLabel.setColour (juce::Label::textColourId, Palette::text);
    helpLabel.setText ("Hover a control to see what it does.", juce::dontSendNotification);

    readout.setJustificationType (juce::Justification::centredLeft);
    readout.setColour (juce::Label::textColourId, SuiteUI::Colours::text);
    readout.setFont (juce::Font (14.0f, juce::Font::bold));

    hostTempoLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    hostTempoLabel.setColour (juce::Label::textColourId, Palette::textDim);
    hostTempoLabel.setJustificationType (juce::Justification::centred);

    styleCaption (scaleCaption, "SCALE");

    // Knob captions: all the same size and weight
    for (auto* caption : { &lengthSlider.label, &densitySlider.label, &seedSlider.label, &gateSlider.label,
                           &swingSlider.label, &velocitySlider.label, &rootSlider.label, &octavesSlider.label,
                           &chaosSlider.label, &timingRangeSlider.label, &velocityChaosSlider.label,
                           &lerpSlider.label, &lerpXMultSlider.label, &lerpYMultSlider.label, &lerpZMultSlider.label,
                           &tempoSlider.label, &speedSlider.label })
        emphasiseCaption (*caption);

    rateSelector.setFontHeight (15.0f);
    lerpViewSelector.setFontHeight (15.0f);

    for (auto* component : { static_cast<juce::Component*> (&helpLabel), static_cast<juce::Component*> (&attractor),
                              static_cast<juce::Component*> (&readout), static_cast<juce::Component*> (&grid),
                              static_cast<juce::Component*> (&lengthSlider), static_cast<juce::Component*> (&densitySlider),
                              static_cast<juce::Component*> (&seedSlider), static_cast<juce::Component*> (&gateSlider),
                              static_cast<juce::Component*> (&swingSlider), static_cast<juce::Component*> (&velocitySlider),
                              static_cast<juce::Component*> (&rootSlider), static_cast<juce::Component*> (&octavesSlider),
                              static_cast<juce::Component*> (&scaleCaption), static_cast<juce::Component*> (&scaleBox),
                              static_cast<juce::Component*> (&chaosSlider), static_cast<juce::Component*> (&timingRangeSlider),
                              static_cast<juce::Component*> (&velocityChaosSlider),
                              static_cast<juce::Component*> (&lerpViewSelector), static_cast<juce::Component*> (&curveEditor),
                              static_cast<juce::Component*> (&lerpPreview), static_cast<juce::Component*> (&modulation),
                              static_cast<juce::Component*> (&lerpSlider), static_cast<juce::Component*> (&lerpXMultSlider),
                              static_cast<juce::Component*> (&lerpYMultSlider), static_cast<juce::Component*> (&lerpZMultSlider),
                              static_cast<juce::Component*> (&tempoSlider), static_cast<juce::Component*> (&speedSlider),
                              static_cast<juce::Component*> (&rateSelector), static_cast<juce::Component*> (&syncButton),
                              static_cast<juce::Component*> (&hostTempoLabel) })
        addAndMakeVisible (component);

    // Help text for every control, and tooltips on the knobs.
    describe (lengthSlider, "Pattern length in steps, 2 to 16.");
    describe (densitySlider, "How many steps fire. The seed decides which ones.");
    describe (seedSlider, "Seed for the fixed pattern and melody. The same seed gives the same pattern.");
    describe (gateSlider, "Note length as a share of a step. Lorenz Y lengthens or shortens it when Lerp is on.");
    describe (swingSlider, "Delays every second step.");
    describe (velocitySlider, "Base velocity of each note, before chaos.");
    describe (rootSlider, "Lowest melody note, as a MIDI note number.");
    describe (octavesSlider, "How many octaves the melody spans.");
    describe (scaleBox, "The scale the melody is chosen from.");
    describe (chaosSlider, "Chaos, 0 to 100 %. It is the input to the lerp curve. At 0 the pattern never moves.");
    describe (timingRangeSlider, "Maximum timing shift in ms, reached at the attractor's X extremes.");
    describe (velocityChaosSlider, "How strongly the attractor's Z moves velocity.");
    describe (lerpSlider, "How far each step drifts from the fixed pattern toward the attractor. 0 % turns the drift off.");
    describe (lerpXMultSlider, "Timing drift multiplier, 0 to 2x.");
    describe (lerpYMultSlider, "Pitch and gate drift multiplier, 0 to 2x.");
    describe (lerpZMultSlider, "Velocity drift multiplier, 0 to 2x.");
    describe (tempoSlider, "Tempo used when BPM Sync is off.");
    describe (speedSlider, "How fast the attractor moves, relative to the tempo.");
    describe (rateSelector, "Step length: quarter, eighth, sixteenth or thirty-second notes.");
    describe (syncButton, "BPM Sync: follow the host's tempo and transport.");
    curveEditor.setHelpText ("The curve maps chaos (0 to 1) to timing, pitch and velocity drift. "
                             "The multipliers scale each axis. Drag points; right-click a point to reset it, "
                             "double-click to reset the curve.");
    lerpPreview.setHelpText ("Live drift per axis, and the timing, pitch and velocity of the last steps.");
    lerpViewSelector.setHelpText ("Switch between the curve editor and the live preview.");

    rateSelector.setOptions ({ "1/4", "1/8", "1/16", "1/32" });
    rateAttachment = std::make_unique<juce::ParameterAttachment> (
        *processor.apvts.getParameter (MicroTimingChaosProcessor::rateId),
        [this] (float value) { rateSelector.setSelectedIndex (juce::roundToInt (value)); });
    rateSelector.onChange = [this] (int index) { rateAttachment->setValueAsCompleteGesture ((float) index); };
    rateAttachment->sendInitialUpdate();

    lerpViewSelector.setOptions ({ "CURVE", "PREVIEW" });
    lerpViewSelector.setSelectedIndex (0);
    lerpViewSelector.onChange = [this] (int index)
    {
        curveEditor.setVisible (index == 0);
        lerpPreview.setVisible (index == 1);
    };
    lerpPreview.setVisible (false);

    scaleBox.addItemList ({ "Chromatic", "Major", "Minor", "Minor Pentatonic", "Dorian", "Whole Tone" }, 1);
    scaleBox.setTextWhenNothingSelected ("Scale");

    hostTempoLabel.setText ("FREE", juce::dontSendNotification);

    // Units and formatting
    lengthSlider.slider.setNumDecimalPlacesToDisplay (0);
    densitySlider.slider.setTextValueSuffix (" %");
    seedSlider.slider.setNumDecimalPlacesToDisplay (0);
    gateSlider.slider.setTextValueSuffix (" %");
    swingSlider.slider.setTextValueSuffix (" %");
    velocitySlider.slider.setNumDecimalPlacesToDisplay (0);
    rootSlider.slider.setNumDecimalPlacesToDisplay (0);
    octavesSlider.slider.setNumDecimalPlacesToDisplay (0);
    chaosSlider.slider.setTextValueSuffix (" %");
    velocityChaosSlider.slider.setTextValueSuffix (" %");
    lerpSlider.slider.setTextValueSuffix (" %");
    lerpXMultSlider.slider.setTextValueSuffix (" x");
    lerpYMultSlider.slider.setTextValueSuffix (" x");
    lerpZMultSlider.slider.setTextValueSuffix (" x");
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
    bind (MicroTimingChaosProcessor::timingRangeId, timingRangeSlider.slider);
    bind (MicroTimingChaosProcessor::velocityChaosId, velocityChaosSlider.slider);
    bind (MicroTimingChaosProcessor::lerpId, lerpSlider.slider);
    bind (MicroTimingChaosProcessor::lerpXMultId, lerpXMultSlider.slider);
    bind (MicroTimingChaosProcessor::lerpYMultId, lerpYMultSlider.slider);
    bind (MicroTimingChaosProcessor::lerpZMultId, lerpZMultSlider.slider);
    bind (MicroTimingChaosProcessor::bpmId, tempoSlider.slider);
    bind (MicroTimingChaosProcessor::speedId, speedSlider.slider);

    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, MicroTimingChaosProcessor::scaleId, scaleBox);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, MicroTimingChaosProcessor::syncId, syncButton);

    // Each control that the routing can move, and the target it shows. Whole-number targets are rounded by the router.
    using Target = MicroTimingChaosProcessor::ModTarget;
    controls = std::make_unique<SuiteModulation::ControlDisplay> (processor.getRouter(),
        std::vector<SuiteModulation::ControlDisplay::Entry> {
            { chaosSlider.slider,         (int) Target::Chaos },
            { timingRangeSlider.slider,   (int) Target::TimingRange },
            { velocityChaosSlider.slider, (int) Target::VelocityChaos },
            { lerpSlider.slider,          (int) Target::Lerp },
            { lerpXMultSlider.slider,     (int) Target::LerpX },
            { lerpYMultSlider.slider,     (int) Target::LerpY },
            { lerpZMultSlider.slider,     (int) Target::LerpZ },
            { densitySlider.slider,       (int) Target::Density },
            { gateSlider.slider,          (int) Target::Gate },
            { swingSlider.slider,         (int) Target::Swing },
            { velocitySlider.slider,      (int) Target::Velocity },
            { speedSlider.slider,         (int) Target::Speed },
            { tempoSlider.slider,         (int) Target::Tempo },
            { seedSlider.slider,          (int) Target::Seed },
        });

    startTimerHz (60);
}

MicroTimingChaosEditor::~MicroTimingChaosEditor()
{
    stopTimer();
    removeMouseListener (&helpTracker);
    setLookAndFeel (nullptr);
}

void MicroTimingChaosEditor::showHelp (const juce::MouseEvent& e)
{
    if (auto* component = e.eventComponent)
    {
        const auto text = component->getHelpText();

        if (text.isNotEmpty())
            helpLabel.setText (text, juce::dontSendNotification);
    }
}

void MicroTimingChaosEditor::setScaleFactor (float newScale)
{
    AudioProcessorEditor::setScaleFactor (newScale * designScale);
}

void MicroTimingChaosEditor::paint (juce::Graphics& g)
{
    lookAndFeel.paint (g, *this);

    // Title band: a flat background, so the grid texture does not run through the title
    g.setColour (Palette::background);
    g.fillRect (0, 0, getWidth(), titleBandHeight);

    const auto header = getLocalBounds().reduced (groupGap, 0).removeFromTop (titleBandHeight - 8).toFloat();

    g.setColour (lookAndFeel.getAccent());
    g.setFont (juce::Font (26.0f, juce::Font::bold));
    g.drawText ("MICRO-TIMING CHAOS", header, juce::Justification::centredLeft);

    // Every group title has the same colour and size.
    for (const auto& group : groups)
    {
        const auto bounds = group.bounds.toFloat();

        g.setColour (Palette::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (Palette::track);
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

        g.setColour (lookAndFeel.getAccent());
        g.setFont (juce::Font (15.0f, juce::Font::bold));
        g.drawText (group.title, group.bounds.withHeight (groupTitleHeight).translated (0, 4).reduced (12, 0),
                    juce::Justification::centredLeft);
    }
}

void MicroTimingChaosEditor::resized()
{
    groups.clear();

    // Every gap is groupGap: the outer margin, and the space between groups in both directions.
    auto content = getLocalBounds().withTrimmedTop (titleBandHeight).reduced (groupGap);
    helpLabel.setBounds (content.removeFromBottom (helpHeight));
    content.removeFromBottom (groupGap);

    const int x0 = content.getX();
    const int y0 = content.getY();
    const int right = content.getRight();
    const int rightX = right - rightColumnWidth;

    // Row 1: lerp curve on the left, attractor visualisation on the right.
    const juce::Rectangle<int> lerpGroup (x0, y0, leftColumnWidth, lerpGroupHeight);
    const juce::Rectangle<int> attractorGroup (rightX, y0, rightColumnWidth, lerpGroupHeight);

    auto attractorInner = innerOf (attractorGroup);
    readout.setBounds (attractorInner.removeFromBottom (readoutHeight));
    attractor.setBounds (attractorInner);

    // Row 2: pattern on the left, LFO on the right, both the same height.
    const int row2Y = lerpGroup.getBottom() + groupGap;
    const juce::Rectangle<int> patternGroup (x0, row2Y, leftColumnWidth, rowTwoHeight);
    const juce::Rectangle<int> lfoGroup (rightX, row2Y, rightColumnWidth, rowTwoHeight);

    // Row 3: melody and chaos on the left, transport on the right.
    const int row3Y = patternGroup.getBottom() + groupGap;
    const juce::Rectangle<int> melodyGroup (x0, row3Y, melodyGroupWidth, lowerGroupHeight);
    const juce::Rectangle<int> chaosGroup (melodyGroup.getRight() + groupGap, row3Y, chaosGroupWidth, lowerGroupHeight);
    const juce::Rectangle<int> transportGroup (rightX, row3Y, rightColumnWidth, lowerGroupHeight);

    // Row 4: the sequencer across the full width, taking all remaining height.
    const int row4Y = transportGroup.getBottom() + groupGap;
    const juce::Rectangle<int> sequencerGroup (x0, row4Y, content.getWidth(), content.getBottom() - row4Y);

    groups.push_back ({ "LERP CURVE", lerpGroup });
    groups.push_back ({ "ATTRACTOR VISUALIZATION", attractorGroup });
    groups.push_back ({ "PATTERN", patternGroup });
    groups.push_back ({ "LFO", lfoGroup });
    groups.push_back ({ "MELODY", melodyGroup });
    groups.push_back ({ "CHAOS", chaosGroup });
    groups.push_back ({ "TRANSPORT", transportGroup });
    groups.push_back ({ "SEQUENCER", sequencerGroup });

    modulation.setBounds (innerOf (lfoGroup));
    grid.setBounds (innerOf (sequencerGroup));

    // Pattern: 3 x 2
    const auto patternCells = gridCells (innerOf (patternGroup), 6, 3);
    placeKnob (lengthSlider, patternCells[0]);
    placeKnob (densitySlider, patternCells[1]);
    placeKnob (seedSlider, patternCells[2]);
    placeKnob (gateSlider, patternCells[3]);
    placeKnob (swingSlider, patternCells[4]);
    placeKnob (velocitySlider, patternCells[5]);

    // Melody: root with its note name under it, octaves, and the scale box
    const auto melodyCells = gridCells (innerOf (melodyGroup), 3, 3);
    placeKnob (rootSlider, melodyCells[0]);
    placeKnob (octavesSlider, melodyCells[1]);
    auto scaleCell = melodyCells[2];
    scaleCaption.setBounds (scaleCell.removeFromTop (18));
    scaleBox.setBounds (scaleCell.withSizeKeepingCentre (scaleCell.getWidth(), 30));

    // Chaos
    const auto chaosCells = gridCells (innerOf (chaosGroup), 3, 3);
    placeKnob (chaosSlider, chaosCells[0]);
    placeKnob (timingRangeSlider, chaosCells[1]);
    placeKnob (velocityChaosSlider, chaosCells[2]);

    // Lerp curve: the tabbed view on the left, the four knobs on the right
    auto lerpInner = innerOf (lerpGroup);
    auto lerpKnobs = lerpInner.removeFromRight (juce::roundToInt ((float) lerpInner.getWidth() * 0.40f));
    lerpViewSelector.setBounds (lerpInner.removeFromTop (34).withWidth (240));
    lerpInner.removeFromTop (8);
    curveEditor.setBounds (lerpInner);
    lerpPreview.setBounds (lerpInner);

    const auto lerpCells = gridCells (lerpKnobs, 4, 2);
    placeKnob (lerpSlider, lerpCells[0]);
    placeKnob (lerpXMultSlider, lerpCells[1]);
    placeKnob (lerpYMultSlider, lerpCells[2]);
    placeKnob (lerpZMultSlider, lerpCells[3]);

    // Transport: tempo and speed knobs on the left; on the right, the rate selector, then BPM Sync with its host tempo
    auto transportInner = innerOf (transportGroup);
    auto knobArea = transportInner.removeFromLeft (300);
    placeKnob (tempoSlider, knobArea.removeFromLeft (150));
    placeKnob (speedSlider, knobArea);
    transportInner.removeFromLeft (groupGap);

    rateSelector.setBounds (transportInner.removeFromTop (38).withSizeKeepingCentre (400, 38));
    transportInner.removeFromTop (groupGap);
    syncButton.setBounds (transportInner.removeFromTop (38).withSizeKeepingCentre (280, 38));
    hostTempoLabel.setBounds (transportInner.removeFromTop (24).withSizeKeepingCentre (280, 24));
}

SuiteModulation::ModulationPanel::Style MicroTimingChaosEditor::modulationStyle()
{
    namespace Palette = MicroTimingChaos::Palette;

    return { { Palette::lfo1, Palette::lfo2 },
             Palette::text, Palette::textDim, Palette::panel, Palette::track };
}

void MicroTimingChaosEditor::timerCallback()
{
    attractor.setTrail (processor.getTrail());

    const auto& apvts = processor.apvts;
    const int lengthValue = juce::roundToInt (apvts.getRawParameterValue (MicroTimingChaosProcessor::lengthId)->load());
    // The pattern shown is the one the sequencer plays: the modulated seed and density, with the same density curve.
    using Target = MicroTimingChaosProcessor::ModTarget;
    const auto seedValue = (std::uint32_t) juce::roundToInt (processor.getModulatedValue (Target::Seed, MicroTimingChaosProcessor::seedId));
    const float densityValue = CoreDSP::Sequencer::densityCurve (
        processor.getModulatedValue (Target::Density, MicroTimingChaosProcessor::densityId) / 100.0f);
    const float timingValue = processor.getModulatedValue (Target::TimingRange, MicroTimingChaosProcessor::timingRangeId);
    const int rootValue = juce::roundToInt (apvts.getRawParameterValue (MicroTimingChaosProcessor::rootId)->load());

    std::vector<bool> fires;
    for (int step = 0; step < CoreDSP::Sequencer::maxSteps; ++step)
        fires.push_back (step < lengthValue && CoreDSP::Sequencer::stepFires (seedValue, step, densityValue));

    const int playhead = processor.getPlayheadStep();
    grid.setState (lengthValue, playhead, timingValue, fires, processor.getRecentSteps());

    readout.setText (playhead >= 0 ? "Playhead: step " + juce::String (playhead + 1) + " of " + juce::String (lengthValue)
                                   : juce::String ("Transport stopped. Press play, or turn off BPM Sync."),
                      juce::dontSendNotification);

    // The marker follows the modulated chaos, so it matches what the sequencer uses.
    curveEditor.setChaosValue (processor.getModulatedValue (MicroTimingChaosProcessor::ModTarget::Chaos, MicroTimingChaosProcessor::chaosId) / 100.0f);
    controls->update();
    lerpPreview.refresh (processor.getRecentSteps(), processor.getDriftAmounts(), rootValue, timingValue);

    // Tempo: follows the host when BPM Sync is on
    modulation.refresh();

    const bool synced = apvts.getRawParameterValue (MicroTimingChaosProcessor::syncId)->load() > 0.5f;
    tempoSlider.setEnabled (! synced);

    if (synced)
    {
        const auto hostBpm = processor.getHostBpm();
        hostTempoLabel.setText (hostBpm.hasValue() ? "HOST " + juce::String (*hostBpm, 1) + " BPM"
                                                   : juce::String ("NO HOST TEMPO"),
                                juce::dontSendNotification);
    }
    else
    {
        hostTempoLabel.setText ("FREE RUNNING", juce::dontSendNotification);
    }

    // Middle C is C3 here, so MIDI 60 reads C3.
}
