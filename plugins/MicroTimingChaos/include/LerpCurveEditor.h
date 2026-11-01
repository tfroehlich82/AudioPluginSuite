#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class LerpCurveEditor : public juce::Component
{
public:
    enum class ViewMode { Curve, Preview };

    explicit LerpCurveEditor (ViewMode mode = ViewMode::Curve);

    void setViewMode (ViewMode mode);
    ViewMode getViewMode() const noexcept;

    void setControlPoints (const std::vector<juce::Point<float>>& points);
    std::vector<juce::Point<float>> getControlPoints() const;

    void setMultipliers (const juce::Point<float>& multipliers);
    juce::Point<float> getMultipliers() const;

    void setLerpValue (float value);
    float getLerpValue() const;

    void setChaosValue (float value);
    float getChaosValue() const;

    juce::Colour getCurveColour() const noexcept;
    juce::Colour getMultiplierColour(int axis) const noexcept;

protected:
    void paint (juce::Graphics& g) override;

private:
    ViewMode viewMode;
    std::vector<juce::Point<float>> controlPoints;
    juce::Point<float> multipliers = { 1.0f, 1.0f, 1.0f };
    float lerpValue = 0.5f;
    float chaosValue = 0.5f;

    std::unique_ptr<juce::Slider> lerpSlider;
    std::unique_ptr<juce::Slider> chaosSlider;

    void updateCurve();
    juce::Point<float> evaluateCurve (float t) const;
};
