#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"

class MicroTimingSeqEditor : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit MicroTimingSeqEditor (MicroTimingSeqProcessor&);
    ~MicroTimingSeqEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MicroTimingSeqProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff7dff5a) };

    SuiteUI::TrailPlot attractor { juce::Colour (0xff7dff5a) };
    juce::Label readout;
    SuiteUI::LabelledSlider intensitySlider { "CHAOS INTENSITY", juce::Slider::LinearHorizontal };
    SuiteUI::LabelledSlider offsetSlider { "TIMING OFFSET MAX", juce::Slider::LinearHorizontal };
    SuiteUI::LabelledSlider velocitySlider { "VELOCITY DRIFT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::SegmentedSelector syncSelector { juce::Colour (0xff7dff5a) };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> intensityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> offsetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velocityAttachment;
    std::unique_ptr<juce::ParameterAttachment> syncAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicroTimingSeqEditor)
};
