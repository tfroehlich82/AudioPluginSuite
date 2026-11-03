#include "LfoScope.h"

#include "MicroTimingChaosLookAndFeel.h"

namespace
{
    namespace Palette = MicroTimingChaos::Palette;

    constexpr int waveformPoints = 64;
}

LfoScope::LfoScope (MicroTimingChaosProcessor& p, int index, const char* shapeParameterId, juce::Colour c)
    : processor (p),
      lfoIndex (index),
      shapeId (shapeParameterId),
      colour (c)
{
    setOpaque (false);
}

void LfoScope::refresh()
{
    const float phase = processor.getLfoPhase (lfoIndex);
    const float value = processor.getLfoValue (lfoIndex);

    if (phase != shownPhase || value != shownValue)
    {
        shownPhase = phase;
        shownValue = value;
        repaint();
    }
}

void LfoScope::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto area = bounds.reduced (6.0f, 5.0f);
    const auto midLine = area.getCentreY();
    const auto shape = static_cast<CoreDSP::Modulation::LfoShape> (
        juce::roundToInt (processor.apvts.getRawParameterValue (shapeId)->load()));
    const auto seed = MicroTimingChaosProcessor::lfoSeeds[(size_t) lfoIndex];

    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (Palette::track);
    g.drawLine (area.getX(), midLine, area.getRight(), midLine, 1.0f);

    // One cycle of the shape, from left to right
    juce::Path wave;
    for (int i = 0; i <= waveformPoints; ++i)
    {
        const double cycles = (double) i / (double) waveformPoints;
        const float value = CoreDSP::Modulation::lfoValue (shape, cycles, seed);
        const juce::Point<float> point (area.getX() + area.getWidth() * (float) cycles,
                                        midLine - value * area.getHeight() * 0.5f);

        if (i == 0)
            wave.startNewSubPath (point);
        else
            wave.lineTo (point);
    }

    g.setColour (colour);
    g.strokePath (wave, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Where the LFO is now
    const float dotX = area.getX() + area.getWidth() * juce::jlimit (0.0f, 1.0f, shownPhase);
    const float dotY = midLine - juce::jlimit (-1.0f, 1.0f, shownValue) * area.getHeight() * 0.5f;
    g.fillEllipse (dotX - 3.0f, dotY - 3.0f, 6.0f, 6.0f);
}
