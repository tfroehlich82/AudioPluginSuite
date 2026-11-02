#include "LerpCurveEditor.h"

namespace {
    constexpr int controlPointsCount = 5;
    constexpr float pointSpacing = 0.25f;
    constexpr float curveWidth = 0.8f;
    constexpr float gridPadding = 0.15f;

    juce::Point<float> interpolate (const std::vector<juce::Point<float>>& points, float t)
    {
        const float step = 1.0f / (controlPointsCount - 1);
        const int index = (int) std::min((int) std::ceil(t / step), controlPointsCount - 1);
        const float alpha = (index + 1) * step - t;
        const auto& p1 = points[index];
        const auto& p2 = points[(index + 1) % points.size()];
        return { p1.x + (p2.x - p1.x) * alpha, p1.y + (p2.y - p1.y) * alpha };
    }

    juce::Point<float> evaluateCurve (const std::vector<juce::Point<float>>& points, float t)
    {
        if (t < 0.0f) return { 0.0f, 0.0f };
        if (t > 1.0f) return { 1.0f, points.back().y };

        const auto curve = interpolate (points, t);
        const float x = t;
        const float y = curve.y;

        const float dx = t - x;
        const float dy = y - curve.y;
        const float dist = std::sqrt (dx * dx + dy * dy);

        const float angle = std::atan2 (dy, dx);
        const float scale = std::min (1.0f, curveWidth / (dist + 0.0001f));

        const float sx = x + dx * scale;
        const float sy = y + dy * scale;

        return { sx, sy };
    }
}

LerpCurveEditor::LerpCurveEditor (ViewMode mode)
    : viewMode (mode)
{
    setOpaque (true);
    setWantsKeyboardFocus (false);

    // Lerp slider
    lerpSlider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal);
    lerpSlider->setRange (0.0f, 1.0f, 0.001f);
    lerpSlider->setTextValueSuffix (" Lerp");
    lerpSlider->setTextBoxStyle (juce::Slider::TextBoxOnly, true, 50, 20);
    addAndMakeVisible (lerpSlider.get());

    // Chaos slider
    chaosSlider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal);
    chaosSlider->setRange (0.0f, 1.0f, 0.001f);
    chaosSlider->setTextValueSuffix (" Chaos");
    chaosSlider->setTextBoxStyle (juce::Slider::TextBoxOnly, true, 50, 20);
    addAndMakeVisible (chaosSlider.get());

    if (viewMode == ViewMode::Preview)
    {
        setBounds (20, 20, 250, 40);
        setSize (350, 40);
    }
    else
    {
        setBounds (0, 0, 320, 120);
        setSize (320, 120);
    }
}

void LerpCurveEditor::setViewMode (ViewMode mode)
{
    viewMode = mode;

    if (mode == ViewMode::Preview)
    {
        setSize (350, 40);
        repaint();
    }
    else
    {
        setSize (320, 120);
        repaint();
    }
}

ViewMode LerpCurveEditor::getViewMode() const noexcept
{
    return viewMode;
}

void LerpCurveEditor::setControlPoints (const std::vector<juce::Point<float>>& points)
{
    controlPoints = points;
    updateCurve();
    repaint();
}

std::vector<juce::Point<float>> LerpCurveEditor::getControlPoints() const
{
    return controlPoints;
}

void LerpCurveEditor::setMultipliers (const juce::Point<float>& multipliers)
{
    multipliers = multipliers;
    repaint();
}

juce::Point<float> LerpCurveEditor::getMultipliers() const
{
    return multipliers;
}

void LerpCurveEditor::setLerpValue (float value)
{
    lerpValue = std::clamp (value, 0.0f, 1.0f);
    if (viewMode == ViewMode::Preview)
        lerpSlider->setValue (lerpValue, juce::dontSendNotification());
    repaint();
}

float LerpCurveEditor::getLerpValue() const
{
    return lerpValue;
}

void LerpCurveEditor::setChaosValue (float value)
{
    chaosValue = std::clamp (value, 0.0f, 1.0f);
    if (viewMode == ViewMode::Preview)
        chaosSlider->setValue (chaosValue, juce::dontSendNotification());
    repaint();
}

float LerpCurveEditor::getChaosValue() const
{
    return chaosValue;
}

juce::Colour LerpCurveEditor::getCurveColour() const noexcept
{
    return juce::Colour (0xff7dff5a);
}

juce::Colour LerpCurveEditor::getMultiplierColour(int axis) const noexcept
{
    static const juce::Colour colours[] = {
        juce::Colour (0xff2de2ff), // X - timing (cyan)
        juce::Colour (0xffff9f1c), // Y - pitch (amber)
        juce::Colour (0xff7dff5a)  // Z - velocity (lime)
    };
    return axis >= 0 && axis < 3 ? colours[axis] : juce::Colour (0xff7dff5a);
}

void LerpCurveEditor::updateCurve()
{
    if (controlPoints.empty())
    {
        controlPoints.resize (controlPointsCount, { 0.0f, 0.0f });
        for (int i = 1; i < controlPointsCount; ++i)
            controlPoints[i] = { (float) i / (controlPointsCount - 1), 0.0f };
    }

    const float x = chaosValue;
    const auto curve = interpolate (controlPoints, x);
    const float y = curve.y;
    const float dx = x - curve.x;
    const float dy = y - curve.y;
    const float dist = std::sqrt (dx * dx + dy * dy);
    const float angle = std::atan2 (dy, dx);
    const float scale = std::min (1.0f, curveWidth / (dist + 0.0001f));
    const float sx = x + dx * scale;
    const float sy = y + dy * scale;

    const float lerpX = multipliers.x;
    const float lerpY = multipliers.y;
    const float lerpZ = multipliers.z;

    const float driftX = lerpX * (1.0f - sx);
    const float driftY = lerpY * (1.0f - sy);
    const float driftZ = lerpZ * (1.0f - sy);

    if (viewMode == ViewMode::Preview)
    {
        lerpSlider->setValue (lerpValue, juce::dontSendNotification());
        chaosSlider->setValue (chaosValue, juce::dontSendNotification());
    }
}

void LerpCurveEditor::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff11141a));
    g.fillRect (getLocalBounds());

    const auto bounds = getLocalBounds().toFloat();
    const float width = bounds.getWidth();
    const float height = bounds.getHeight();

    if (viewMode == ViewMode::Preview)
    {
        // Preview view: show timing/pitch/velocity deviations
        const float yPerAxis = height / 3.0f;
        const float xOffset = 10.0f;
        const float yOffset = height / 2.0f;

        g.setColour (juce::Colour (0xff333744));
        g.drawRect (xOffset, yOffset - yPerAxis * 2, width - 2 * xOffset, yPerAxis * 2, 1.0f);
        g.drawRect (xOffset, yOffset - yPerAxis, width - 2 * xOffset, yPerAxis, 1.0f);
        g.drawRect (xOffset, yOffset, width - 2 * xOffset, yPerAxis, 1.0f);

        g.setFont (juce::Font (8.0f));
        g.setColour (juce::Colour (0xff555866));
        g.drawText (juce::String (driftX * 100.0f) + "%  (timing)", xOffset, yOffset - yPerAxis * 1.7f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftY * 100.0f) + "%  (pitch)", xOffset, yOffset - yPerAxis * 0.5f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftZ * 100.0f) + "%  (velocity)", xOffset, yOffset + yPerAxis * 0.3f, width - 2 * xOffset, 12.0f, juce::Justification::centred);

        return;
    }

    // Curve view: control points and curve
    const float padding = 20.0f;
    const float graphWidth = bounds.getWidth() - 2 * padding;
    const float graphHeight = bounds.getHeight() - 2 * padding;

    // Grid
    g.setColour (juce::Colour (0xff222633));
    g.drawRect (padding, padding, graphWidth, graphHeight, 1.0f);

    // Horizontal grid lines
    for (int i = 0; i <= 4; ++i)
    {
        const float y = padding + graphHeight * i / 4.0f;
        g.drawHorizontalLine (y, padding, padding + graphWidth, 1.0f);
    }

    // Vertical grid lines
    for (int i = 0; i <= 4; ++i)
    {
        const float x = padding + graphWidth * i / 4.0f;
        g.drawVerticalLine (x, padding, padding + graphHeight, 1.0f);
    }

    // Axis labels
    g.setColour (juce::Colour (0xff555866));
    g.setFont (juce::Font (9.0f));

    // X axis (Chaos 0–1)
    g.drawText ("0", padding + graphWidth * 0.2f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.drawText ("1", padding + graphWidth * 0.8f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Chaos", padding, padding + graphHeight * 0.95f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Y axis (Drift 0–1)
    g.drawText ("0", padding, padding + graphHeight * 0.05f, juce::Justification::centred);
    g.drawText ("1", padding, padding + graphHeight * 0.85f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Drift", padding, padding + graphHeight * 0.05f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Control points
    for (int i = 0; i < (int) controlPoints.size(); ++i)
    {
        const float x = padding + controlPoints[i].x * graphWidth;
        const float y = padding + graphHeight - controlPoints[i].y * graphHeight;

        // Draw control point
        const float radius = 4.0f;
        g.setColour (getCurveColour());
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 0.0f, juce::jmax (0.0f, 360.0f - i * 72.0f));

        // Draw point label
        g.setColour (juce::Colour (0xff7dff5a));
        g.setFont (juce::Font (8.0f));
        const float labelX = x + 12.0f;
        const float labelY = y - 12.0f;
        g.drawEllipse (labelX - 3.0f, labelY - 3.0f, 6.0f, 6.0f, 0.0f, 60.0f);
    }

    // Curve
    const float pointsT = 0.0f;
    const float pointsX = controlPoints[0].x;
    const float pointsY = controlPoints[0].y;
    const float dx = pointsX - 0.0f;
    const float dy = pointsY - 0.0f;
    const float dist = std::sqrt (dx * dx + dy * dy);
    const float angle = std::atan2 (dy, dx);
    const float scale = std::min (1.0f, curveWidth / (dist + 0.0001f));
    const float sx = pointsX + dx * scale;
    const float sy = pointsY + dy * scale;

    const float lerpX = multipliers.x;
    const float lerpY = multipliers.y;
    const float lerpZ = multipliers.z;

    const float driftX = lerpX * (1.0f - sx);
    const float driftY = lerpY * (1.0f - sy);
    const float driftZ = lerpZ * (1.0f - sy);

    const float curveX = chaosValue;
    const auto curve = interpolate (controlPoints, chaosValue);
    const float curveY = curve.y;
    const float curveDx = chaosValue - curve.x;
    const float curveDy = curveY - curve.y;
    const float curveDist = std::sqrt (curveDx * curveDx + curveDy * curveDy);
    const float curveAngle = std::atan2 (curveDy, curveDx);
    const float curveScale = std::min (1.0f, curveWidth / (curveDist + 0.0001f));
    const float curveSx = chaosValue + curveDx * curveScale;
    const float curveSy = curveY + curveDy * curveScale;

    const float curveDriftX = lerpX * (1.0f - curveSx);
    const float curveDriftY = lerpY * (1.0f - curveSy);
    const float curveDriftZ = lerpZ * (1.0f - curveSy);

    const float previewX = lerpValue;
    const auto previewCurve = interpolate (controlPoints, previewX);
    const float previewY = previewCurve.y;
    const float previewDx = previewX - previewCurve.x;
    const float previewDy = previewY - previewCurve.y;
    const float previewDist = std::sqrt (previewDx * previewDx + previewDy * previewDy);
    const float previewAngle = std::atan2 (previewDy, previewDx);
    const float previewScale = std::min (1.0f, curveWidth / (previewDist + 0.0001f));
    const float previewSx = previewX + previewDx * previewScale;
    const float previewSy = previewY + previewDy * previewScale;

    const float previewDriftX = lerpX * (1.0f - previewSx);
    const float previewDriftY = lerpY * (1.0f - previewSy);
    const float previewDriftZ = lerpZ * (1.0f - previewSy);

    if (viewMode == ViewMode::Preview)
    {
        const float yPerAxis = height / 3.0f;
        const float xOffset = 10.0f;
        const float yOffset = height / 2.0f;

        g.setColour (juce::Colour (0xff333744));
        g.drawRect (xOffset, yOffset - yPerAxis * 2, width - 2 * xOffset, yPerAxis * 2, 1.0f);
        g.drawRect (xOffset, yOffset - yPerAxis, width - 2 * xOffset, yPerAxis, 1.0f);
        g.drawRect (xOffset, yOffset, width - 2 * xOffset, yPerAxis, 1.0f);

        g.setFont (juce::Font (8.0f));
        g.setColour (juce::Colour (0xff555866));
        g.drawText (juce::String (driftX * 100.0f) + "%  (timing)", xOffset, yOffset - yPerAxis * 1.7f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftY * 100.0f) + "%  (pitch)", xOffset, yOffset - yPerAxis * 0.5f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftZ * 100.0f) + "%  (velocity)", xOffset, yOffset + yPerAxis * 0.3f, width - 2 * xOffset, 12.0f, juce::Justification::centred);

        return;
    }

    // Axis labels
    g.setColour (juce::Colour (0xff555866));
    g.setFont (juce::Font (9.0f));

    // X axis (Chaos 0–1)
    g.drawText ("0", padding + graphWidth * 0.2f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.drawText ("1", padding + graphWidth * 0.8f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Chaos", padding, padding + graphHeight * 0.95f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Y axis (Drift 0–1)
    g.drawText ("0", padding, padding + graphHeight * 0.05f, juce::Justification::centred);
    g.drawText ("1", padding, padding + graphHeight * 0.85f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Drift", padding, padding + graphHeight * 0.05f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Control points
    for (int i = 0; i < (int) controlPoints.size(); ++i)
    {
        const float x = padding + controlPoints[i].x * graphWidth;
        const float y = padding + graphHeight - controlPoints[i].y * graphHeight;

        const float radius = 4.0f;
        g.setColour (getCurveColour());
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 0.0f, juce::jmax (0.0f, 360.0f - i * 72.0f));

        g.setColour (juce::Colour (0xff7dff5a));
        g.setFont (juce::Font (8.0f));
        const float labelX = x + 12.0f;
        const float labelY = y - 12.0f;
        g.drawEllipse (labelX - 3.0f, labelY - 3.0f, 6.0f, 6.0f, 0.0f, 60.0f);
    }

    // Curve
    const float pointsT = 0.0f;
    const float pointsX = controlPoints[0].x;
    const float pointsY = controlPoints[0].y;
    const float dx = pointsX - 0.0f;
    const float dy = pointsY - 0.0f;
    const float dist = std::sqrt (dx * dx + dy * dy);
    const float angle = std::atan2 (dy, dx);
    const float scale = std::min (1.0f, curveWidth / (dist + 0.0001f));
    const float sx = pointsX + dx * scale;
    const float sy = pointsY + dy * scale;

    const float lerpX = multipliers.x;
    const float lerpY = multipliers.y;
    const float lerpZ = multipliers.z;

    const float driftX = lerpX * (1.0f - sx);
    const float driftY = lerpY * (1.0f - sy);
    const float driftZ = lerpZ * (1.0f - sy);

    const float curveX = chaosValue;
    const auto curve = interpolate (controlPoints, chaosValue);
    const float curveY = curve.y;
    const float curveDx = chaosValue - curve.x;
    const float curveDy = curveY - curve.y;
    const float curveDist = std::sqrt (curveDx * curveDx + curveDy * curveDy);
    const float curveAngle = std::atan2 (curveDy, curveDx);
    const float curveScale = std::min (1.0f, curveWidth / (curveDist + 0.0001f));
    const float curveSx = chaosValue + curveDx * curveScale;
    const float curveSy = curveY + curveDy * curveScale;

    const float curveDriftX = lerpX * (1.0f - curveSx);
    const float curveDriftY = lerpY * (1.0f - curveSy);
    const float curveDriftZ = lerpZ * (1.0f - curveSy);

    const float previewX = lerpValue;
    const auto previewCurve = interpolate (controlPoints, previewX);
    const float previewY = previewCurve.y;
    const float previewDx = previewX - previewCurve.x;
    const float previewDy = previewY - previewCurve.y;
    const float previewDist = std::sqrt (previewDx * previewDx + previewDy * previewDy);
    const float previewAngle = std::atan2 (previewDy, previewDx);
    const float previewScale = std::min (1.0f, curveWidth / (previewDist + 0.0001f));
    const float previewSx = previewX + previewDx * previewScale;
    const float previewSy = previewY + previewDy * previewScale;

    const float previewDriftX = lerpX * (1.0f - previewSx);
    const float previewDriftY = lerpY * (1.0f - previewSy);
    const float previewDriftZ = lerpZ * (1.0f - previewSy);

    if (viewMode == ViewMode::Preview)
    {
        const float yPerAxis = height / 3.0f;
        const float xOffset = 10.0f;
        const float yOffset = height / 2.0f;

        g.setColour (juce::Colour (0xff333744));
        g.drawRect (xOffset, yOffset - yPerAxis * 2, width - 2 * xOffset, yPerAxis * 2, 1.0f);
        g.drawRect (xOffset, yOffset - yPerAxis, width - 2 * xOffset, yPerAxis, 1.0f);
        g.drawRect (xOffset, yOffset, width - 2 * xOffset, yPerAxis, 1.0f);

        g.setFont (juce::Font (8.0f));
        g.setColour (juce::Colour (0xff555866));
        g.drawText (juce::String (driftX * 100.0f) + "%  (timing)", xOffset, yOffset - yPerAxis * 1.7f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftY * 100.0f) + "%  (pitch)", xOffset, yOffset - yPerAxis * 0.5f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftZ * 100.0f) + "%  (velocity)", xOffset, yOffset + yPerAxis * 0.3f, width - 2 * xOffset, 12.0f, juce::Justification::centred);

        return;
    }

    // Axis labels
    g.setColour (juce::Colour (0xff555866));
    g.setFont (juce::Font (9.0f));

    // X axis (Chaos 0–1)
    g.drawText ("0", padding + graphWidth * 0.2f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.drawText ("1", padding + graphWidth * 0.8f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Chaos", padding, padding + graphHeight * 0.95f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Y axis (Drift 0–1)
    g.drawText ("0", padding, padding + graphHeight * 0.05f, juce::Justification::centred);
    g.drawText ("1", padding, padding + graphHeight * 0.85f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Drift", padding, padding + graphHeight * 0.05f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Control points
    for (int i = 0; i < (int) controlPoints.size(); ++i)
    {
        const float x = padding + controlPoints[i].x * graphWidth;
        const float y = padding + graphHeight - controlPoints[i].y * graphHeight;

        const float radius = 4.0f;
        g.setColour (getCurveColour());
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 0.0f, juce::jmax (0.0f, 360.0f - i * 72.0f));

        g.setColour (juce::Colour (0xff7dff5a));
        g.setFont (juce::Font (8.0f));
        const float labelX = x + 12.0f;
        const float labelY = y - 12.0f;
        g.drawEllipse (labelX - 3.0f, labelY - 3.0f, 6.0f, 6.0f, 0.0f, 60.0f);
    }

    // Curve
    const float pointsT = 0.0f;
    const float pointsX = controlPoints[0].x;
    const float pointsY = controlPoints[0].y;
    const float dx = pointsX - 0.0f;
    const float dy = pointsY - 0.0f;
    const float dist = std::sqrt (dx * dx + dy * dy);
    const float angle = std::atan2 (dy, dx);
    const float scale = std::min (1.0f, curveWidth / (dist + 0.0001f));
    const float sx = pointsX + dx * scale;
    const float sy = pointsY + dy * scale;

    const float lerpX = multipliers.x;
    const float lerpY = multipliers.y;
    const float lerpZ = multipliers.z;

    const float driftX = lerpX * (1.0f - sx);
    const float driftY = lerpY * (1.0f - sy);
    const float driftZ = lerpZ * (1.0f - sy);

    const float curveX = chaosValue;
    const auto curve = interpolate (controlPoints, chaosValue);
    const float curveY = curve.y;
    const float curveDx = chaosValue - curve.x;
    const float curveDy = curveY - curve.y;
    const float curveDist = std::sqrt (curveDx * curveDx + curveDy * curveDy);
    const float curveAngle = std::atan2 (curveDy, curveDx);
    const float curveScale = std::min (1.0f, curveWidth / (curveDist + 0.0001f));
    const float curveSx = chaosValue + curveDx * curveScale;
    const float curveSy = curveY + curveDy * curveScale;

    const float curveDriftX = lerpX * (1.0f - curveSx);
    const float curveDriftY = lerpY * (1.0f - curveSy);
    const float curveDriftZ = lerpZ * (1.0f - curveSy);

    const float previewX = lerpValue;
    const auto previewCurve = interpolate (controlPoints, previewX);
    const float previewY = previewCurve.y;
    const float previewDx = previewX - previewCurve.x;
    const float previewDy = previewY - previewCurve.y;
    const float previewDist = std::sqrt (previewDx * previewDx + previewDy * previewDy);
    const float previewAngle = std::atan2 (previewDy, previewDx);
    const float previewScale = std::min (1.0f, curveWidth / (previewDist + 0.0001f));
    const float previewSx = previewX + previewDx * previewScale;
    const float previewSy = previewY + previewDy * previewScale;

    const float previewDriftX = lerpX * (1.0f - previewSx);
    const float previewDriftY = lerpY * (1.0f - previewSy);
    const float previewDriftZ = lerpZ * (1.0f - previewSy);

    if (viewMode == ViewMode::Preview)
    {
        const float yPerAxis = height / 3.0f;
        const float xOffset = 10.0f;
        const float yOffset = height / 2.0f;

        g.setColour (juce::Colour (0xff333744));
        g.drawRect (xOffset, yOffset - yPerAxis * 2, width - 2 * xOffset, yPerAxis * 2, 1.0f);
        g.drawRect (xOffset, yOffset - yPerAxis, width - 2 * xOffset, yPerAxis, 1.0f);
        g.drawRect (xOffset, yOffset, width - 2 * xOffset, yPerAxis, 1.0f);

        g.setFont (juce::Font (8.0f));
        g.setColour (juce::Colour (0xff555866));
        g.drawText (juce::String (driftX * 100.0f) + "%  (timing)", xOffset, yOffset - yPerAxis * 1.7f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftY * 100.0f) + "%  (pitch)", xOffset, yOffset - yPerAxis * 0.5f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftZ * 100.0f) + "%  (velocity)", xOffset, yOffset + yPerAxis * 0.3f, width - 2 * xOffset, 12.0f, juce::Justification::centred);

        return;
    }

    // Axis labels
    g.setColour (juce::Colour (0xff555866));
    g.setFont (juce::Font (9.0f));

    // X axis (Chaos 0–1)
    g.drawText ("0", padding + graphWidth * 0.2f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.drawText ("1", padding + graphWidth * 0.8f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Chaos", padding, padding + graphHeight * 0.95f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Y axis (Drift 0–1)
    g.drawText ("0", padding, padding + graphHeight * 0.05f, juce::Justification::centred);
    g.drawText ("1", padding, padding + graphHeight * 0.85f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Drift", padding, padding + graphHeight * 0.05f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Control points
    for (int i = 0; i < (int) controlPoints.size(); ++i)
    {
        const float x = padding + controlPoints[i].x * graphWidth;
        const float y = padding + graphHeight - controlPoints[i].y * graphHeight;

        const float radius = 4.0f;
        g.setColour (getCurveColour());
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 0.0f, juce::jmax (0.0f, 360.0f - i * 72.0f));

        g.setColour (juce::Colour (0xff7dff5a));
        g.setFont (juce::Font (8.0f));
        const float labelX = x + 12.0f;
        const float labelY = y - 12.0f;
        g.drawEllipse (labelX - 3.0f, labelY - 3.0f, 6.0f, 6.0f, 0.0f, 60.0f);
    }

    // Curve
    const float pointsT = 0.0f;
    const float pointsX = controlPoints[0].x;
    const float pointsY = controlPoints[0].y;
    const float dx = pointsX - 0.0f;
    const float dy = pointsY - 0.0f;
    const float dist = std::sqrt (dx * dx + dy * dy);
    const float angle = std::atan2 (dy, dx);
    const float scale = std::min (1.0f, curveWidth / (dist + 0.0001f));
    const float sx = pointsX + dx * scale;
    const float sy = pointsY + dy * scale;

    const float lerpX = multipliers.x;
    const float lerpY = multipliers.y;
    const float lerpZ = multipliers.z;

    const float driftX = lerpX * (1.0f - sx);
    const float driftY = lerpY * (1.0f - sy);
    const float driftZ = lerpZ * (1.0f - sy);

    const float curveX = chaosValue;
    const auto curve = interpolate (controlPoints, chaosValue);
    const float curveY = curve.y;
    const float curveDx = chaosValue - curve.x;
    const float curveDy = curveY - curve.y;
    const float curveDist = std::sqrt (curveDx * curveDx + curveDy * curveDy);
    const float curveAngle = std::atan2 (curveDy, curveDx);
    const float curveScale = std::min (1.0f, curveWidth / (curveDist + 0.0001f));
    const float curveSx = chaosValue + curveDx * curveScale;
    const float curveSy = curveY + curveDy * curveScale;

    const float curveDriftX = lerpX * (1.0f - curveSx);
    const float curveDriftY = lerpY * (1.0f - curveSy);
    const float curveDriftZ = lerpZ * (1.0f - curveSy);

    const float previewX = lerpValue;
    const auto previewCurve = interpolate (controlPoints, previewX);
    const float previewY = previewCurve.y;
    const float previewDx = previewX - previewCurve.x;
    const float previewDy = previewY - previewCurve.y;
    const float previewDist = std::sqrt (previewDx * previewDx + previewDy * previewDy);
    const float previewAngle = std::atan2 (previewDy, previewDx);
    const float previewScale = std::min (1.0f, curveWidth / (previewDist + 0.0001f));
    const float previewSx = previewX + previewDx * previewScale;
    const float previewSy = previewY + previewDy * previewScale;

    const float previewDriftX = lerpX * (1.0f - previewSx);
    const float previewDriftY = lerpY * (1.0f - previewSy);
    const float previewDriftZ = lerpZ * (1.0f - previewSy);

    if (viewMode == ViewMode::Preview)
    {
        const float yPerAxis = height / 3.0f;
        const float xOffset = 10.0f;
        const float yOffset = height / 2.0f;

        g.setColour (juce::Colour (0xff333744));
        g.drawRect (xOffset, yOffset - yPerAxis * 2, width - 2 * xOffset, yPerAxis * 2, 1.0f);
        g.drawRect (xOffset, yOffset - yPerAxis, width - 2 * xOffset, yPerAxis, 1.0f);
        g.drawRect (xOffset, yOffset, width - 2 * xOffset, yPerAxis, 1.0f);

        g.setFont (juce::Font (8.0f));
        g.setColour (juce::Colour (0xff555866));
        g.drawText (juce::String (driftX * 100.0f) + "%  (timing)", xOffset, yOffset - yPerAxis * 1.7f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftY * 100.0f) + "%  (pitch)", xOffset, yOffset - yPerAxis * 0.5f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftZ * 100.0f) + "%  (velocity)", xOffset, yOffset + yPerAxis * 0.3f, width - 2 * xOffset, 12.0f, juce::Justification::centred);

        return;
    }

    // Axis labels
    g.setColour (juce::Colour (0xff555866));
    g.setFont (juce::Font (9.0f));

    // X axis (Chaos 0–1)
    g.drawText ("0", padding + graphWidth * 0.2f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.drawText ("1", padding + graphWidth * 0.8f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Chaos", padding, padding + graphHeight * 0.95f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Y axis (Drift 0–1)
    g.drawText ("0", padding, padding + graphHeight * 0.05f, juce::Justification::centred);
    g.drawText ("1", padding, padding + graphHeight * 0.85f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Drift", padding, padding + graphHeight * 0.05f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Control points
    for (int i = 0; i < (int) controlPoints.size(); ++i)
    {
        const float x = padding + controlPoints[i].x * graphWidth;
        const float y = padding + graphHeight - controlPoints[i].y * graphHeight;

        const float radius = 4.0f;
        g.setColour (getCurveColour());
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 0.0f, juce::jmax (0.0f, 360.0f - i * 72.0f));

        g.setColour (juce::Colour (0xff7dff5a));
        g.setFont (juce::Font (8.0f));
        const float labelX = x + 12.0f;
        const float labelY = y - 12.0f;
        g.drawEllipse (labelX - 3.0f, labelY - 3.0f, 6.0f, 6.0f, 0.0f, 60.0f);
    }

    // Curve
    const float pointsT = 0.0f;
    const float pointsX = controlPoints[0].x;
    const float pointsY = controlPoints[0].y;
    const float dx = pointsX - 0.0f;
    const float dy = pointsY - 0.0f;
    const float dist = std::sqrt (dx * dx + dy * dy);
    const float angle = std::atan2 (dy, dx);
    const float scale = std::min (1.0f, curveWidth / (dist + 0.0001f));
    const float sx = pointsX + dx * scale;
    const float sy = pointsY + dy * scale;

    const float lerpX = multipliers.x;
    const float lerpY = multipliers.y;
    const float lerpZ = multipliers.z;

    const float driftX = lerpX * (1.0f - sx);
    const float driftY = lerpY * (1.0f - sy);
    const float driftZ = lerpZ * (1.0f - sy);

    const float curveX = chaosValue;
    const auto curve = interpolate (controlPoints, chaosValue);
    const float curveY = curve.y;
    const float curveDx = chaosValue - curve.x;
    const float curveDy = curveY - curve.y;
    const float curveDist = std::sqrt (curveDx * curveDx + curveDy * curveDy);
    const float curveAngle = std::atan2 (curveDy, curveDx);
    const float curveScale = std::min (1.0f, curveWidth / (curveDist + 0.0001f));
    const float curveSx = chaosValue + curveDx * curveScale;
    const float curveSy = curveY + curveDy * curveScale;

    const float curveDriftX = lerpX * (1.0f - curveSx);
    const float curveDriftY = lerpY * (1.0f - curveSy);
    const float curveDriftZ = lerpZ * (1.0f - curveSy);

    const float previewX = lerpValue;
    const auto previewCurve = interpolate (controlPoints, previewX);
    const float previewY = previewCurve.y;
    const float previewDx = previewX - previewCurve.x;
    const float previewDy = previewY - previewCurve.y;
    const float previewDist = std::sqrt (previewDx * previewDx + previewDy * previewDy);
    const float previewAngle = std::atan2 (previewDy, previewDx);
    const float previewScale = std::min (1.0f, curveWidth / (previewDist + 0.0001f));
    const float previewSx = previewX + previewDx * previewScale;
    const float previewSy = previewY + previewDy * previewScale;

    const float previewDriftX = lerpX * (1.0f - previewSx);
    const float previewDriftY = lerpY * (1.0f - previewSy);
    const float previewDriftZ = lerpZ * (1.0f - previewSy);

    if (viewMode == ViewMode::Preview)
    {
        const float yPerAxis = height / 3.0f;
        const float xOffset = 10.0f;
        const float yOffset = height / 2.0f;

        g.setColour (juce::Colour (0xff333744));
        g.drawRect (xOffset, yOffset - yPerAxis * 2, width - 2 * xOffset, yPerAxis * 2, 1.0f);
        g.drawRect (xOffset, yOffset - yPerAxis, width - 2 * xOffset, yPerAxis, 1.0f);
        g.drawRect (xOffset, yOffset, width - 2 * xOffset, yPerAxis, 1.0f);

        g.setFont (juce::Font (8.0f));
        g.setColour (juce::Colour (0xff555866));
        g.drawText (juce::String (driftX * 100.0f) + "%  (timing)", xOffset, yOffset - yPerAxis * 1.7f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftY * 100.0f) + "%  (pitch)", xOffset, yOffset - yPerAxis * 0.5f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftZ * 100.0f) + "%  (velocity)", xOffset, yOffset + yPerAxis * 0.3f, width - 2 * xOffset, 12.0f, juce::Justification::centred);

        return;
    }

    // Axis labels
    g.setColour (juce::Colour (0xff555866));
    g.setFont (juce::Font (9.0f));

    // X axis (Chaos 0–1)
    g.drawText ("0", padding + graphWidth * 0.2f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.drawText ("1", padding + graphWidth * 0.8f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Chaos", padding, padding + graphHeight * 0.95f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Y axis (Drift 0–1)
    g.drawText ("0", padding, padding + graphHeight * 0.05f, juce::Justification::centred);
    g.drawText ("1", padding, padding + graphHeight * 0.85f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Drift", padding, padding + graphHeight * 0.05f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Control points
    for (int i = 0; i < (int) controlPoints.size(); ++i)
    {
        const float x = padding + controlPoints[i].x * graphWidth;
        const float y = padding + graphHeight - controlPoints[i].y * graphHeight;

        const float radius = 4.0f;
        g.setColour (getCurveColour());
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 0.0f, juce::jmax (0.0f, 360.0f - i * 72.0f));

        g.setColour (juce::Colour (0xff7dff5a));
        g.setFont (juce::Font (8.0f));
        const float labelX = x + 12.0f;
        const float labelY = y - 12.0f;
        g.drawEllipse (labelX - 3.0f, labelY - 3.0f, 6.0f, 6.0f, 0.0f, 60.0f);
    }

    // Curve
    const float pointsT = 0.0f;
    const float pointsX = controlPoints[0].x;
    const float pointsY = controlPoints[0].y;
    const float dx = pointsX - 0.0f;
    const float dy = pointsY - 0.0f;
    const float dist = std::sqrt (dx * dx + dy * dy);
    const float angle = std::atan2 (dy, dx);
    const float scale = std::min (1.0f, curveWidth / (dist + 0.0001f));
    const float sx = pointsX + dx * scale;
    const float sy = pointsY + dy * scale;

    const float lerpX = multipliers.x;
    const float lerpY = multipliers.y;
    const float lerpZ = multipliers.z;

    const float driftX = lerpX * (1.0f - sx);
    const float driftY = lerpY * (1.0f - sy);
    const float driftZ = lerpZ * (1.0f - sy);

    const float curveX = chaosValue;
    const auto curve = interpolate (controlPoints, chaosValue);
    const float curveY = curve.y;
    const float curveDx = chaosValue - curve.x;
    const float curveDy = curveY - curve.y;
    const float curveDist = std::sqrt (curveDx * curveDx + curveDy * curveDy);
    const float curveAngle = std::atan2 (curveDy, curveDx);
    const float curveScale = std::min (1.0f, curveWidth / (curveDist + 0.0001f));
    const float curveSx = chaosValue + curveDx * curveScale;
    const float curveSy = curveY + curveDy * curveScale;

    const float curveDriftX = lerpX * (1.0f - curveSx);
    const float curveDriftY = lerpY * (1.0f - curveSy);
    const float curveDriftZ = lerpZ * (1.0f - curveSy);

    const float previewX = lerpValue;
    const auto previewCurve = interpolate (controlPoints, previewX);
    const float previewY = previewCurve.y;
    const float previewDx = previewX - previewCurve.x;
    const float previewDy = previewY - previewCurve.y;
    const float previewDist = std::sqrt (previewDx * previewDx + previewDy * previewDy);
    const float previewAngle = std::atan2 (previewDy, previewDx);
    const float previewScale = std::min (1.0f, curveWidth / (previewDist + 0.0001f));
    const float previewSx = previewX + previewDx * previewScale;
    const float previewSy = previewY + previewDy * previewScale;

    const float previewDriftX = lerpX * (1.0f - previewSx);
    const float previewDriftY = lerpY * (1.0f - previewSy);
    const float previewDriftZ = lerpZ * (1.0f - previewSy);

    if (viewMode == ViewMode::Preview)
    {
        const float yPerAxis = height / 3.0f;
        const float xOffset = 10.0f;
        const float yOffset = height / 2.0f;

        g.setColour (juce::Colour (0xff333744));
        g.drawRect (xOffset, yOffset - yPerAxis * 2, width - 2 * xOffset, yPerAxis * 2, 1.0f);
        g.drawRect (xOffset, yOffset - yPerAxis, width - 2 * xOffset, yPerAxis, 1.0f);
        g.drawRect (xOffset, yOffset, width - 2 * xOffset, yPerAxis, 1.0f);

        g.setFont (juce::Font (8.0f));
        g.setColour (juce::Colour (0xff555866));
        g.drawText (juce::String (driftX * 100.0f) + "%  (timing)", xOffset, yOffset - yPerAxis * 1.7f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftY * 100.0f) + "%  (pitch)", xOffset, yOffset - yPerAxis * 0.5f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftZ * 100.0f) + "%  (velocity)", xOffset, yOffset + yPerAxis * 0.3f, width - 2 * xOffset, 12.0f, juce::Justification::centred);

        return;
    }

    // Axis labels
    g.setColour (juce::Colour (0xff555866));
    g.setFont (juce::Font (9.0f));

    // X axis (Chaos 0–1)
    g.drawText ("0", padding + graphWidth * 0.2f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.drawText ("1", padding + graphWidth * 0.8f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Chaos", padding, padding + graphHeight * 0.95f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Y axis (Drift 0–1)
    g.drawText ("0", padding, padding + graphHeight * 0.05f, juce::Justification::centred);
    g.drawText ("1", padding, padding + graphHeight * 0.85f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Drift", padding, padding + graphHeight * 0.05f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Control points
    for (int i = 0; i < (int) controlPoints.size(); ++i)
    {
        const float x = padding + controlPoints[i].x * graphWidth;
        const float y = padding + graphHeight - controlPoints[i].y * graphHeight;

        const float radius = 4.0f;
        g.setColour (getCurveColour());
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 0.0f, juce::jmax (0.0f, 360.0f - i * 72.0f));

        g.setColour (juce::Colour (0xff7dff5a));
        g.setFont (juce::Font (8.0f));
        const float labelX = x + 12.0f;
        const float labelY = y - 12.0f;
        g.drawEllipse (labelX - 3.0f, labelY - 3.0f, 6.0f, 6.0f, 0.0f, 60.0f);
    }

    // Curve
    const float pointsT = 0.0f;
    const float pointsX = controlPoints[0].x;
    const float pointsY = controlPoints[0].y;
    const float dx = pointsX - 0.0f;
    const float dy = pointsY - 0.0f;
    const float dist = std::sqrt (dx * dx + dy * dy);
    const float angle = std::atan2 (dy, dx);
    const float scale = std::min (1.0f, curveWidth / (dist + 0.0001f));
    const float sx = pointsX + dx * scale;
    const float sy = pointsY + dy * scale;

    const float lerpX = multipliers.x;
    const float lerpY = multipliers.y;
    const float lerpZ = multipliers.z;

    const float driftX = lerpX * (1.0f - sx);
    const float driftY = lerpY * (1.0f - sy);
    const float driftZ = lerpZ * (1.0f - sy);

    const float curveX = chaosValue;
    const auto curve = interpolate (controlPoints, chaosValue);
    const float curveY = curve.y;
    const float curveDx = chaosValue - curve.x;
    const float curveDy = curveY - curve.y;
    const float curveDist = std::sqrt (curveDx * curveDx + curveDy * curveDy);
    const float curveAngle = std::atan2 (curveDy, curveDx);
    const float curveScale = std::min (1.0f, curveWidth / (curveDist + 0.0001f));
    const float curveSx = chaosValue + curveDx * curveScale;
    const float curveSy = curveY + curveDy * curveScale;

    const float curveDriftX = lerpX * (1.0f - curveSx);
    const float curveDriftY = lerpY * (1.0f - curveSy);
    const float curveDriftZ = lerpZ * (1.0f - curveSy);

    const float previewX = lerpValue;
    const auto previewCurve = interpolate (controlPoints, previewX);
    const float previewY = previewCurve.y;
    const float previewDx = previewX - previewCurve.x;
    const float previewDy = previewY - previewCurve.y;
    const float previewDist = std::sqrt (previewDx * previewDx + previewDy * previewDy);
    const float previewAngle = std::atan2 (previewDy, previewDx);
    const float previewScale = std::min (1.0f, curveWidth / (previewDist + 0.0001f));
    const float previewSx = previewX + previewDx * previewScale;
    const float previewSy = previewY + previewDy * previewScale;

    const float previewDriftX = lerpX * (1.0f - previewSx);
    const float previewDriftY = lerpY * (1.0f - previewSy);
    const float previewDriftZ = lerpZ * (1.0f - previewSy);

    if (viewMode == ViewMode::Preview)
    {
        const float yPerAxis = height / 3.0f;
        const float xOffset = 10.0f;
        const float yOffset = height / 2.0f;

        g.setColour (juce::Colour (0xff333744));
        g.drawRect (xOffset, yOffset - yPerAxis * 2, width - 2 * xOffset, yPerAxis * 2, 1.0f);
        g.drawRect (xOffset, yOffset - yPerAxis, width - 2 * xOffset, yPerAxis, 1.0f);
        g.drawRect (xOffset, yOffset, width - 2 * xOffset, yPerAxis, 1.0f);

        g.setFont (juce::Font (8.0f));
        g.setColour (juce::Colour (0xff555866));
        g.drawText (juce::String (driftX * 100.0f) + "%  (timing)", xOffset, yOffset - yPerAxis * 1.7f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftY * 100.0f) + "%  (pitch)", xOffset, yOffset - yPerAxis * 0.5f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftZ * 100.0f) + "%  (velocity)", xOffset, yOffset + yPerAxis * 0.3f, width - 2 * xOffset, 12.0f, juce::Justification::centred);

        return;
    }

    // Axis labels
    g.setColour (juce::Colour (0xff555866));
    g.setFont (juce::Font (9.0f));

    // X axis (Chaos 0–1)
    g.drawText ("0", padding + graphWidth * 0.2f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.drawText ("1", padding + graphWidth * 0.8f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Chaos", padding, padding + graphHeight * 0.95f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Y axis (Drift 0–1)
    g.drawText ("0", padding, padding + graphHeight * 0.05f, juce::Justification::centred);
    g.drawText ("1", padding, padding + graphHeight * 0.85f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Drift", padding, padding + graphHeight * 0.05f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Control points
    for (int i = 0; i < (int) controlPoints.size(); ++i)
    {
        const float x = padding + controlPoints[i].x * graphWidth;
        const float y = padding + graphHeight - controlPoints[i].y * graphHeight;

        const float radius = 4.0f;
        g.setColour (getCurveColour());
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 0.0f, juce::jmax (0.0f, 360.0f - i * 72.0f));

        g.setColour (juce::Colour (0xff7dff5a));
        g.setFont (juce::Font (8.0f));
        const float labelX = x + 12.0f;
        const float labelY = y - 12.0f;
        g.drawEllipse (labelX - 3.0f, labelY - 3.0f, 6.0f, 6.0f, 0.0f, 60.0f);
    }

    // Curve
    const float pointsT = 0.0f;
    const float pointsX = controlPoints[0].x;
    const float pointsY = controlPoints[0].y;
    const float dx = pointsX - 0.0f;
    const float dy = pointsY - 0.0f;
    const float dist = std::sqrt (dx * dx + dy * dy);
    const float angle = std::atan2 (dy, dx);
    const float scale = std::min (1.0f, curveWidth / (dist + 0.0001f));
    const float sx = pointsX + dx * scale;
    const float sy = pointsY + dy * scale;

    const float lerpX = multipliers.x;
    const float lerpY = multipliers.y;
    const float lerpZ = multipliers.z;

    const float driftX = lerpX * (1.0f - sx);
    const float driftY = lerpY * (1.0f - sy);
    const float driftZ = lerpZ * (1.0f - sy);

    const float curveX = chaosValue;
    const auto curve = interpolate (controlPoints, chaosValue);
    const float curveY = curve.y;
    const float curveDx = chaosValue - curve.x;
    const float curveDy = curveY - curve.y;
    const float curveDist = std::sqrt (curveDx * curveDx + curveDy * curveDy);
    const float curveAngle = std::atan2 (curveDy, curveDx);
    const float curveScale = std::min (1.0f, curveWidth / (curveDist + 0.0001f));
    const float curveSx = chaosValue + curveDx * curveScale;
    const float curveSy = curveY + curveDy * curveScale;

    const float curveDriftX = lerpX * (1.0f - curveSx);
    const float curveDriftY = lerpY * (1.0f - curveSy);
    const float curveDriftZ = lerpZ * (1.0f - curveSy);

    const float previewX = lerpValue;
    const auto previewCurve = interpolate (controlPoints, previewX);
    const float previewY = previewCurve.y;
    const float previewDx = previewX - previewCurve.x;
    const float previewDy = previewY - previewCurve.y;
    const float previewDist = std::sqrt (previewDx * previewDx + previewDy * previewDy);
    const float previewAngle = std::atan2 (previewDy, previewDx);
    const float previewScale = std::min (1.0f, curveWidth / (previewDist + 0.0001f));
    const float previewSx = previewX + previewDx * previewScale;
    const float previewSy = previewY + previewDy * previewScale;

    const float previewDriftX = lerpX * (1.0f - previewSx);
    const float previewDriftY = lerpY * (1.0f - previewSy);
    const float previewDriftZ = lerpZ * (1.0f - previewSy);

    if (viewMode == ViewMode::Preview)
    {
        const float yPerAxis = height / 3.0f;
        const float xOffset = 10.0f;
        const float yOffset = height / 2.0f;

        g.setColour (juce::Colour (0xff333744));
        g.drawRect (xOffset, yOffset - yPerAxis * 2, width - 2 * xOffset, yPerAxis * 2, 1.0f);
        g.drawRect (xOffset, yOffset - yPerAxis, width - 2 * xOffset, yPerAxis, 1.0f);
        g.drawRect (xOffset, yOffset, width - 2 * xOffset, yPerAxis, 1.0f);

        g.setFont (juce::Font (8.0f));
        g.setColour (juce::Colour (0xff555866));
        g.drawText (juce::String (driftX * 100.0f) + "%  (timing)", xOffset, yOffset - yPerAxis * 1.7f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftY * 100.0f) + "%  (pitch)", xOffset, yOffset - yPerAxis * 0.5f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftZ * 100.0f) + "%  (velocity)", xOffset, yOffset + yPerAxis * 0.3f, width - 2 * xOffset, 12.0f, juce::Justification::centred);

        return;
    }

    // Axis labels
    g.setColour (juce::Colour (0xff555866));
    g.setFont (juce::Font (9.0f));

    // X axis (Chaos 0–1)
    g.drawText ("0", padding + graphWidth * 0.2f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.drawText ("1", padding + graphWidth * 0.8f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Chaos", padding, padding + graphHeight * 0.95f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Y axis (Drift 0–1)
    g.drawText ("0", padding, padding + graphHeight * 0.05f, juce::Justification::centred);
    g.drawText ("1", padding, padding + graphHeight * 0.85f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Drift", padding, padding + graphHeight * 0.05f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Control points
    for (int i = 0; i < (int) controlPoints.size(); ++i)
    {
        const float x = padding + controlPoints[i].x * graphWidth;
        const float y = padding + graphHeight - controlPoints[i].y * graphHeight;

        const float radius = 4.0f;
        g.setColour (getCurveColour());
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 0.0f, juce::jmax (0.0f, 360.0f - i * 72.0f));

        g.setColour (juce::Colour (0xff7dff5a));
        g.setFont (juce::Font (8.0f));
        const float labelX = x + 12.0f;
        const float labelY = y - 12.0f;
        g.drawEllipse (labelX - 3.0f, labelY - 3.0f, 6.0f, 6.0f, 0.0f, 60.0f);
    }

    // Curve
    const float pointsT = 0.0f;
    const float pointsX = controlPoints[0].x;
    const float pointsY = controlPoints[0].y;
    const float dx = pointsX - 0.0f;
    const float dy = pointsY - 0.0f;
    const float dist = std::sqrt (dx * dx + dy * dy);
    const float angle = std::atan2 (dy, dx);
    const float scale = std::min (1.0f, curveWidth / (dist + 0.0001f));
    const float sx = pointsX + dx * scale;
    const float sy = pointsY + dy * scale;

    const float lerpX = multipliers.x;
    const float lerpY = multipliers.y;
    const float lerpZ = multipliers.z;

    const float driftX = lerpX * (1.0f - sx);
    const float driftY = lerpY * (1.0f - sy);
    const float driftZ = lerpZ * (1.0f - sy);

    const float curveX = chaosValue;
    const auto curve = interpolate (controlPoints, chaosValue);
    const float curveY = curve.y;
    const float curveDx = chaosValue - curve.x;
    const float curveDy = curveY - curve.y;
    const float curveDist = std::sqrt (curveDx * curveDx + curveDy * curveDy);
    const float curveAngle = std::atan2 (curveDy, curveDx);
    const float curveScale = std::min (1.0f, curveWidth / (curveDist + 0.0001f));
    const float curveSx = chaosValue + curveDx * curveScale;
    const float curveSy = curveY + curveDy * curveScale;

    const float curveDriftX = lerpX * (1.0f - curveSx);
    const float curveDriftY = lerpY * (1.0f - curveSy);
    const float curveDriftZ = lerpZ * (1.0f - curveSy);

    const float previewX = lerpValue;
    const auto previewCurve = interpolate (controlPoints, previewX);
    const float previewY = previewCurve.y;
    const float previewDx = previewX - previewCurve.x;
    const float previewDy = previewY - previewCurve.y;
    const float previewDist = std::sqrt (previewDx * previewDx + previewDy * previewDy);
    const float previewAngle = std::atan2 (previewDy, previewDx);
    const float previewScale = std::min (1.0f, curveWidth / (previewDist + 0.0001f));
    const float previewSx = previewX + previewDx * previewScale;
    const float previewSy = previewY + previewDy * previewScale;

    const float previewDriftX = lerpX * (1.0f - previewSx);
    const float previewDriftY = lerpY * (1.0f - previewSy);
    const float previewDriftZ = lerpZ * (1.0f - previewSy);

    if (viewMode == ViewMode::Preview)
    {
        const float yPerAxis = height / 3.0f;
        const float xOffset = 10.0f;
        const float yOffset = height / 2.0f;

        g.setColour (juce::Colour (0xff333744));
        g.drawRect (xOffset, yOffset - yPerAxis * 2, width - 2 * xOffset, yPerAxis * 2, 1.0f);
        g.drawRect (xOffset, yOffset - yPerAxis, width - 2 * xOffset, yPerAxis, 1.0f);
        g.drawRect (xOffset, yOffset, width - 2 * xOffset, yPerAxis, 1.0f);

        g.setFont (juce::Font (8.0f));
        g.setColour (juce::Colour (0xff555866));
        g.drawText (juce::String (driftX * 100.0f) + "%  (timing)", xOffset, yOffset - yPerAxis * 1.7f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftY * 100.0f) + "%  (pitch)", xOffset, yOffset - yPerAxis * 0.5f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftZ * 100.0f) + "%  (velocity)", xOffset, yOffset + yPerAxis * 0.3f, width - 2 * xOffset, 12.0f, juce::Justification::centred);

        return;
    }

    // Axis labels
    g.setColour (juce::Colour (0xff555866));
    g.setFont (juce::Font (9.0f));

    // X axis (Chaos 0–1)
    g.drawText ("0", padding + graphWidth * 0.2f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.drawText ("1", padding + graphWidth * 0.8f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Chaos", padding, padding + graphHeight * 0.95f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Y axis (Drift 0–1)
    g.drawText ("0", padding, padding + graphHeight * 0.05f, juce::Justification::centred);
    g.drawText ("1", padding, padding + graphHeight * 0.85f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Drift", padding, padding + graphHeight * 0.05f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Control points
    for (int i = 0; i < (int) controlPoints.size(); ++i)
    {
        const float x = padding + controlPoints[i].x * graphWidth;
        const float y = padding + graphHeight - controlPoints[i].y * graphHeight;

        const float radius = 4.0f;
        g.setColour (getCurveColour());
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 0.0f, juce::jmax (0.0f, 360.0f - i * 72.0f));

        g.setColour (juce::Colour (0xff7dff5a));
        g.setFont (juce::Font (8.0f));
        const float labelX = x + 12.0f;
        const float labelY = y - 12.0f;
        g.drawEllipse (labelX - 3.0f, labelY - 3.0f, 6.0f, 6.0f, 0.0f, 60.0f);
    }

    // Curve
    const float pointsT = 0.0f;
    const float pointsX = controlPoints[0].x;
    const float pointsY = controlPoints[0].y;
    const float dx = pointsX - 0.0f;
    const float dy = pointsY - 0.0f;
    const float dist = std::sqrt (dx * dx + dy * dy);
    const float angle = std::atan2 (dy, dx);
    const float scale = std::min (1.0f, curveWidth / (dist + 0.0001f));
    const float sx = pointsX + dx * scale;
    const float sy = pointsY + dy * scale;

    const float lerpX = multipliers.x;
    const float lerpY = multipliers.y;
    const float lerpZ = multipliers.z;

    const float driftX = lerpX * (1.0f - sx);
    const float driftY = lerpY * (1.0f - sy);
    const float driftZ = lerpZ * (1.0f - sy);

    const float curveX = chaosValue;
    const auto curve = interpolate (controlPoints, chaosValue);
    const float curveY = curve.y;
    const float curveDx = chaosValue - curve.x;
    const float curveDy = curveY - curve.y;
    const float curveDist = std::sqrt (curveDx * curveDx + curveDy * curveDy);
    const float curveAngle = std::atan2 (curveDy, curveDx);
    const float curveScale = std::min (1.0f, curveWidth / (curveDist + 0.0001f));
    const float curveSx = chaosValue + curveDx * curveScale;
    const float curveSy = curveY + curveDy * curveScale;

    const float curveDriftX = lerpX * (1.0f - curveSx);
    const float curveDriftY = lerpY * (1.0f - curveSy);
    const float curveDriftZ = lerpZ * (1.0f - curveSy);

    const float previewX = lerpValue;
    const auto previewCurve = interpolate (controlPoints, previewX);
    const float previewY = previewCurve.y;
    const float previewDx = previewX - previewCurve.x;
    const float previewDy = previewY - previewCurve.y;
    const float previewDist = std::sqrt (previewDx * previewDx + previewDy * previewDy);
    const float previewAngle = std::atan2 (previewDy, previewDx);
    const float previewScale = std::min (1.0f, curveWidth / (previewDist + 0.0001f));
    const float previewSx = previewX + previewDx * previewScale;
    const float previewSy = previewY + previewDy * previewScale;

    const float previewDriftX = lerpX * (1.0f - previewSx);
    const float previewDriftY = lerpY * (1.0f - previewSy);
    const float previewDriftZ = lerpZ * (1.0f - previewSy);

    if (viewMode == ViewMode::Preview)
    {
        const float yPerAxis = height / 3.0f;
        const float xOffset = 10.0f;
        const float yOffset = height / 2.0f;

        g.setColour (juce::Colour (0xff333744));
        g.drawRect (xOffset, yOffset - yPerAxis * 2, width - 2 * xOffset, yPerAxis * 2, 1.0f);
        g.drawRect (xOffset, yOffset - yPerAxis, width - 2 * xOffset, yPerAxis, 1.0f);
        g.drawRect (xOffset, yOffset, width - 2 * xOffset, yPerAxis, 1.0f);

        g.setFont (juce::Font (8.0f));
        g.setColour (juce::Colour (0xff555866));
        g.drawText (juce::String (driftX * 100.0f) + "%  (timing)", xOffset, yOffset - yPerAxis * 1.7f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftY * 100.0f) + "%  (pitch)", xOffset, yOffset - yPerAxis * 0.5f, width - 2 * xOffset, 12.0f, juce::Justification::centred);
        g.drawText (juce::String (driftZ * 100.0f) + "%  (velocity)", xOffset, yOffset + yPerAxis * 0.3f, width - 2 * xOffset, 12.0f, juce::Justification::centred);

        return;
    }

    // Axis labels
    g.setColour (juce::Colour (0xff555866));
    g.setFont (juce::Font (9.0f));

    // X axis (Chaos 0–1)
    g.drawText ("0", padding + graphWidth * 0.2f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.drawText ("1", padding + graphWidth * 0.8f, padding + graphHeight * 0.95f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Chaos", padding, padding + graphHeight * 0.95f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Y axis (Drift 0–1)
    g.drawText ("0", padding, padding + graphHeight * 0.05f, juce::Justification::centred);
    g.drawText ("1", padding, padding + graphHeight * 0.85f, juce::Justification::centred);
    g.setFont (juce::Font (7.0f));
    g.drawText ("Drift", padding, padding + graphHeight * 0.05f, graphWidth * 0.6f, 14.0f, juce::Justification::centred);

    // Control points
    for (int i = 0; i < (int) controlPoints.size(); ++i)
    {
        const float x = padding + controlPoints[i].x * graphWidth;
        const float y = padding + graphHeight - controlPoints[i].y * graphHeight;

        const float radius = 4.0f;
        g.setColour (getCurveColour());
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 0.0f, juce::jmax (0.0f, 360.0f - i * 72.0f));

        g.setColour (juce::Colour (0xff7dff5a));
        g.setFont (juce::Font (8.0f));
        const float labelX = x + 12.0f;
        const float labelY = y - 12.0f;
        g.drawEllipse (labelX - 3.0f, labelY - 3.0f, 6.0f, 6.0f, 0.0f, 60.0f);
    }
}
