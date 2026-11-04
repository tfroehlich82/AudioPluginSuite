#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"

class SpectralDriftEditor : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit SpectralDriftEditor (SpectralDriftProcessor&);
    ~SpectralDriftEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    SpectralDriftProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff2de2ff) };

    SuiteUI::SpectrumDisplay spectrum { juce::Colour (0xffa9b7c9), juce::Colour (0xff2de2ff) };

    // Drift
    SuiteUI::LabelledSlider rateSlider { "DRIFT RATE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider depthSlider { "DRIFT DEPTH", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider spreadSlider { "BIN SPREAD", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider stereoSlider { "STEREO PHASE", juce::Slider::RotaryVerticalDrag };

    // Spectral shaping
    SuiteUI::LabelledSlider shiftSlider { "SHIFT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider tiltSlider { "TILT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider gateSlider { "GATE", juce::Slider::RotaryVerticalDrag };
    juce::Label shapeCaption;
    SuiteUI::SegmentedSelector shapeSelector { juce::Colour (0xff2de2ff) };

    // Feedback and mix
    SuiteUI::LabelledSlider feedbackSlider { "FEEDBACK", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider dampingSlider { "FB DAMPING", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider driftStartSlider { "DRIFT START", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider outputSlider { "OUTPUT GAIN", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider mixSlider { "MIX", juce::Slider::RotaryVerticalDrag };
    juce::ToggleButton freezeButton { "FREEZE" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    std::unique_ptr<juce::ParameterAttachment> shapeAttachment;

    std::vector<float> inputMagnitudes;
    std::vector<float> outputMagnitudes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralDriftEditor)
};
