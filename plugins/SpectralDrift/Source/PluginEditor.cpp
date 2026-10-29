#include "PluginEditor.h"

namespace
{
    constexpr int editorWidth = 780;
    constexpr int editorHeight = 740;
    constexpr double fallbackSampleRate = 48000.0;
    constexpr float signalThreshold = 1.0e-4f;

    // Places components side by side in equal-width cells.
    void layoutRow (juce::Rectangle<int> row, std::initializer_list<juce::Component*> cells)
    {
        const int width = row.getWidth() / (int) cells.size();

        for (auto* cell : cells)
            cell->setBounds (row.removeFromLeft (width).reduced (4));
    }
}

SpectralDriftEditor::SpectralDriftEditor (SpectralDriftProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      inputMagnitudes (CoreDSP::Spectral::numBins, 0.0f),
      outputMagnitudes (CoreDSP::Spectral::numBins, 0.0f)
{
    setLookAndFeel (&lookAndFeel);
    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (640, 620, 1400, 1100);

    for (auto* component : { static_cast<juce::Component*> (&spectrum),
                             static_cast<juce::Component*> (&rateSlider),
                             static_cast<juce::Component*> (&depthSlider),
                             static_cast<juce::Component*> (&spreadSlider),
                             static_cast<juce::Component*> (&stereoSlider),
                             static_cast<juce::Component*> (&shiftSlider),
                             static_cast<juce::Component*> (&tiltSlider),
                             static_cast<juce::Component*> (&gateSlider),
                             static_cast<juce::Component*> (&shapeCaption),
                             static_cast<juce::Component*> (&shapeSelector),
                             static_cast<juce::Component*> (&feedbackSlider),
                             static_cast<juce::Component*> (&dampingSlider),
                             static_cast<juce::Component*> (&driftStartSlider),
                             static_cast<juce::Component*> (&outputSlider),
                             static_cast<juce::Component*> (&mixSlider),
                             static_cast<juce::Component*> (&freezeButton) })
        addAndMakeVisible (component);

    rateSlider.slider.setTextValueSuffix (" Hz");
    rateSlider.slider.setNumDecimalPlacesToDisplay (2);
    depthSlider.slider.setTextValueSuffix (" %");
    spreadSlider.slider.setNumDecimalPlacesToDisplay (3);
    stereoSlider.slider.setTextValueSuffix (" deg");
    shiftSlider.slider.setTextValueSuffix (" bins");
    shiftSlider.slider.setNumDecimalPlacesToDisplay (1);
    tiltSlider.slider.setNumDecimalPlacesToDisplay (2);
    gateSlider.slider.setTextValueSuffix (" dB");
    feedbackSlider.slider.setTextValueSuffix (" %");
    feedbackSlider.slider.setNumDecimalPlacesToDisplay (1);
    dampingSlider.slider.setTextValueSuffix (" %");
    driftStartSlider.slider.setTextValueSuffix (" %");
    outputSlider.slider.setTextValueSuffix (" dB");
    outputSlider.slider.setNumDecimalPlacesToDisplay (1);
    mixSlider.slider.setTextValueSuffix (" %");

    shapeCaption.setText ("LFO SHAPE", juce::dontSendNotification);
    shapeCaption.setJustificationType (juce::Justification::centred);
    shapeCaption.setFont (juce::Font (12.0f, juce::Font::bold));

    auto& apvts = processor.apvts;

    auto bind = [this, &apvts] (const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider));
    };

    bind (SpectralDriftProcessor::driftRateId, rateSlider.slider);
    bind (SpectralDriftProcessor::depthId, depthSlider.slider);
    bind (SpectralDriftProcessor::spreadId, spreadSlider.slider);
    bind (SpectralDriftProcessor::stereoPhaseId, stereoSlider.slider);
    bind (SpectralDriftProcessor::shiftId, shiftSlider.slider);
    bind (SpectralDriftProcessor::tiltId, tiltSlider.slider);
    bind (SpectralDriftProcessor::gateId, gateSlider.slider);
    bind (SpectralDriftProcessor::feedbackId, feedbackSlider.slider);
    bind (SpectralDriftProcessor::feedbackDampingId, dampingSlider.slider);
    bind (SpectralDriftProcessor::driftStartId, driftStartSlider.slider);
    bind (SpectralDriftProcessor::outputGainId, outputSlider.slider);
    bind (SpectralDriftProcessor::mixId, mixSlider.slider);

    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, SpectralDriftProcessor::freezeId, freezeButton);

    // The shape switch mirrors a choice parameter: the segment index equals the choice index.
    shapeSelector.setOptions ({ "SINE", "TRI", "SAW", "SQUARE" });
    shapeAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter (SpectralDriftProcessor::lfoShapeId),
        [this] (float value) { shapeSelector.setSelectedIndex (juce::roundToInt (value)); });
    shapeSelector.onChange = [this] (int index) { shapeAttachment->setValueAsCompleteGesture ((float) index); };
    shapeAttachment->sendInitialUpdate();

    startTimerHz (60);
}

SpectralDriftEditor::~SpectralDriftEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SpectralDriftEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (SuiteUI::Colours::panelLight, 0.0f, 0.0f,
                                     SuiteUI::Colours::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    g.setColour (lookAndFeel.getAccent());
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText ("SPECTRAL DRIFT", getLocalBounds().reduced (16, 0).removeFromTop (30),
                juce::Justification::centredLeft);

    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (juce::Font (12.0f));
    g.drawText ("FFT 2048 / 75 % overlap / 2048 samples latency", getLocalBounds().reduced (16, 0).removeFromTop (30),
                juce::Justification::centredRight);
}

void SpectralDriftEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (34);

    spectrum.setBounds (area.removeFromTop ((int) ((float) area.getHeight() * 0.34f)));
    area.removeFromTop (8);

    const int rowHeight = area.getHeight() / 4;

    // Drift
    layoutRow (area.removeFromTop (rowHeight), { &rateSlider, &depthSlider, &spreadSlider, &stereoSlider });

    // Spectral shaping: the last cell holds the LFO shape caption and switch.
    auto shapingRow = area.removeFromTop (rowHeight);
    const int cellWidth = shapingRow.getWidth() / 4;
    shiftSlider.setBounds (shapingRow.removeFromLeft (cellWidth).reduced (4));
    tiltSlider.setBounds (shapingRow.removeFromLeft (cellWidth).reduced (4));
    gateSlider.setBounds (shapingRow.removeFromLeft (cellWidth).reduced (4));

    auto shapeCell = shapingRow.reduced (8, 4);
    shapeCaption.setBounds (shapeCell.removeFromTop (20));
    shapeCell.removeFromTop (12);
    shapeSelector.setBounds (shapeCell.withSizeKeepingCentre (shapeCell.getWidth(), juce::jmin (30, shapeCell.getHeight())));

    // Feedback, damping, mix and freeze.
    auto feedbackRow = area.removeFromTop (rowHeight);
    auto freezeCell = feedbackRow.removeFromRight (feedbackRow.getWidth() / 4);
    layoutRow (feedbackRow, { &feedbackSlider, &dampingSlider, &mixSlider });
    freezeButton.setBounds (freezeCell.withSizeKeepingCentre (juce::jmin (180, freezeCell.getWidth()), 44));

    // Drift start and output gain.
    layoutRow (area, { &driftStartSlider, &outputSlider });
}

void SpectralDriftEditor::timerCallback()
{
    const auto& snapshot = processor.getSpectrumSnapshot();
    float peak = 0.0f;

    for (int k = 0; k < CoreDSP::Spectral::numBins; ++k)
    {
        const float in = snapshot.input[(size_t) k].load (std::memory_order_relaxed);
        inputMagnitudes[(size_t) k] = in;
        outputMagnitudes[(size_t) k] = snapshot.output[(size_t) k].load (std::memory_order_relaxed);
        peak = juce::jmax (peak, in);
    }

    const auto frames = snapshot.frames.load (std::memory_order_relaxed);
    const bool signal = frames > 0 && peak > signalThreshold;
    spectrum.setSignalPresent (signal);

    // Say which of the two causes applies, so a silent preview can be diagnosed from the screen alone.
    if (frames == 0)
        spectrum.setWaitingMessage ("No audio is reaching the plugin yet. Check that it is on an audible track or the Standalone input.");
    else
        spectrum.setWaitingMessage ("Audio is arriving, but the input is silent. Play or route a signal into the plugin.");

    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : fallbackSampleRate;
    const double binWidth = sampleRate / (double) CoreDSP::Spectral::frameSize;

    // Scales a full-scale Hann-windowed sine to about -6 dB on the display.
    const float magnitudeScale = 2.0f / (float) CoreDSP::Spectral::frameSize;

    spectrum.setSpectra (inputMagnitudes.data(), outputMagnitudes.data(),
                         CoreDSP::Spectral::numBins, binWidth, magnitudeScale);
}
