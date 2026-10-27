#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

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

    SuiteUI::LabelledSlider driveSlider { "DRIVE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider sensitivitySlider { "SENSITIVITY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider mixSlider { "MIX", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider outputSlider { "OUTPUT", juce::Slider::RotaryVerticalDrag };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensitivityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicSaturationEditor)
};
