#include "PluginEditor.h"

#include <cmath>

namespace
{
    constexpr int editorWidth = 680;
    constexpr int editorHeight = 420;
}

MicroTimingSeqEditor::MicroTimingSeqEditor (MicroTimingSeqProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (520, 340, 1280, 900);

    readout.setJustificationType (juce::Justification::centredLeft);
    readout.setColour (juce::Label::textColourId, SuiteUI::Colours::text);
    readout.setFont (juce::Font (14.0f));

    addAndMakeVisible (attractor);
    addAndMakeVisible (readout);
    addAndMakeVisible (intensitySlider);
    addAndMakeVisible (offsetSlider);
    addAndMakeVisible (velocitySlider);
    addAndMakeVisible (syncSelector);

    intensitySlider.slider.setTextValueSuffix (" %");
    offsetSlider.slider.setTextValueSuffix (" ms");
    velocitySlider.slider.setTextValueSuffix (" %");

    auto& apvts = processor.apvts;

    intensityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MicroTimingSeqProcessor::chaosIntensityId, intensitySlider.slider);
    offsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MicroTimingSeqProcessor::timingOffsetId, offsetSlider.slider);
    velocityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MicroTimingSeqProcessor::velocityDriftId, velocitySlider.slider);

    syncSelector.setOptions ({ "FREE", "HOST SYNC" });
    syncAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter (MicroTimingSeqProcessor::bpmSyncId),
        [this] (float value) { syncSelector.setSelectedIndex (juce::roundToInt (value)); });
    syncSelector.onChange = [this] (int index) { syncAttachment->setValueAsCompleteGesture ((float) index); };
    syncAttachment->sendInitialUpdate();

    startTimerHz (60);
}

MicroTimingSeqEditor::~MicroTimingSeqEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MicroTimingSeqEditor::paint (juce::Graphics& g)
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
    g.drawText ("Lorenz X: timing  |  Lorenz Z: velocity", getLocalBounds().reduced (16, 0).removeFromTop (30),
                juce::Justification::centredRight);
}

void MicroTimingSeqEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (34);

    attractor.setBounds (area.removeFromLeft ((int) ((float) area.getWidth() * 0.55f)));
    area.removeFromLeft (16);

    readout.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    syncSelector.setBounds (area.removeFromTop (30).reduced (20, 0));
    area.removeFromTop (12);

    intensitySlider.setBounds (area.removeFromTop (60));
    offsetSlider.setBounds (area.removeFromTop (60));
    velocitySlider.setBounds (area.reduced (40, 0));
}

void MicroTimingSeqEditor::timerCallback()
{
    attractor.setTrail (processor.getTrail());

    const float offsetMs = processor.getLastTimingOffsetMs();
    readout.setText ("Last note offset: " + juce::String (offsetMs, 1) + " ms", juce::dontSendNotification);
}
