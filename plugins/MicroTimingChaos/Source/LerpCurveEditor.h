#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

/**
    The lerp curve: five points that map Chaos (x, 0 to 1) to drift (y, 0 to 1).

    Drag a point to shape the curve. Right-click a point to reset it to the natural curve, and
    double-click the graph to reset the whole curve. The live marker shows the drift for the
    current Chaos value.
*/
class LerpCurveEditor : public juce::Component
{
public:
    explicit LerpCurveEditor (MicroTimingChaosProcessor& processor);

    /** The current Chaos value, 0 to 1, shown as the live marker. */
    void setChaosValue (float chaos);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toPlot (juce::Point<float> curvePoint) const;
    juce::Point<float> fromPlot (juce::Point<float> position) const;
    int nearestPoint (juce::Point<float> position) const;
    juce::Path curvePath (bool natural) const;

    MicroTimingChaosProcessor& processor;
    float chaosValue = 0.5f;
    int activePoint = -1;
    int hoverPoint = -1;
};
