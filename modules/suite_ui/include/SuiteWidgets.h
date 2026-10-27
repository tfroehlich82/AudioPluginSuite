#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace SuiteUI
{
    /**
        Log-frequency spectrum view with two overlaid traces: the input (dim) and the output (bright).
        Feed it linear magnitudes from the FFT; it converts them to dB for display.
    */
    class SpectrumDisplay : public juce::Component
    {
    public:
        SpectrumDisplay (juce::Colour inputColour, juce::Colour outputColour);

        /**
            numBins: number of magnitude values per trace (frameSize / 2 + 1).
            binWidthHz: sampleRate / frameSize.
            magnitudeScale: multiplier that brings a full-scale sine near 0 dB.
        */
        void setSpectra (const float* inputMagnitudes, const float* outputMagnitudes,
                         int numBins, double binWidthHz, float magnitudeScale);

        void paint (juce::Graphics&) override;

    private:
        juce::Path makePath (const std::vector<float>& decibels, juce::Rectangle<float> area) const;

        juce::Colour inputColour, outputColour;
        std::vector<float> inputDb, outputDb;
        double binWidthHz = 1.0;
    };

    /** Phase-space plot: a fading trail of normalised (x, y) points in [-1, 1]. */
    class TrailPlot : public juce::Component
    {
    public:
        explicit TrailPlot (juce::Colour colour);

        /** Points are ordered oldest first, newest last. */
        void setTrail (std::vector<juce::Point<float>> points);

        void paint (juce::Graphics&) override;

    private:
        juce::Colour colour;
        std::vector<juce::Point<float>> trail;
    };

    /** Draws a transfer function over [-1, 1] and marks the current operating point. */
    class CurveView : public juce::Component
    {
    public:
        explicit CurveView (juce::Colour colour);

        /** curve maps input to output, both in [-1, 1]. markerInput is the current input level. */
        void setCurve (std::function<float (float)> curve, float markerInput);

        void paint (juce::Graphics&) override;

    private:
        juce::Colour colour;
        std::function<float (float)> curve;
        float markerInput = 0.0f;
    };

    /** Vertical meter for a linear level. Maps -60 dB .. 0 dB onto the full height. */
    class LevelMeter : public juce::Component
    {
    public:
        explicit LevelMeter (juce::Colour colour);

        void setLevel (float linearLevel);

        void paint (juce::Graphics&) override;

    private:
        juce::Colour colour;
        float level = 0.0f;
    };

    /** A row of mutually exclusive segments, used as a switch for choice parameters. */
    class SegmentedSelector : public juce::Component
    {
    public:
        explicit SegmentedSelector (juce::Colour accentColour);

        void setOptions (const juce::StringArray& labels);

        /** Updates the display without calling onChange. */
        void setSelectedIndex (int index);
        int getSelectedIndex() const noexcept { return selected; }

        /** Called on user clicks only. */
        std::function<void (int)> onChange;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        juce::Colour accent;
        juce::StringArray options;
        int selected = 0;
    };

    /** A slider with a caption above it. Rotary sliders get their value box below the knob. */
    class LabelledSlider : public juce::Component
    {
    public:
        LabelledSlider (const juce::String& caption, juce::Slider::SliderStyle style);

        juce::Slider slider;
        juce::Label label;

        void resized() override;
    };
}
