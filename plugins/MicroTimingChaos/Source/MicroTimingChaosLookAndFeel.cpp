#include "MicroTimingChaosLookAndFeel.h"

#include "BinaryData.h"

namespace MicroTimingChaos
{
    namespace
    {
        juce::Image loadImageResource (const char* name)
        {
            int size = 0;
            if (const char* data = BinaryData::getNamedResource (name, size))
                return juce::ImageFileFormat::loadFrom (data, (size_t) size);

            return {};
        }

        /** The 4K textures are much larger than they are drawn, so they are scaled down once when loaded. */
        juce::Image loadTexture (const char* name, int maxWidth)
        {
            auto image = loadImageResource (name);

            if (image.isValid() && image.getWidth() > maxWidth)
                image = image.rescaled (maxWidth, juce::roundToInt ((double) image.getHeight() * maxWidth / image.getWidth()));

            return image;
        }

        std::unique_ptr<juce::Drawable> loadIcon (const char* name)
        {
            int size = 0;
            if (const char* data = BinaryData::getNamedResource (name, size))
            {
                if (auto svg = juce::XmlDocument::parse (juce::String::fromUTF8 (data, size)))
                    return juce::Drawable::createFromSVG (*svg);
            }

            return {};
        }

        const juce::Image& backgroundImage()
        {
            static const juce::Image image = loadTexture ("sequencer_bg_4k_png", 1024);
            return image;
        }

        const juce::Image& gridImage()
        {
            static const juce::Image image = loadTexture ("sequencer_grid_4k_png", 1024);
            return image;
        }

        const juce::Image& syncImage()
        {
            static const juce::Image image = loadTexture ("bpm_sync_4k_png", 512);
            return image;
        }

        const juce::Drawable* syncIcon (bool active)
        {
            static const auto activeIcon = loadIcon ("bpm_sync_active_svg");
            static const auto inactiveIcon = loadIcon ("bpm_sync_inactive_svg");
            return (active ? activeIcon : inactiveIcon).get();
        }
    }

    LookAndFeel::LookAndFeel (juce::Colour accentColour)
        : accent (accentColour)
    {
        setColour (juce::Label::textColourId, Palette::text);
        setColour (juce::Slider::textBoxTextColourId, Palette::text);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, Palette::text);
        setColour (juce::PopupMenu::backgroundColourId, Palette::panel);
        setColour (juce::PopupMenu::textColourId, Palette::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, Palette::text);
    }

    void LookAndFeel::paint (juce::Graphics& g, juce::Component& component)
    {
        const auto area = component.getLocalBounds().toFloat();

        g.fillAll (Palette::background);

        const auto& background = backgroundImage();
        if (background.isValid())
            g.drawImage (background, area);

        const auto& grid = gridImage();
        if (grid.isValid())
            g.drawImage (grid, area);
    }

    void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (5.0f);
        const auto centre = bounds.getCentre();
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float thickness = juce::jmax (2.0f, radius * 0.12f);
        const float arcRadius = radius - thickness * 0.5f;
        const float angle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * sliderPosProportional;

        const auto stroke = juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path trackPath;
        trackPath.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (Palette::track);
        g.strokePath (trackPath, stroke);

        juce::Path valuePath;
        valuePath.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);

        // Neon value arc with a soft glow underneath
        g.setColour (accent.withAlpha (slider.isEnabled() ? 0.22f : 0.08f));
        g.strokePath (valuePath, juce::PathStrokeType (thickness * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (slider.isEnabled() ? accent : Palette::textDim);
        g.strokePath (valuePath, stroke);

        // Knob body
        const float bodyRadius = radius * 0.64f;
        juce::ColourGradient body (Palette::panelLight, centre.x, centre.y - bodyRadius,
                                   Palette::panel, centre.x, centre.y + bodyRadius, false);
        g.setGradientFill (body);
        g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);

        // Pointer
        const float dirX = std::sin (angle);
        const float dirY = -std::cos (angle);
        g.setColour (slider.isEnabled() ? accent : Palette::textDim);
        g.drawLine (centre.x + dirX * bodyRadius * 0.25f, centre.y + dirY * bodyRadius * 0.25f,
                    centre.x + dirX * bodyRadius * 0.85f, centre.y + dirY * bodyRadius * 0.85f,
                    juce::jmax (2.0f, thickness * 0.8f));
    }

    void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const bool active = button.getToggleState();
        const auto colour = active ? accent : Palette::textDim;

        g.setColour (shouldDrawButtonAsDown ? Palette::panelLight : Palette::panel);
        g.fillRoundedRectangle (bounds, 6.0f);

        if (active)
        {
            // Texture overlay, clipped to the button
            const auto& texture = syncImage();
            if (texture.isValid())
            {
                juce::Graphics::ScopedSaveState state (g);
                g.reduceClipRegion (bounds.toNearestInt());
                g.setOpacity (0.35f);
                g.drawImage (texture, bounds);
            }
        }

        g.setColour (colour.withAlpha (active || shouldDrawButtonAsHighlighted ? 0.9f : 0.45f));
        g.drawRoundedRectangle (bounds, 6.0f, active ? 1.5f : 1.0f);

        // Icon on the left, label and state on the right
        const auto iconArea = bounds.removeFromLeft (bounds.getHeight()).reduced (bounds.getHeight() * 0.2f);
        if (const auto* icon = syncIcon (active))
            icon->drawWithin (g, iconArea, juce::RectanglePlacement::centred, 1.0f);

        auto textArea = bounds.reduced (6.0f, 0.0f);

        g.setColour (active ? Palette::text : Palette::textDim);
        g.setFont (juce::Font (14.0f, juce::Font::bold));
        g.drawText (button.getButtonText(), textArea.withTrimmedRight (34.0f), juce::Justification::centredLeft);

        g.setColour (colour);
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText (active ? "ON" : "OFF", textArea.removeFromRight (30.0f), juce::Justification::centredRight);
    }

    void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                    int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box)
    {
        const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);

        g.setColour (Palette::panel);
        g.fillRoundedRectangle (bounds, 5.0f);

        g.setColour (box.hasKeyboardFocus (true) ? accent : Palette::track);
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

        const auto arrowArea = juce::Rectangle<float> ((float) buttonX, (float) buttonY, (float) buttonW, (float) buttonH).reduced (buttonW * 0.32f, buttonH * 0.38f);
        juce::Path arrow;
        arrow.addTriangle (arrowArea.getX(), arrowArea.getY(),
                           arrowArea.getRight(), arrowArea.getY(),
                           arrowArea.getCentreX(), arrowArea.getBottom());

        g.setColour (isButtonDown ? accent : Palette::textDim);
        g.fillPath (arrow);
    }

    void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
    {
        label.setBounds (box.getLocalBounds().reduced (10, 1).withTrimmedRight (14));
        label.setFont (getComboBoxFont (box));
    }

    juce::Font LookAndFeel::getLabelFont (juce::Label&)
    {
        return juce::Font (14.0f);
    }

    juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)
    {
        return juce::Font (14.0f);
    }

    juce::Font LookAndFeel::getPopupMenuFont()
    {
        return juce::Font (14.0f);
    }
}
