#include "MicroTimingChaosLookAndFeel.h"

namespace MicroTimingChaos
{
    static LookAndFeel* instance = nullptr;

    LookAndFeel& LookAndFeel::getInstance()
    {
        if (instance == nullptr)
            instance = new LookAndFeel();
        return *instance;
    }

    LookAndFeel::LookAndFeel (juce::Colour accentColour)
    {
        accent = accentColour;

        panel = juce::Colour (0xff151a22);
        panelLight = juce::Colour (0xff1f2631);
        track = juce::Colour (0xff2a3240);
        text = juce::Colour (0xffdde4ee);
        textDim = juce::Colour (0xff7d8899);

        // Rotary slider: arc knob with LED ring
        setDrawRotarySliderBackgroundShape (true);
        setDrawRotarySliderText (true);
        setDrawRotarySliderValueLabel (true);

        // Toggle button: glow toggle
        setDrawToggleButtonOutline (true);
        setDrawToggleButtonAsHighlighted (true);
    }

    void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
    {
        juce::Slider::DrawRotarySlider (g, x, y, width, height, sliderPosProportional,
                                        rotaryStartAngle, rotaryEndAngle, slider);

        // LED ring
        g.setColour (accent);
        g.setDiscardAlphaBlending (false);
        g.setAntiAliasedAlpha (0.6f);

        const float angle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * sliderPosProportional;
        const float radius = std::min (width, height) * 0.35f;

        const float cx = x + width * 0.5f;
        const float cy = y + height * 0.5f;

        g.fillArc (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f,
                   angle - 1.5707963f, 1.5707963f);
    }

    void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float minSliderPos, float maxSliderPos,
                                        juce::Slider::SliderStyle, juce::Slider& slider)
    {
        juce::Slider::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos,
                                        slider.getSliderStyle(), slider);

        // Subtle highlight
        g.setColour (accent);
        g.setDiscardAlphaBlending (false);
        g.setAntiAliasedAlpha (0.3f);

        const float pos = (sliderPos - minSliderPos) / (maxSliderPos - minSliderPos) * height;
        g.fillRoundedRectangle (x, y + pos - 2.0f, width, 4.0f, 2.0f);
    }

    void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool shouldDrawButtonAsDown)
    {
        const auto bounds = button.getLocalBounds();

        g.setColour (panel);
        g.fillRect (bounds);

        const float pad = 12.0f;
        const float radius = bounds.getWidth() * 0.4f;

        // Glow ring
        g.setColour (accent);
        g.setDiscardAlphaBlending (false);
        g.setAntiAliasedAlpha (0.5f);

        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float ringRadius = radius * 0.9f;

        if (shouldDrawButtonAsHighlighted)
        {
            g.drawRoundedRectangle (cx - ringRadius, cy - ringRadius, ringRadius * 2.0f, ringRadius * 2.0f, 4.0f);
        }

        // Button body
        g.setColour (shouldDrawButtonAsHighlighted ? accent.withAlpha (0.2f) : textDim.withAlpha (0.3f));
        g.fillRoundedRectangle (bounds.reduced (pad), radius);

        // Icon/text
        g.setColour (shouldDrawButtonAsHighlighted ? text : textDim);
        g.setFont (juce::Font (10.0f));

        const float iconSize = bounds.getWidth() * 0.35f;
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();

        if (button.isToggled())
        {
            g.drawText ("ON", bounds.reduced (pad), juce::Justification::centred);
        }
        else
        {
            g.drawText ("OFF", bounds.reduced (pad), juce::Justification::centred);
        }
    }
}
