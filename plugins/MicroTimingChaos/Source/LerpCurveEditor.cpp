#include "LerpCurveEditor.h"

#include "MicroTimingChaosLookAndFeel.h"

namespace
{
    namespace Palette = MicroTimingChaos::Palette;

    constexpr float pointRadius = 5.0f;
    constexpr float grabRadius = 14.0f;
    constexpr int curveSamples = 64;
}

LerpCurveEditor::LerpCurveEditor (MicroTimingChaosProcessor& p)
    : processor (p)
{
    setOpaque (false);
}

void LerpCurveEditor::setChaosValue (float chaos)
{
    const float clamped = juce::jlimit (0.0f, 1.0f, chaos);

    if (clamped != chaosValue)
    {
        chaosValue = clamped;
        repaint();
    }
}

juce::Rectangle<float> LerpCurveEditor::plotArea() const
{
    // Room on the left for the drift scale and at the bottom for the chaos scale.
    auto area = getLocalBounds().toFloat().reduced (10.0f, 10.0f);
    area.removeFromLeft (20.0f);
    area.removeFromBottom (18.0f);
    return area;
}

juce::Point<float> LerpCurveEditor::toPlot (juce::Point<float> curvePoint) const
{
    const auto area = plotArea();
    return { area.getX() + curvePoint.x * area.getWidth(),
             area.getBottom() - curvePoint.y * area.getHeight() };
}

juce::Point<float> LerpCurveEditor::fromPlot (juce::Point<float> position) const
{
    const auto area = plotArea();
    return { juce::jlimit (0.0f, 1.0f, (position.x - area.getX()) / area.getWidth()),
             juce::jlimit (0.0f, 1.0f, (area.getBottom() - position.y) / area.getHeight()) };
}

int LerpCurveEditor::nearestPoint (juce::Point<float> position) const
{
    const auto points = processor.getLerpCurve();

    int best = -1;
    float bestDistance = grabRadius;

    for (int i = 0; i < (int) points.size(); ++i)
    {
        const float distance = toPlot (points[(size_t) i]).getDistanceFrom (position);
        if (distance <= bestDistance)
        {
            bestDistance = distance;
            best = i;
        }
    }

    return best;
}

juce::Path LerpCurveEditor::curvePath (bool natural) const
{
    const auto area = plotArea();
    juce::Path path;

    for (int i = 0; i <= curveSamples; ++i)
    {
        const float t = (float) i / (float) curveSamples;
        const float value = natural ? MicroTimingChaosProcessor::naturalCurveValue (t)
                                    : processor.evaluateLerpCurve (t);
        const juce::Point<float> point (area.getX() + t * area.getWidth(), area.getBottom() - value * area.getHeight());

        if (i == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }

    return path;
}

void LerpCurveEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto area = plotArea();
    const auto accent = Palette::velocity;

    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    // Grid
    g.setColour (Palette::track.withAlpha (0.6f));
    for (int i = 0; i <= 4; ++i)
    {
        const float fraction = (float) i / 4.0f;
        g.drawLine (area.getX() + fraction * area.getWidth(), area.getY(), area.getX() + fraction * area.getWidth(), area.getBottom(), 1.0f);
        g.drawLine (area.getX(), area.getBottom() - fraction * area.getHeight(), area.getRight(), area.getBottom() - fraction * area.getHeight(), 1.0f);
    }

    // Natural reference curve, dashed
    const float dashes[] = { 4.0f, 4.0f };
    juce::Path dashed;
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, curvePath (true), dashes, 2);
    g.setColour (Palette::textDim.withAlpha (0.4f));
    g.fillPath (dashed);

    // Curve: filled area, glow and line
    const auto curve = curvePath (false);

    juce::Path fill (curve);
    fill.lineTo (area.getBottomRight());
    fill.lineTo (area.getBottomLeft());
    fill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.28f), area.getX(), area.getY(),
                                             accent.withAlpha (0.0f), area.getX(), area.getBottom(), false));
    g.fillPath (fill);

    g.setColour (accent.withAlpha (0.22f));
    g.strokePath (curve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Live marker at the current chaos value
    const float markerX = area.getX() + chaosValue * area.getWidth();
    const float driftNow = processor.evaluateLerpCurve (chaosValue);
    const auto markerPoint = toPlot ({ chaosValue, driftNow });

    g.setColour (Palette::text.withAlpha (0.25f));
    g.drawLine (markerX, area.getY(), markerX, area.getBottom(), 1.0f);
    g.setColour (accent);
    g.fillEllipse (markerPoint.x - 4.0f, markerPoint.y - 4.0f, 8.0f, 8.0f);

    // Control points
    const auto points = processor.getLerpCurve();
    for (int i = 0; i < (int) points.size(); ++i)
    {
        const auto p = toPlot (points[(size_t) i]);
        const bool highlighted = i == activePoint || i == hoverPoint;
        const float radius = highlighted ? pointRadius * 1.35f : pointRadius;

        g.setColour (Palette::panel);
        g.fillEllipse (p.x - radius, p.y - radius, radius * 2.0f, radius * 2.0f);

        g.setColour (highlighted ? Palette::text : accent);
        g.drawEllipse (p.x - radius, p.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);
    }

    // Readout: live mapping, or the selected point
    juce::String readout;
    if (activePoint >= 0)
    {
        const auto& point = points[(size_t) activePoint];
        readout = "Point " + juce::String (activePoint + 1) + ":  chaos " + juce::String (point.x, 2)
                + "  ->  drift " + juce::String (point.y, 2);
    }
    else
    {
        readout = "Chaos " + juce::String (chaosValue, 2) + "  ->  drift " + juce::String (driftNow, 2);
    }

    g.setColour (Palette::previewText);
    g.setFont (juce::Font (14.0f, juce::Font::bold));
    g.drawText (readout, area.reduced (8.0f, 4.0f), juce::Justification::topLeft);

    // Axis labels: the drift scale on the left (rotated), the chaos scale underneath
    g.setColour (Palette::previewText);
    g.setFont (juce::Font (11.0f));
    g.drawText ("1", juce::Rectangle<float> (bounds.getX() + 4.0f, area.getY() - 4.0f, 14.0f, 12.0f), juce::Justification::centredRight);
    g.drawText ("0", juce::Rectangle<float> (bounds.getX() + 4.0f, area.getBottom() - 8.0f, 14.0f, 12.0f), juce::Justification::centredRight);

    const float labelX = bounds.getX() + 10.0f;
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, labelX, area.getCentreY()));
    g.drawText ("DRIFT", juce::Rectangle<float> (labelX - 30.0f, area.getCentreY() - 6.0f, 60.0f, 12.0f), juce::Justification::centred);
    g.restoreState();

    g.drawText ("CHAOS  0 - 1", juce::Rectangle<float> (area.getX(), area.getBottom() + 2.0f, area.getWidth(), 14.0f), juce::Justification::centred);
}

void LerpCurveEditor::mouseMove (const juce::MouseEvent& e)
{
    const int hit = nearestPoint (e.position);

    if (hit != hoverPoint)
    {
        hoverPoint = hit;
        repaint();
    }
}

void LerpCurveEditor::mouseExit (const juce::MouseEvent&)
{
    if (hoverPoint != -1)
    {
        hoverPoint = -1;
        repaint();
    }
}

void LerpCurveEditor::mouseDown (const juce::MouseEvent& e)
{
    const int hit = nearestPoint (e.position);

    if (hit < 0)
        return;

    if (e.mods.isPopupMenu())
    {
        processor.setLerpCurvePoint (hit, MicroTimingChaosProcessor::naturalCurvePoint (hit));
    }
    else
    {
        activePoint = hit;
    }

    repaint();
}

void LerpCurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (activePoint < 0)
        return;

    processor.setLerpCurvePoint (activePoint, fromPlot (e.position));
    repaint();
}

void LerpCurveEditor::mouseUp (const juce::MouseEvent&)
{
    activePoint = -1;
    repaint();
}

void LerpCurveEditor::mouseDoubleClick (const juce::MouseEvent&)
{
    processor.resetLerpCurve();
    activePoint = -1;
    repaint();
}
