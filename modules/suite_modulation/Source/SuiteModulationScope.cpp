#include "SuiteModulationScope.h"

#include "CoreDSP.h"

namespace SuiteModulation
{
    namespace
    {
        constexpr int waveformPoints = 64;

        // Shapes that change from one cycle to the next. A single cycle of them shows nothing, so they scroll.
        bool isHistoryShape(CoreDSP::Modulation::LfoShape shape)
        {
            using Shape = CoreDSP::Modulation::LfoShape;
            return shape == Shape::sampleAndHold || shape == Shape::random || shape == Shape::noise;
        }
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

        // The history records every tick, so the trace moves along even when the value repeats.
        const auto shape = static_cast<CoreDSP::Modulation::LfoShape>(router.getLfoShape(lfoIndex));
        if (isHistoryShape(shape))
        {
            history[(size_t) historyNext] = value;
            historyNext = (historyNext + 1) % historyLength;
            shownValue = value;
            repaint();
            return;
        }

        if (phase != shownPhase || value != shownValue || offset != shownOffset || (int) shape != shownShape)
        {
            shownPhase = phase;
            shownValue = value;
            shownOffset = offset;
            shownShape = (int) shape;
            repaint();
        }
    }

    float LfoScope::curveValue(float phase) const
    {
        const auto shape = static_cast<CoreDSP::Modulation::LfoShape>(router.getLfoShape(lfoIndex));
        const auto seed = Router::lfoSeeds[(size_t) lfoIndex];

        return Router::withOffset(CoreDSP::Modulation::lfoValue(shape, (double) phase, seed), router.getLfoOffset(lfoIndex));
    }

    void LfoScope::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto area = bounds.reduced(6.0f, 5.0f);
        const auto midLine = area.getCentreY();
        const auto shape = static_cast<CoreDSP::Modulation::LfoShape>(router.getLfoShape(lfoIndex));
        const auto seed = Router::lfoSeeds[(size_t) lfoIndex];
        const bool scrolling = isHistoryShape(shape);

        g.setColour(background);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(track);
        g.drawLine(area.getX(), midLine, area.getRight(), midLine, 1.0f);

        juce::Path wave;

        if (scrolling)
        {
            // The recorded output, oldest on the left and the latest on the right
            for (int i = 0; i < historyLength; ++i)
            {
                const float value = history[(size_t) ((historyNext + i) % historyLength)];
                const juce::Point<float> point(area.getX() + area.getWidth() * (float) i / (float) (historyLength - 1),
                                               midLine - value * area.getHeight() * 0.5f);

                if (i == 0)
                    wave.startNewSubPath(point);
                else
                    wave.lineTo(point);
            }
        }
        else
        {
            // One cycle of the shape, from left to right
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
        }

        g.setColour(colour);
        g.strokePath(wave, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Where the LFO is now: at the right edge of the history, or at its phase within the cycle
        const float phase = juce::jlimit(0.0f, 1.0f, shownPhase);
        const float dotX = scrolling ? area.getRight() : area.getX() + area.getWidth() * phase;
        const float dotValue = scrolling ? juce::jlimit(-1.0f, 1.0f, shownValue) : curveValue(phase);
        const float dotY = midLine - dotValue * area.getHeight() * 0.5f;
        g.fillEllipse(dotX - 3.0f, dotY - 3.0f, 6.0f, 6.0f);
    }

}  // namespace SuiteModulation
