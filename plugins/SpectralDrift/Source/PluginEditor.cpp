#include "PluginEditor.h"

namespace
{
    constexpr int editorWidth = 640;
    constexpr int editorHeight = 440;
    constexpr double fallbackSampleRate = 48000.0;
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
    setResizeLimits (480, 340, 1280, 900);

    addAndMakeVisible (spectrum);
    addAndMakeVisible (rateSlider);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (feedbackSlider);
    addAndMakeVisible (freezeButton);

    rateSlider.slider.setTextValueSuffix (" Hz");
    rateSlider.slider.setNumDecimalPlacesToDisplay (2);
    mixSlider.slider.setTextValueSuffix (" %");
    feedbackSlider.slider.setTextValueSuffix (" %");

    auto& apvts = processor.apvts;

    rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, SpectralDriftProcessor::driftRateId, rateSlider.slider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, SpectralDriftProcessor::mixId, mixSlider.slider);
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, SpectralDriftProcessor::feedbackId, feedbackSlider.slider);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, SpectralDriftProcessor::freezeId, freezeButton);

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
    g.drawText ("FFT 2048 / 75% overlap", getLocalBounds().reduced (16, 0).removeFromTop (30),
                juce::Justification::centredRight);
}

void SpectralDriftEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (34);

    const int spectrumHeight = (int) ((float) area.getHeight() * 0.55f);
    spectrum.setBounds (area.removeFromTop (spectrumHeight));
    area.removeFromTop (12);

    auto freezeArea = area.removeFromRight (120).reduced (12, 40);
    freezeButton.setBounds (freezeArea);

    const int knobWidth = area.getWidth() / 3;
    rateSlider.setBounds (area.removeFromLeft (knobWidth).reduced (4));
    mixSlider.setBounds (area.removeFromLeft (knobWidth).reduced (4));
    feedbackSlider.setBounds (area.reduced (4));
}

void SpectralDriftEditor::timerCallback()
{
    const auto& snapshot = processor.getSpectrumSnapshot();

    for (int k = 0; k < CoreDSP::Spectral::numBins; ++k)
    {
        inputMagnitudes[(size_t) k] = snapshot.input[(size_t) k].load (std::memory_order_relaxed);
        outputMagnitudes[(size_t) k] = snapshot.output[(size_t) k].load (std::memory_order_relaxed);
    }

    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : fallbackSampleRate;
    const double binWidth = sampleRate / (double) CoreDSP::Spectral::frameSize;

    // Scales a full-scale Hann-windowed sine to about -6 dB on the display.
    const float magnitudeScale = 2.0f / (float) CoreDSP::Spectral::frameSize;

    spectrum.setSpectra (inputMagnitudes.data(), outputMagnitudes.data(),
                         CoreDSP::Spectral::numBins, binWidth, magnitudeScale);
}
