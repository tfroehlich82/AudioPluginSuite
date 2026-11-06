#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SuiteLookAndFeel.h"

namespace MicroTimingChaos
{
    /** MTC-specific colours. The neutral palette is `SuiteUI::Colours`. */
    namespace Palette
    {
        inline const juce::Colour previewText { 0xffffffff };  // white: all text in the Lerp curve and preview panels

        inline const juce::Colour timing     { 0xff2de2ff };  // cyan: X, timing
        inline const juce::Colour pitch      { 0xffff9f1c };  // amber: Y, pitch and gate
        inline const juce::Colour velocity   { 0xff7dff5a };  // lime: Z, velocity and the default accent
        inline const juce::Colour lfo1       { 0xffff9f1c };  // amber: LFO 1
        inline const juce::Colour lfo2       { 0xffff9f1c };  // amber: LFO 2
    }

    /** Look and feel for Micro-Timing Chaos. Textures and icons come from BinaryData (see CMakeLists.txt). */
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit LookAndFeel (juce::Colour accentColour);

        juce::Colour getAccent() const noexcept { return accent; }

        /** Paints the background and grid texture behind the editor. */
        void paint (juce::Graphics& g, juce::Component& component);

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;

        /** The BPM Sync button: an icon, a texture overlay when active, and an ON or OFF state. */
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;

        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

        /** Larger text throughout: labels, value boxes, combo boxes and menus. */
        juce::Font getLabelFont (juce::Label&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getPopupMenuFont() override;

    private:
        juce::Colour accent;
    };
}
