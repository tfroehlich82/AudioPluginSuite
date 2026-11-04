#include "PluginEditor.h"

namespace
{
    constexpr int editorWidth = 880;
    constexpr int editorHeight = 620;

    // Places components side by side in equal-width cells.
    void layoutRow (juce::Rectangle<int> row, std::initializer_list<juce::Component*> cells)
    {
        const int width = row.getWidth() / (int) cells.size();

        for (auto* cell : cells)
            cell->setBounds (row.removeFromLeft (width).reduced (4));
    }
}

DynamicSaturationEditor::DynamicSaturationEditor (DynamicSaturationProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (720, 540, 1400, 1000);

    for (auto* component : { static_cast<juce::Component*> (&transferCurve),
                             static_cast<juce::Component*> (&inputMeter),
                             static_cast<juce::Component*> (&modeSelector),
                             static_cast<juce::Component*> (&driveSlider),
                             static_cast<juce::Component*> (&sensitivitySlider),
                             static_cast<juce::Component*> (&curveSlider),
                             static_cast<juce::Component*> (&attackSlider),
                             static_cast<juce::Component*> (&releaseSlider),
                             static_cast<juce::Component*> (&mixSlider),
                             static_cast<juce::Component*> (&detectHpfSlider),
                             static_cast<juce::Component*> (&stereoLinkSlider),
                             static_cast<juce::Component*> (&biasSlider),
                             static_cast<juce::Component*> (&foldSlider),
                             static_cast<juce::Component*> (&toneSlider),
                             static_cast<juce::Component*> (&outputSlider) })
        addAndMakeVisible (component);

    driveSlider.slider.setTextValueSuffix (" dB");
    sensitivitySlider.slider.setNumDecimalPlacesToDisplay (2);
    curveSlider.slider.setNumDecimalPlacesToDisplay (2);
    attackSlider.slider.setTextValueSuffix (" ms");
    releaseSlider.slider.setTextValueSuffix (" ms");
    mixSlider.slider.setTextValueSuffix (" %");
    detectHpfSlider.slider.setTextValueSuffix (" Hz");
    stereoLinkSlider.slider.setTextValueSuffix (" %");
    biasSlider.slider.setNumDecimalPlacesToDisplay (2);
    foldSlider.slider.setTextValueSuffix (" %");
    toneSlider.slider.setNumDecimalPlacesToDisplay (2);
    outputSlider.slider.setTextValueSuffix (" dB");

    auto& apvts = processor.apvts;

    auto bind = [this, &apvts] (const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider));
    };

    bind (DynamicSaturationProcessor::driveId, driveSlider.slider);
    bind (DynamicSaturationProcessor::sensitivityId, sensitivitySlider.slider);
    bind (DynamicSaturationProcessor::curveId, curveSlider.slider);
    bind (DynamicSaturationProcessor::attackId, attackSlider.slider);
    bind (DynamicSaturationProcessor::releaseId, releaseSlider.slider);
    bind (DynamicSaturationProcessor::mixId, mixSlider.slider);
    bind (DynamicSaturationProcessor::detectHpfId, detectHpfSlider.slider);
    bind (DynamicSaturationProcessor::stereoLinkId, stereoLinkSlider.slider);
    bind (DynamicSaturationProcessor::biasId, biasSlider.slider);
    bind (DynamicSaturationProcessor::foldId, foldSlider.slider);
    bind (DynamicSaturationProcessor::toneId, toneSlider.slider);
    bind (DynamicSaturationProcessor::outputLevelId, outputSlider.slider);

    // The mode switch mirrors a choice parameter: the segment index equals the choice index.
    modeSelector.setOptions ({ "TUBE", "TAPE", "DIODE" });
    modeAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter (DynamicSaturationProcessor::modeId),
        [this] (float value) { modeSelector.setSelectedIndex (juce::roundToInt (value)); });
    modeSelector.onChange = [this] (int index) { modeAttachment->setValueAsCompleteGesture ((float) index); };
    modeAttachment->sendInitialUpdate();

    startTimerHz (60);
}

DynamicSaturationEditor::~DynamicSaturationEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DynamicSaturationEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (SuiteUI::Colours::panelLight, 0.0f, 0.0f,
                                     SuiteUI::Colours::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    g.setColour (lookAndFeel.getAccent());
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText ("DYNAMIC SATURATION", getLocalBounds().reduced (16, 0).removeFromTop (30),
                juce::Justification::centredLeft);

    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (juce::Font (12.0f));
    g.drawText ("2x oversampled  |  transfer curve tracks the input level",
                getLocalBounds().reduced (16, 0).removeFromTop (30), juce::Justification::centredRight);
}

void DynamicSaturationEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (34);

    auto curveRow = area.removeFromTop ((int) ((float) area.getHeight() * 0.42f));
    inputMeter.setBounds (curveRow.removeFromRight (28).reduced (4, 6));
    transferCurve.setBounds (curveRow.reduced (4, 0));

    area.removeFromTop (8);
    modeSelector.setBounds (area.removeFromTop (30).reduced (120, 0));
    area.removeFromTop (8);

    const int rowHeight = area.getHeight() / 2;

    layoutRow (area.removeFromTop (rowHeight),
               { &driveSlider, &sensitivitySlider, &curveSlider, &attackSlider, &releaseSlider, &mixSlider });

    layoutRow (area,
               { &detectHpfSlider, &stereoLinkSlider, &biasSlider, &foldSlider, &toneSlider, &outputSlider });
}

void DynamicSaturationEditor::timerCallback()
{
    const auto& apvts = processor.apvts;

    const float env = processor.getEnvelopeLevel();
    const float baseGain = CoreDSP::decibelsToGain (apvts.getRawParameterValue (DynamicSaturationProcessor::driveId)->load());
    const float sens = apvts.getRawParameterValue (DynamicSaturationProcessor::sensitivityId)->load();
    const float curveValue = apvts.getRawParameterValue (DynamicSaturationProcessor::curveId)->load();
    const float biasValue = apvts.getRawParameterValue (DynamicSaturationProcessor::biasId)->load();
    const float foldAmount = apvts.getRawParameterValue (DynamicSaturationProcessor::foldId)->load() / 100.0f;
    const auto shaping = static_cast<CoreDSP::Saturation::Mode> (juce::roundToInt (
        apvts.getRawParameterValue (DynamicSaturationProcessor::modeId)->load()));

    // Draw the curve at the drive the processor is using right now.
    const float preGain = CoreDSP::Saturation::dynamicDrive (baseGain, sens,
                                                             CoreDSP::Saturation::shapeEnvelope (env, curveValue));

    inputMeter.setLevel (env);
    transferCurve.setCurve ([preGain, shaping, biasValue, foldAmount] (float x)
                            {
                                const float shaped = CoreDSP::Saturation::shape (x * preGain, shaping, biasValue);
                                return CoreDSP::Saturation::fold (shaped, foldAmount);
                            },
                            env);
}
