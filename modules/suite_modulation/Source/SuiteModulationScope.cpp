#include "SuiteModulationScope.h"

#include "CoreDSP.h"

namespace SuiteModulation
{
    namespace
    {
        constexpr int waveformPoints = 64;
    }

    LfoScope::LfoScope(Router& r, int index, juce::Colour c, juce::Colour bg, juce::Colour tr)
        : router(r), lfoIndex(index), colour(c), background(bg), track(tr)
    {
        setOpaque(false);
    }

    void LfoScope::refresh()
    {
        const float phase = router.getLfoPhase(lfoIndex);
        const float value = router.getLfoValue(lfoIndex);
        const float offset = router.getLfoOffset(lfoIndex);

        if (phase != shownPhase || value != shownValue || offset != shownOffset)
        {
            shownPhase = phase;
            shownValue = value;
            shownOffset = offset;
            repaint();
        }
    }

    void LfoScope::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto area = bounds.reduced(6.0f, 5.0f);
        const auto midLine = area.getCentreY();
        const auto shape = static_cast<CoreDSP::Modulation::LfoShape>(router.getLfoShape(lfoIndex));
        const auto seed = Router::lfoSeeds[(size_t) lfoIndex];

        g.setColour(background);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(track);
        g.drawLine(area.getX(), midLine, area.getRight(), midLine, 1.0f);

        // One cycle of the shape, from left to right
        juce::Path wave;
        for (int i = 0; i <= waveformPoints; ++i)
        {
            const double cycles = (double) i / (double) waveformPoints;
            // The curve is the output the routing receives, so it includes the Offset.
            const float value = Router::withOffset(CoreDSP::Modulation::lfoValue(shape, cycles, seed), router.getLfoOffset(lfoIndex));
            const juce::Point<float> point(area.getX() + area.getWidth() * (float) cycles,
                                           midLine - value * area.getHeight() * 0.5f);

            if (i == 0)
                wave.startNewSubPath(point);
            else
                wave.lineTo(point);
        }

        g.setColour(colour);
        g.strokePath(wave, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Where the LFO is now
        const float dotX = area.getX() + area.getWidth() * juce::jlimit(0.0f, 1.0f, shownPhase);
        const float dotY = midLine - juce::jlimit(-1.0f, 1.0f, shownValue) * area.getHeight() * 0.5f;
        g.fillEllipse(dotX - 3.0f, dotY - 3.0f, 6.0f, 6.0f);
    }

}  // namespace SuiteModulation
