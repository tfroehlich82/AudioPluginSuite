#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "LerpCurveEditor.h"
#include "LerpPreview.h"
#include "ModulationPanel.h"
#include "MicroTimingChaosLookAndFeel.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"

/** The sequencer's step grid, showing the pattern, playhead, and recent notes. */
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

    /** The host's scale, multiplied by the design scale below, so the window is two thirds of the design size. */
    void setScaleFactor (float newScale) override;

private:
    void timerCallback() override;
    void showModulatedValues();
    void showHelp (const juce::MouseEvent&);

    /** Forwards mouse moves from every child to the editor, so the help line follows the pointer. */
    struct HelpTracker : public juce::MouseListener
    {
        std::function<void (const juce::MouseEvent&)> onMove;
        void mouseMove (const juce::MouseEvent& e) override { if (onMove) onMove (e); }
    };

    struct Group
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    MicroTimingChaosProcessor& processor;

    // Declared before the components that use it, so it outlives them.
    MicroTimingChaos::LookAndFeel lookAndFeel { MicroTimingChaos::Palette::velocity };
    HelpTracker helpTracker;
    std::vector<Group> groups;

    juce::Label helpLabel;

    // Visualisers
    SuiteUI::TrailPlot attractor { MicroTimingChaos::Palette::velocity };
    juce::Label readout;
    StepGridView grid { MicroTimingChaos::Palette::velocity };

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
juce::Label scaleCaption;
    juce::ComboBox scaleBox;

    // Chaos
    SuiteUI::LabelledSlider chaosSlider { "CHAOS", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider timingRangeSlider { "TIMING RANGE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider velocityChaosSlider { "VELOCITY CHAOS", juce::Slider::RotaryVerticalDrag };

    // Lerp curve: the curve and preview tabs, and the lerp and per-axis multipliers
    SuiteUI::SegmentedSelector lerpViewSelector { MicroTimingChaos::Palette::velocity };
    LerpCurveEditor curveEditor;
    LerpPreview lerpPreview;

    // Modulation: two LFOs and four routing slots, at the bottom of the left column
    ModulationPanel modulation { processor };
    SuiteUI::LabelledSlider lerpSlider { "LERP", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider lerpXMultSlider { "X MULT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider lerpYMultSlider { "Y MULT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider lerpZMultSlider { "Z MULT", juce::Slider::RotaryVerticalDrag };

    // Transport
    SuiteUI::LabelledSlider tempoSlider { "TEMPO", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider speedSlider { "SPEED", juce::Slider::RotaryVerticalDrag };
    SuiteUI::SegmentedSelector rateSelector { MicroTimingChaos::Palette::velocity };
    juce::ToggleButton syncButton { "BPM SYNC" };
    juce::Label hostTempoLabel;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;
    std::unique_ptr<juce::ParameterAttachment> rateAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicroTimingChaosEditor)
};
