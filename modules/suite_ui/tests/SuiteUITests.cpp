// Tests for the shared UI module. Widgets are laid out and painted into images, so the drawing
// code and the look and feel run as they would in a plugin window.

#undef NDEBUG
#include <cassert>

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>
#include <vector>

#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"

namespace
{
    constexpr juce::uint32 testAccent = 0xff2de2ff;

    // Paints a component into an image and returns the number of non-transparent pixels.
    int paintAndCountVisiblePixels (juce::Component& component, int width, int height)
    {
        component.setBounds (0, 0, width, height);

        juce::Image image (juce::Image::ARGB, width, height, true);

        // The Graphics must be released before the pixels are read. JUCE 9 does not make the drawing visible in the image until then.
        {
            juce::Graphics g (image);
            component.paintEntireComponent (g, true);
        }

        int visible = 0;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                if (image.getPixelAt (x, y).getAlpha() > 0)
                    ++visible;

        return visible;
    }

    void testSegmentedSelector()
    {
        SuiteUI::SegmentedSelector selector { juce::Colour (testAccent) };
        selector.setOptions ({ "TUBE", "TAPE", "DIODE" });

        int callbacks = 0;
        selector.onChange = [&callbacks] (int) { ++callbacks; };

        assert (selector.getSelectedIndex() == 0);

        // Programmatic changes update the display but do not notify, so they cannot feed back into the parameter.
        selector.setSelectedIndex (2);
        assert (selector.getSelectedIndex() == 2);
        assert (callbacks == 0);

        // Out-of-range indices are clamped to the last option.
        selector.setSelectedIndex (17);
        assert (selector.getSelectedIndex() == 2);

        selector.setSelectedIndex (-4);
        assert (selector.getSelectedIndex() == 0);

        // An empty selector stays at zero.
        SuiteUI::SegmentedSelector empty { juce::Colour (testAccent) };
        empty.setSelectedIndex (3);
        assert (empty.getSelectedIndex() == 0);

        assert (paintAndCountVisiblePixels (selector, 240, 30) > 0);

        std::puts ("[PASS] testSegmentedSelector");
    }

    void testSegmentedSelectorClicksNotify()
    {
        SuiteUI::SegmentedSelector selector { juce::Colour (testAccent) };
        selector.setOptions ({ "A", "B", "C", "D" });
        selector.setBounds (0, 0, 400, 30);

        std::vector<int> reported;
        selector.onChange = [&reported] (int index) { reported.push_back (index); };

        // Clicking the middle of each segment selects that segment.
        const auto click = [&selector] (float x)
        {
            juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(),
                                    { x, 15.0f }, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                    &selector, &selector, juce::Time::getCurrentTime(),
                                    { x, 15.0f }, juce::Time::getCurrentTime(), 1, false);
            selector.mouseDown (event);
        };

        click (150.0f);   // segment 1
        click (350.0f);   // segment 3
        click (350.0f);   // already selected, so no notification

        assert (reported.size() == 2);
        assert (reported[0] == 1 && reported[1] == 3);
        assert (selector.getSelectedIndex() == 3);

        std::puts ("[PASS] testSegmentedSelectorClicksNotify");
    }

    void testLabelledSliders()
    {
        SuiteUI::LabelledSlider rotary ("DRIVE", juce::Slider::RotaryVerticalDrag);
        SuiteUI::LabelledSlider linear ("CHAOS", juce::Slider::LinearHorizontal);

        assert (rotary.label.getText() == "DRIVE");
        assert (linear.label.getText() == "CHAOS");

        rotary.slider.setRange (0.0, 10.0);
        rotary.slider.setValue (4.0, juce::dontSendNotification);
        assert (std::abs (rotary.slider.getValue() - 4.0) < 1e-9);

        // The look and feel draws the knob and the linear track.
        SuiteUI::SuiteLookAndFeel laf { juce::Colour (testAccent) };
        rotary.setLookAndFeel (&laf);
        linear.setLookAndFeel (&laf);

        assert (paintAndCountVisiblePixels (rotary, 120, 150) > 0);
        assert (paintAndCountVisiblePixels (linear, 260, 60) > 0);

        rotary.setLookAndFeel (nullptr);
        linear.setLookAndFeel (nullptr);

        std::puts ("[PASS] testLabelledSliders");
    }

    void testLookAndFeelToggleButton()
    {
        SuiteUI::SuiteLookAndFeel laf { juce::Colour (testAccent) };
        assert (laf.getAccent() == juce::Colour (testAccent));

        juce::ToggleButton button ("FREEZE");
        button.setLookAndFeel (&laf);

        // Off and on, and while pressed, each take a different branch in the drawing code.
        button.setToggleState (false, juce::dontSendNotification);
        const int offPixels = paintAndCountVisiblePixels (button, 140, 40);

        button.setToggleState (true, juce::dontSendNotification);
        const int onPixels = paintAndCountVisiblePixels (button, 140, 40);

        assert (offPixels > 0 && onPixels > 0);

        button.setLookAndFeel (nullptr);

        std::puts ("[PASS] testLookAndFeelToggleButton");
    }

    void testSpectrumDisplay()
    {
        SuiteUI::SpectrumDisplay display (juce::Colour (0xffa9b7c9), juce::Colour (testAccent));

        // Before any data arrives the display shows its waiting message.
        assert (paintAndCountVisiblePixels (display, 500, 200) > 0);

        std::vector<float> input (1025), output (1025);

        for (int k = 0; k < 1025; ++k)
        {
            input[(size_t) k] = (k == 43) ? 200.0f : 0.001f;
            output[(size_t) k] = (k == 43) ? 180.0f : 0.0f;
        }

        display.setSignalPresent (true);
        display.setSpectra (input.data(), output.data(), 1025, 48000.0 / 2048.0, 2.0f / 2048.0f);

        assert (paintAndCountVisiblePixels (display, 500, 200) > 0);

        // Back to waiting, and a silent frame, both paint without problems.
        display.setSignalPresent (false);
        assert (paintAndCountVisiblePixels (display, 500, 200) > 0);

        std::fill (input.begin(), input.end(), 0.0f);
        std::fill (output.begin(), output.end(), 0.0f);
        display.setSignalPresent (true);
        display.setSpectra (input.data(), output.data(), 1025, 48000.0 / 2048.0, 2.0f / 2048.0f);
        assert (paintAndCountVisiblePixels (display, 500, 200) > 0);

        std::puts ("[PASS] testSpectrumDisplay");
    }

    // Counts painted pixels within a tolerance of a colour. Used to check that a trace is really drawn.
    int countPixelsNear (juce::Component& component, int width, int height, juce::Colour target, int tolerance)
    {
        component.setBounds (0, 0, width, height);

        juce::Image image (juce::Image::ARGB, width, height, true);

        {
            juce::Graphics g (image);
            component.paintEntireComponent (g, true);
        }

        int count = 0;

        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                const auto pixel = image.getPixelAt (x, y);

                if (pixel.getAlpha() > 0
                    && std::abs (pixel.getRed() - target.getRed()) <= tolerance
                    && std::abs (pixel.getGreen() - target.getGreen()) <= tolerance
                    && std::abs (pixel.getBlue() - target.getBlue()) <= tolerance)
                    ++count;
            }

        return count;
    }

    void testSpectrumCurveIsDrawn()
    {
        const juce::Colour outputColour (testAccent);
        SuiteUI::SpectrumDisplay display (juce::Colour (0xffa9b7c9), outputColour);

        std::vector<float> input (1025, 1.0e-6f), output (1025, 1.0e-6f);
        input[43] = 200.0f;
        output[43] = 180.0f;

        // No signal: the trace must not be drawn, only the waiting message.
        display.setSignalPresent (false);
        display.setSpectra (input.data(), output.data(), 1025, 48000.0 / 2048.0, 2.0f / 2048.0f);
        assert (countPixelsNear (display, 500, 200, outputColour, 30) == 0);

        // With signal: the output trace must cover real pixels, not just a single point.
        display.setSignalPresent (true);
        display.setSpectra (input.data(), output.data(), 1025, 48000.0 / 2048.0, 2.0f / 2048.0f);
        assert (countPixelsNear (display, 500, 200, outputColour, 30) > 200);

        std::puts ("[PASS] testSpectrumCurveIsDrawn");
    }

    void testTrailPlot()
    {
        SuiteUI::TrailPlot plot { juce::Colour (testAccent) };

        // Fewer than two points draws only the axes.
        assert (paintAndCountVisiblePixels (plot, 200, 200) > 0);

        std::vector<juce::Point<float>> points;
        for (int i = 0; i < 300; ++i)
            points.emplace_back (std::sin (0.05f * (float) i), std::cos (0.07f * (float) i));

        plot.setTrail (points);
        assert (paintAndCountVisiblePixels (plot, 200, 200) > 0);

        std::puts ("[PASS] testTrailPlot");
    }

    void testCurveView()
    {
        SuiteUI::CurveView view { juce::Colour (testAccent) };

        // No curve set yet: only the reference line is drawn.
        assert (paintAndCountVisiblePixels (view, 200, 160) > 0);

        view.setCurve ([] (float x) { return std::tanh (3.0f * x); }, 0.5f);
        assert (paintAndCountVisiblePixels (view, 200, 160) > 0);

        // A curve that overshoots the range is clamped for display.
        view.setCurve ([] (float x) { return 5.0f * x; }, 2.0f);
        assert (paintAndCountVisiblePixels (view, 200, 160) > 0);

        std::puts ("[PASS] testCurveView");
    }

    void testLevelMeter()
    {
        SuiteUI::LevelMeter meter { juce::Colour (testAccent) };

        for (float level : { 0.0f, 0.001f, 0.5f, 1.0f, 4.0f })
        {
            meter.setLevel (level);
            assert (paintAndCountVisiblePixels (meter, 24, 160) > 0);
        }

        std::puts ("[PASS] testLevelMeter");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running SuiteUI tests...");

    testSegmentedSelector();
    testSegmentedSelectorClicksNotify();
    testLabelledSliders();
    testLookAndFeelToggleButton();
    testSpectrumDisplay();
    testSpectrumCurveIsDrawn();
    testTrailPlot();
    testCurveView();
    testLevelMeter();

    std::puts ("All SuiteUI tests passed.");
    return 0;
}
