#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"

/** Step grid: the pattern, the playhead, and where each recent note landed in time. */
class StepGridView : public juce::Component
{
public:
    explicit StepGridView (juce::Colour accentColour);

    void setState (int patternLength, int playhead, float timingRangeMs, std::vector<bool> fires,
                   std::vector<MicroTimingChaosProcessor::StepEvent> events);

    void paint (juce::Graphics&) override;

private:
    juce::Colour accent;
    int length = 8;
    int playheadStep = -1;
    float rangeMs = 20.0f;
    std::vector<bool> firing;
    std::vector<MicroTimingChaosProcessor::StepEvent> recent;
};

class MicroTimingChaosEditor : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit MicroTimingChaosEditor (MicroTimingChaosProcessor&);
    ~MicroTimingChaosEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MicroTimingChaosProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff7dff5a) };

    SuiteUI::TrailPlot attractor { juce::Colour (0xff7dff5a) };
    StepGridView grid { juce::Colour (0xff7dff5a) };
    juce::Label readout;

    SuiteUI::SegmentedSelector rateSelector { juce::Colour (0xff7dff5a) };
    juce::ComboBox scaleBox;
    juce::ToggleButton syncButton { "HOST SYNC" };

    // Pattern
    SuiteUI::LabelledSlider lengthSlider { "LENGTH", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider densitySlider { "DENSITY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider seedSlider { "SEED", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider gateSlider { "GATE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider swingSlider { "SWING", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider velocitySlider { "VELOCITY", juce::Slider::RotaryVerticalDrag };

    // Melody
    SuiteUI::LabelledSlider rootSlider { "ROOT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider octavesSlider { "OCTAVES", juce::Slider::RotaryVerticalDrag };

    // Chaos
    SuiteUI::LabelledSlider chaosSlider { "CHAOS", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider timingSlider { "TIMING RANGE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider velocityChaosSlider { "VEL. CHAOS", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider speedSlider { "CHAOS SPEED", juce::Slider::RotaryVerticalDrag };

    // Tempo
    SuiteUI::LabelledSlider tempoSlider { "TEMPO", juce::Slider::RotaryVerticalDrag };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;
    std::unique_ptr<juce::ParameterAttachment> rateAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicroTimingChaosEditor)
};
