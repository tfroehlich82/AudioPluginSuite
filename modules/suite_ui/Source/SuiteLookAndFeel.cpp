#include "SuiteLookAndFeel.h"

namespace SuiteUI
{
    namespace
    {
        constexpr int numLeds = 21;
    }

    SuiteLookAndFeel::SuiteLookAndFeel (juce::Colour accentColour)
        : accent (accentColour)
    {
        setColour (juce::Slider::textBoxTextColourId, Colours::text);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::thumbColourId, accent);
        setColour (juce::Label::textColourId, Colours::textDim);
        setColour (juce::ToggleButton::textColourId, Colours::text);
    }

    void SuiteLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float startAngle, float endAngle,
                                             juce::Slider&)
    {
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (3.0f);
        const float diameter = juce::jmin (area.getWidth(), area.getHeight());
        const auto centre = area.getCentre();
        const float ledRadius = diameter * 0.5f;

        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const auto pointOn = [&] (float radius, float a)
        {
            return centre + juce::Point<float> (std::sin (a), -std::cos (a)) * radius;
        };

        // LED ring: lit up to the current value, dim beyond it.
        for (int i = 0; i < numLeds; ++i)
        {
            const float t = (float) i / (float) (numLeds - 1);
            const auto p = pointOn (ledRadius - 2.0f, startAngle + t * (endAngle - startAngle));

            g.setColour (t <= sliderPos + 1.0e-4f ? accent : Colours::track);
            g.fillEllipse (p.x - 1.6f, p.y - 1.6f, 3.2f, 3.2f);
        }

        // Track and value arc, with a soft glow under the value arc.
        const float arcRadius = ledRadius - 9.0f;
        juce::Path trackArc, valueArc;
        trackArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);

        g.setColour (Colours::track);
        g.strokePath (trackArc, juce::PathStrokeType (3.0f));

        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (valueArc, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (accent);
        g.strokePath (valueArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Knob body and pointer.
        const float bodyRadius = arcRadius - 6.0f;
        juce::ColourGradient body (Colours::panelLight, centre.x, centre.y - bodyRadius,
                                   Colours::panel, centre.x, centre.y + bodyRadius, false);
        g.setGradientFill (body);
        g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);

        g.setColour (Colours::panelEdge);
        g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.0f);

        juce::Path pointer;
        pointer.startNewSubPath (pointOn (bodyRadius * 0.3f, angle));
        pointer.lineTo (pointOn (bodyRadius * 0.85f, angle));

        g.setColour (accent);
        g.strokePath (pointer, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void SuiteLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float minSliderPos, float maxSliderPos,
                                             juce::Slider::SliderStyle style, juce::Slider& slider)
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos,
                                              maxSliderPos, style, slider);
            return;
        }

        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const float centreY = bounds.getCentreY();
        const juce::Rectangle<float> trackArea (bounds.getX() + 6.0f, centreY - 2.0f, bounds.getWidth() - 12.0f, 4.0f);

        g.setColour (Colours::track);
        g.fillRoundedRectangle (trackArea, 2.0f);

        g.setColour (accent);
        g.fillRoundedRectangle (trackArea.withRight (sliderPos), 2.0f);

        g.setColour (Colours::text);
        g.fillEllipse (sliderPos - 5.0f, centreY - 5.0f, 10.0f, 10.0f);
    }

    void SuiteLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                             bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
        const float radius = bounds.getHeight() * 0.5f;
        const bool on = button.getToggleState();

        juce::Path pill;
        pill.addRoundedRectangle (bounds, radius);

        // Glow only while on, so an active toggle reads as lit.
        if (on)
            juce::DropShadow (accent.withAlpha (0.85f), 14, {}).drawForPath (g, pill);

        g.setColour (on ? accent.withAlpha (0.2f) : Colours::panel);
        g.fillPath (pill);

        g.setColour (on ? accent : Colours::panelEdge);
        g.strokePath (pill, juce::PathStrokeType (1.5f));

        const float brightness = shouldDrawButtonAsDown ? 0.8f : (shouldDrawButtonAsHighlighted ? 1.0f : 0.9f);
        g.setColour ((on ? accent : Colours::textDim).withMultipliedBrightness (brightness));
        g.setFont (juce::Font (14.0f, juce::Font::bold));
        g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
    }
}
