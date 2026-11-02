#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"

class DynamicSaturationEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit DynamicSaturationEditor (DynamicSaturationProcessor&);
    ~DynamicSaturationEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    DynamicSaturationProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xffff9f1c) };

    SuiteUI::CurveView transferCurve { juce::Colour (0xffff9f1c) };
    SuiteUI::LevelMeter inputMeter { juce::Colour (0xffff9f1c) };
    SuiteUI::SegmentedSelector modeSelector { juce::Colour (0xffff9f1c) };

    // Row 1: drive and dynamics
    SuiteUI::LabelledSlider driveSlider { "DRIVE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider sensitivitySlider { "SENSITIVITY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider curveSlider { "CURVE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider attackSlider { "ATTACK", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider releaseSlider { "RELEASE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider mixSlider { "MIX", juce::Slider::RotaryVerticalDrag };

    // Row 2: detector, character and output
    SuiteUI::LabelledSlider detectHpfSlider { "DETECT HPF", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider stereoLinkSlider { "STEREO LINK", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider biasSlider { "TUBE BIAS", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider foldSlider { "FOLD", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider toneSlider { "TONE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider outputSlider { "OUTPUT", juce::Slider::RotaryVerticalDrag };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicSaturationEditor)
};
