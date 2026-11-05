#include "SuiteWidgets.h"
#include "SuiteLookAndFeel.h"

#include <cmath>

namespace SuiteUI
{
    namespace
    {
        constexpr float minDb = -96.0f;
        constexpr float meterFloorDb = -60.0f;

        float toDb (float magnitude, float scale) noexcept
        {
            return 20.0f * std::log10 (juce::jmax (magnitude * scale, 1.0e-6f));
        }

        float unitInterval (float value, float low, float high) noexcept
        {
            return juce::jlimit (0.0f, 1.0f, (value - low) / (high - low));
        }

        // Maps a point in [-1, 1] x [-1, 1] (y pointing up) into the plot area.
        juce::Point<float> plotPoint (juce::Rectangle<float> area, float x, float y) noexcept
        {
            return { area.getX() + (x * 0.5f + 0.5f) * area.getWidth(),
                     area.getY() + (0.5f - y * 0.5f) * area.getHeight() };
        }
    }

    //==============================================================================
    SpectrumDisplay::SpectrumDisplay (juce::Colour in, juce::Colour out)
        : inputColour (in), outputColour (out)
    {
    }

    void SpectrumDisplay::setSpectra (const float* inputMagnitudes, const float* outputMagnitudes,
                                      int numBins, double binWidthHz_, float magnitudeScale)
    {
        binWidthHz = binWidthHz_;
        inputDb.resize ((size_t) numBins);
        outputDb.resize ((size_t) numBins);

        for (int k = 0; k < numBins; ++k)
        {
            inputDb[(size_t) k] = toDb (inputMagnitudes[k], magnitudeScale);
            outputDb[(size_t) k] = toDb (outputMagnitudes[k], magnitudeScale);
        }

        repaint();
    }

    juce::Path SpectrumDisplay::makePath (const std::vector<float>& decibels, juce::Rectangle<float> area) const
    {
        juce::Path path;
        const float minFreq = 20.0f;
        const float maxFreq = 20000.0f;
        bool started = false;

        for (size_t k = 1; k < decibels.size(); ++k)
        {
            const float freq = (float) (k * binWidthHz);

            if (freq < minFreq || freq > maxFreq)
                continue;

            const float x = area.getX() + std::log (freq / minFreq) / std::log (maxFreq / minFreq) * area.getWidth();
            const float y = area.getBottom() - unitInterval (decibels[k], minDb, 0.0f) * area.getHeight();

            // Path::isEmpty() stays true until a line exists, so it cannot tell whether a sub-path has started.
            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }
        }

        return path;
    }

    void SpectrumDisplay::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat().reduced (4.0f);

        g.setColour (Colours::panel);
        g.fillRoundedRectangle (area, 6.0f);

        // Horizontal dB gridlines.
        g.setColour (Colours::track);
        for (float db = -24.0f; db > minDb; db -= 24.0f)
            g.drawHorizontalLine ((int) (area.getBottom() - unitInterval (db, minDb, 0.0f) * area.getHeight()),
                                  area.getX(), area.getRight());

        // Frequency gridlines and labels: 100 Hz, 1 kHz and 10 kHz.
        const float minFreq = 20.0f;
        const float maxFreq = 20000.0f;
        g.setFont (juce::Font (10.0f));

        for (const float freq : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = area.getX() + std::log (freq / minFreq) / std::log (maxFreq / minFreq) * area.getWidth();
            g.setColour (Colours::track);
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            g.setColour (Colours::textDim);
            g.drawText (freq >= 1000.0f ? juce::String ((int) (freq / 1000.0f)) + "k" : juce::String ((int) freq),
                        juce::Rectangle<float> (x + 2.0f, area.getBottom() - 14.0f, 40.0f, 12.0f),
                        juce::Justification::left);
        }

        if (! signalPresent || inputDb.empty())
        {
            g.setColour (Colours::textDim);
            g.setFont (juce::Font (13.0f));
            g.drawText (waitingMessage, area, juce::Justification::centred);
            return;
        }

        g.setColour (inputColour.withAlpha (0.55f));
        g.strokePath (makePath (inputDb, area), juce::PathStrokeType (1.2f));

        const auto outputPath = makePath (outputDb, area);
        g.setColour (outputColour.withAlpha (0.25f));
        g.strokePath (outputPath, juce::PathStrokeType (5.0f));
        g.setColour (outputColour);
        g.strokePath (outputPath, juce::PathStrokeType (1.8f));
    }

    void SpectrumDisplay::setWaitingMessage (const juce::String& message)
    {
        if (waitingMessage != message)
        {
            waitingMessage = message;
            repaint();
        }
    }

    void SpectrumDisplay::setSignalPresent (bool present)
    {
        if (signalPresent != present)
        {
            signalPresent = present;
            repaint();
        }
    }

    //==============================================================================
    TrailPlot::TrailPlot (juce::Colour c)
        : colour (c)
    {
    }

    void TrailPlot::setTrail (std::vector<juce::Point<float>> points)
    {
        trail = std::move (points);
        repaint();
    }

    void TrailPlot::paint (juce::Graphics& g)
    {
        const auto area = getLocalBounds().toFloat().reduced (4.0f);

        g.setColour (Colours::panel);
        g.fillRoundedRectangle (area, 6.0f);

        g.setColour (Colours::track);
        g.drawLine (area.getX(), area.getCentreY(), area.getRight(), area.getCentreY());
        g.drawLine (area.getCentreX(), area.getY(), area.getCentreX(), area.getBottom());

        if (trail.size() < 2)
            return;

        // Older segments fade out; the newest point gets a bright marker.
        const auto count = trail.size();

        for (size_t i = 1; i < count; ++i)
        {
            const float age = (float) i / (float) count;
            g.setColour (colour.withAlpha (age * 0.9f));
            g.drawLine (juce::Line<float> (plotPoint (area, trail[i - 1].x, trail[i - 1].y),
                                           plotPoint (area, trail[i].x, trail[i].y)), 1.5f);
        }

        const auto head = plotPoint (area, trail.back().x, trail.back().y);
        g.setColour (colour);
        g.fillEllipse (head.x - 3.5f, head.y - 3.5f, 7.0f, 7.0f);
    }

    //==============================================================================
    CurveView::CurveView (juce::Colour c)
        : colour (c)
    {
    }

    void CurveView::setCurve (std::function<float (float)> newCurve, float marker)
    {
        curve = std::move (newCurve);
        markerInput = marker;
        repaint();
    }

    void CurveView::paint (juce::Graphics& g)
    {
        const auto area = getLocalBounds().toFloat().reduced (4.0f);

        g.setColour (Colours::panel);
        g.fillRoundedRectangle (area, 6.0f);

        // Unity-gain reference line.
        g.setColour (Colours::track);
        g.drawLine (juce::Line<float> (plotPoint (area, -1.0f, -1.0f), plotPoint (area, 1.0f, 1.0f)), 1.0f);

        if (! curve)
            return;

        constexpr int steps = 128;
        juce::Path path;

        for (int i = 0; i <= steps; ++i)
        {
            const float x = -1.0f + 2.0f * (float) i / (float) steps;
            const auto p = plotPoint (area, x, juce::jlimit (-1.0f, 1.0f, curve (x)));

            if (i == 0)
                path.startNewSubPath (p);
            else
                path.lineTo (p);
        }

        g.setColour (colour.withAlpha (0.25f));
        g.strokePath (path, juce::PathStrokeType (6.0f));
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.0f));

        const float markerX = juce::jlimit (-1.0f, 1.0f, markerInput);
        const auto marker = plotPoint (area, markerX, juce::jlimit (-1.0f, 1.0f, curve (markerX)));
        g.setColour (colour);
        g.fillEllipse (marker.x - 4.0f, marker.y - 4.0f, 8.0f, 8.0f);
    }

    //==============================================================================
    LevelMeter::LevelMeter (juce::Colour c)
        : colour (c)
    {
    }

    void LevelMeter::setLevel (float linearLevel)
    {
        level = linearLevel;
        repaint();
    }

    void LevelMeter::paint (juce::Graphics& g)
    {
        const auto area = getLocalBounds().toFloat().reduced (2.0f);
        const float db = 20.0f * std::log10 (juce::jmax (level, 1.0e-6f));
        const float fill = unitInterval (db, meterFloorDb, 0.0f);

        g.setColour (Colours::track);
        g.fillRoundedRectangle (area, 3.0f);

        auto filled = area.withTop (area.getBottom() - fill * area.getHeight());
        g.setColour (colour);
        g.fillRoundedRectangle (filled, 3.0f);
    }

    //==============================================================================
    SegmentedSelector::SegmentedSelector (juce::Colour accentColour)
        : accent (accentColour)
    {
    }

    void SegmentedSelector::setOptions (const juce::StringArray& labels)
    {
        options = labels;
        repaint();
    }

    void SegmentedSelector::setSelectedIndex (int index)
    {
        selected = juce::jlimit (0, juce::jmax (0, options.size() - 1), index);
        repaint();
    }

    void SegmentedSelector::setFontHeight (float height)
    {
        fontHeight = height;
        repaint();
    }

    void SegmentedSelector::paint (juce::Graphics& g)
    {
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        const float segmentWidth = area.getWidth() / (float) juce::jmax (1, options.size());

        g.setColour (Colours::panel);
        g.fillRoundedRectangle (area, area.getHeight() * 0.5f);
        g.setColour (Colours::panelEdge);
        g.drawRoundedRectangle (area, area.getHeight() * 0.5f, 1.0f);

        for (int i = 0; i < options.size(); ++i)
        {
            const juce::Rectangle<float> segment (area.getX() + segmentWidth * (float) i, area.getY(),
                                                  segmentWidth, area.getHeight());
            const bool isSelected = i == selected;

            if (isSelected)
            {
                juce::DropShadow (accent.withAlpha (0.7f), 10, {}).drawForRectangle (g, segment.toNearestInt());
                g.setColour (accent.withAlpha (0.22f));
                g.fillRoundedRectangle (segment.reduced (3.0f), (area.getHeight() - 6.0f) * 0.5f);
            }

            g.setColour (isSelected ? accent : Colours::textDim);
            g.setFont (juce::Font (fontHeight, isSelected ? juce::Font::bold : juce::Font::plain));
            g.drawText (options[i], segment, juce::Justification::centred);
        }
    }

    void SegmentedSelector::mouseDown (const juce::MouseEvent& event)
    {
        if (options.isEmpty())
            return;

        const float segmentWidth = (float) getWidth() / (float) options.size();
        const int index = juce::jlimit (0, options.size() - 1, (int) (event.position.x / segmentWidth));

        if (index != selected)
        {
            setSelectedIndex (index);

            if (onChange)
                onChange (index);
        }
    }

    //==============================================================================
    LabelledSlider::LabelledSlider (const juce::String& caption, juce::Slider::SliderStyle style)
    {
        const bool rotary = style == juce::Slider::RotaryVerticalDrag
                         || style == juce::Slider::RotaryHorizontalVerticalDrag
                         || style == juce::Slider::Rotary;

        slider.setSliderStyle (style);
        slider.setTextBoxStyle (rotary ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight,
                                false, 84, 18);

        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (12.0f, juce::Font::bold));

        addAndMakeVisible (label);
        addAndMakeVisible (slider);
    }

    void LabelledSlider::resized()
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (18));
        slider.setBounds (area);
    }
}
