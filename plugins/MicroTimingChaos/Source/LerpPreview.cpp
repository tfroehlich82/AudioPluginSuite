#include "LerpPreview.h"

#include "MicroTimingChaosLookAndFeel.h"

namespace
{
    namespace Palette = MicroTimingChaos::Palette;

    constexpr float meterRowHeight = 26.0f;
    constexpr float laneLabelWidth = 84.0f;
    constexpr float dotRadius = 3.5f;
    constexpr int pitchSpanSemitones = 36;     // the pitch lane shows root - 12 to root + 24
    constexpr int pitchLowestOffset = -12;

    void drawMeter (juce::Graphics& g, juce::Rectangle<float> row, const char* label, float amount, juce::Colour colour)
    {
        auto labelArea = row.removeFromLeft (laneLabelWidth);
        auto valueArea = row.removeFromRight (60.0f);

        g.setColour (Palette::textDim);
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText (label, labelArea, juce::Justification::centredLeft);

        g.setColour (Palette::track);
        g.fillRoundedRectangle (row.reduced (0.0f, 6.0f), 3.0f);

        g.setColour (colour);
        g.fillRoundedRectangle (row.reduced (0.0f, 6.0f).withWidth (row.getWidth() * juce::jlimit (0.0f, 1.0f, amount)), 3.0f);

        g.setColour (Palette::text);
        g.setFont (juce::Font (13.0f));
        g.drawText (juce::String (juce::roundToInt (amount * 100.0f)) + " %", valueArea, juce::Justification::centredRight);
    }
}

LerpPreview::LerpPreview()
{
    setOpaque (false);
}

void LerpPreview::refresh (std::vector<MicroTimingChaosProcessor::StepEvent> recentSteps,
                           MicroTimingChaosProcessor::DriftAmounts amounts,
                           int rootNote, float timingRangeMs)
{
    steps = std::move (recentSteps);
    drift = amounts;
    root = rootNote;
    rangeMs = juce::jmax (1.0f, timingRangeMs);
    repaint();
}

void LerpPreview::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    auto area = bounds.reduced (12.0f, 10.0f);

    // Current drift per axis
    drawMeter (g, area.removeFromTop (meterRowHeight), "TIMING", drift.timing, Palette::timing);
    drawMeter (g, area.removeFromTop (meterRowHeight), "PITCH", drift.pitch, Palette::pitch);
    drawMeter (g, area.removeFromTop (meterRowHeight), "VELOCITY", drift.velocity, Palette::velocity);

    area.removeFromTop (8.0f);
    auto caption = area.removeFromBottom (14.0f);
    area.removeFromBottom (4.0f);

    g.setColour (Palette::textDim);
    g.setFont (juce::Font (11.0f));
    g.drawText ("LAST " + juce::String ((int) steps.size()) + " STEPS, OLDEST LEFT", caption, juce::Justification::centredLeft);

    // Three lanes: timing, pitch and velocity of each recent step
    const float laneHeight = (area.getHeight() - 2.0f * 6.0f) / 3.0f;
    const juce::Colour laneColours[] = { Palette::timing, Palette::pitch, Palette::velocity };
    const char* laneLabels[] = { "ms", "note", "vel" };

    for (int lane = 0; lane < 3; ++lane)
    {
        auto laneArea = area.removeFromTop (laneHeight);
        area.removeFromTop (6.0f);

        auto plot = laneArea.withTrimmedLeft (laneLabelWidth);

        g.setColour (Palette::track.withAlpha (0.35f));
        g.fillRoundedRectangle (plot, 4.0f);

        g.setColour (Palette::textDim.withAlpha (0.6f));
        g.setFont (juce::Font (11.0f));
        g.drawText (laneLabels[lane], laneArea.withWidth (laneLabelWidth - 8.0f), juce::Justification::centredLeft);

        if (lane == 0)
        {
            g.setColour (Palette::track);
            g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
        }

        const int slots = (int) steps.size();
        if (slots == 0)
            continue;

        const float slotWidth = plot.getWidth() / (float) slots;

        for (int i = 0; i < slots; ++i)
        {
            const auto& event = steps[(size_t) i];
            if (event.velocity <= 0.0f)
                continue;

            const float x = plot.getX() + (i + 0.5f) * slotWidth;
            g.setColour (laneColours[lane]);

            if (lane == 0)
            {
                const float offset = juce::jlimit (-1.0f, 1.0f, event.offsetMs / rangeMs);
                const float y = plot.getCentreY() - offset * plot.getHeight() * 0.42f;
                g.fillEllipse (x - dotRadius, y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
            }
            else if (lane == 1)
            {
                const float semitones = (float) (event.note - root);
                const float fraction = juce::jlimit (0.0f, 1.0f, (semitones - pitchLowestOffset) / (float) pitchSpanSemitones);
                const float y = plot.getBottom() - fraction * plot.getHeight();
                g.fillEllipse (x - dotRadius, y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
            }
            else
            {
                const float height = plot.getHeight() * juce::jlimit (0.0f, 1.0f, event.velocity / 127.0f);
                g.fillRoundedRectangle (x - slotWidth * 0.3f, plot.getBottom() - height, slotWidth * 0.6f, height, 2.0f);
            }
        }
    }
}
