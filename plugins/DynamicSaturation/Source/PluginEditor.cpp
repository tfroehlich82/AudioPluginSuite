#include "PluginEditor.h"

namespace
{
    constexpr int editorWidth = 640;
    constexpr int editorHeight = 460;
}

DynamicSaturationEditor::DynamicSaturationEditor (DynamicSaturationProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (480, 360, 1280, 900);

    addAndMakeVisible (transferCurve);
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (modeSelector);
    addAndMakeVisible (driveSlider);
    addAndMakeVisible (sensitivitySlider);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (outputSlider);

    driveSlider.slider.setTextValueSuffix (" dB");
    outputSlider.slider.setTextValueSuffix (" dB");
    mixSlider.slider.setTextValueSuffix (" %");
    sensitivitySlider.slider.setNumDecimalPlacesToDisplay (2);

    auto& apvts = processor.apvts;

    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, DynamicSaturationProcessor::driveId, driveSlider.slider);
    sensitivityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, DynamicSaturationProcessor::sensitivityId, sensitivitySlider.slider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, DynamicSaturationProcessor::mixId, mixSlider.slider);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, DynamicSaturationProcessor::outputLevelId, outputSlider.slider);

    // The mode switch mirrors a choice parameter: segment index equals the parameter's choice index.
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
    g.drawText ("Transfer curve tracks the input level", getLocalBounds().reduced (16, 0).removeFromTop (30),
                juce::Justification::centredRight);
}

void DynamicSaturationEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (34);

    auto curveRow = area.removeFromTop ((int) ((float) area.getHeight() * 0.5f));
    inputMeter.setBounds (curveRow.removeFromRight (28).reduced (4, 6));
    transferCurve.setBounds (curveRow.reduced (4, 0));

    area.removeFromTop (10);
    modeSelector.setBounds (area.removeFromTop (30).reduced (60, 0));
    area.removeFromTop (10);

    const int knobWidth = area.getWidth() / 4;
    driveSlider.setBounds (area.removeFromLeft (knobWidth).reduced (4));
    sensitivitySlider.setBounds (area.removeFromLeft (knobWidth).reduced (4));
    mixSlider.setBounds (area.removeFromLeft (knobWidth).reduced (4));
    outputSlider.setBounds (area.reduced (4));
}

void DynamicSaturationEditor::timerCallback()
{
    const auto& apvts = processor.apvts;

    const float env = processor.getEnvelopeLevel();
    const float baseGain = CoreDSP::decibelsToGain (apvts.getRawParameterValue (DynamicSaturationProcessor::driveId)->load());
    const float sens = apvts.getRawParameterValue (DynamicSaturationProcessor::sensitivityId)->load();
    const auto shaping = static_cast<CoreDSP::Saturation::Mode> (juce::roundToInt (
        apvts.getRawParameterValue (DynamicSaturationProcessor::modeId)->load()));

    // Draw the curve at the drive the processor is using right now.
    const float preGain = CoreDSP::Saturation::dynamicDrive (baseGain, sens, env);

    inputMeter.setLevel (env);
    transferCurve.setCurve ([preGain, shaping] (float x) { return CoreDSP::Saturation::shape (x * preGain, shaping); },
                            env);
}
