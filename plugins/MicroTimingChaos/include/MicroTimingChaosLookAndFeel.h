#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace MicroTimingChaos
{
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        static LookAndFeel& getInstance();

        explicit LookAndFeel (juce::Colour accentColour);

        juce::Colour getAccent() const noexcept { return accent; }

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;

        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;

        void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

        juce::Colour getPanelColour() const noexcept { return panel; }
        juce::Colour getPanelLightColour() const noexcept { return panelLight; }
        juce::Colour getTrackColour() const noexcept { return track; }
        juce::Colour getTextColour() const noexcept { return text; }
        juce::Colour getTextDimColour() const noexcept { return textDim; }

    private:
        juce::Colour accent;
        juce::Colour panel;
        juce::Colour panelLight;
        juce::Colour track;
        juce::Colour text;
        juce::Colour textDim;
    };
}
