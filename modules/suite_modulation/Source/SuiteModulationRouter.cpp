#include "SuiteModulationRouter.h"

#include "CoreDSP.h"

#include <cmath>

namespace SuiteModulation
{
    namespace
    {
        constexpr std::array<const char*, Router::lfoCount> lfoRateIds { "lfo1Rate", "lfo2Rate" };
        constexpr std::array<const char*, Router::lfoCount> lfoShapeIds { "lfo1Shape", "lfo2Shape" };
        constexpr std::array<const char*, Router::lfoCount> lfoSyncIds { "lfo1Sync", "lfo2Sync" };
        constexpr std::array<const char*, Router::lfoCount> lfoDivisionIds { "lfo1Division", "lfo2Division" };

        constexpr std::array<const char*, Router::slotCount> modSourceIds { "mod1Source", "mod2Source", "mod3Source", "mod4Source", "mod5Source", "mod6Source" };
        constexpr std::array<const char*, Router::slotCount> modTargetIds { "mod1Target", "mod2Target", "mod3Target", "mod4Target", "mod5Target", "mod6Target" };
        constexpr std::array<const char*, Router::slotCount> modAmountIds { "mod1Amount", "mod2Amount", "mod3Amount", "mod4Amount", "mod5Amount", "mod6Amount" };

        juce::NormalisableRange<float> logRange(float start, float end)
        {
            return { start, end,
                     [](float s, float e, float v) { return s * std::pow(e / s, v); },
                     [](float s, float e, float v) { return std::log(v / s) / std::log(e / s); } };
        }

        juce::StringArray lfoShapeNames() { return { "Sine", "Triangle", "Saw", "Square", "Sample & Hold", "Random", "Noise" }; }
        juce::StringArray divisionNames() { return { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16" }; }
        juce::StringArray sourceNames() { return { "LFO 1", "LFO 2" }; }

        int clampedChoice(float raw, int count)
        {
            return std::clamp(juce::roundToInt(raw), 0, count - 1);
        }
    }

    void Router::addParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
                               const juce::StringArray& targetNames)
    {
        jassert(targetNames.size() >= 1 && targetNames[0] == "None");
        jassert(targetNames.size() <= maxTargets);

        for (int lfo = 0; lfo < lfoCount; ++lfo)
        {
            const auto number = juce::String(lfo + 1);

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { lfoRateIds[(size_t) lfo], 1 }, "LFO " + number + " Rate",
                logRange(0.02f, 20.0f), 0.25f, "Hz",
                juce::AudioProcessorParameter::genericParameter,
                [](float v, int) { return juce::String(v, 2) + " Hz"; }, nullptr));

            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID { lfoShapeIds[(size_t) lfo], 1 }, "LFO " + number + " Shape", lfoShapeNames(), 0));

            params.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID { lfoSyncIds[(size_t) lfo], 1 }, "LFO " + number + " Sync", false));

            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID { lfoDivisionIds[(size_t) lfo], 1 }, "LFO " + number + " Division", divisionNames(), 4));
        }

        for (int slot = 0; slot < slotCount; ++slot)
        {
            const auto number = juce::String(slot + 1);

            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID { modSourceIds[(size_t) slot], 1 }, "Mod " + number + " Source", sourceNames(), 0));

            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID { modTargetIds[(size_t) slot], 1 }, "Mod " + number + " Target", targetNames, 0));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { modAmountIds[(size_t) slot], 1 }, "Mod " + number + " Amount",
                juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f, "%"));
        }
    }

    Router::Router(juce::AudioProcessorValueTreeState& state, const TargetInfo* t, int count, const char* syncId)
        : apvts(state), targets(t), targetCount(count), hostSyncId(syncId)
    {
        jassert(targetCount + 1 <= maxTargets);

        for (int lfo = 0; lfo < lfoCount; ++lfo)
        {
            lfoRate[(size_t) lfo] = apvts.getRawParameterValue(lfoRateIds[(size_t) lfo]);
            lfoShape[(size_t) lfo] = apvts.getRawParameterValue(lfoShapeIds[(size_t) lfo]);
            lfoSync[(size_t) lfo] = apvts.getRawParameterValue(lfoSyncIds[(size_t) lfo]);
            lfoDivision[(size_t) lfo] = apvts.getRawParameterValue(lfoDivisionIds[(size_t) lfo]);
        }

        for (int slot = 0; slot < slotCount; ++slot)
        {
            modSource[(size_t) slot] = apvts.getRawParameterValue(modSourceIds[(size_t) slot]);
            modTarget[(size_t) slot] = apvts.getRawParameterValue(modTargetIds[(size_t) slot]);
            modAmount[(size_t) slot] = apvts.getRawParameterValue(modAmountIds[(size_t) slot]);
        }

        targetParams[0] = nullptr;
        for (int i = 1; i <= targetCount; ++i)
        {
            const char* id = targets[i - 1].parameterId;
            targetParams[(size_t) i] = apvts.getParameter(id);
            targetRaw[(size_t) i] = apvts.getRawParameterValue(id);
        }

        hostSync = hostSyncId != nullptr ? apvts.getRawParameterValue(hostSyncId) : nullptr;
    }

    int Router::getNumTargets() const noexcept
    {
        return targetCount;
    }

    void Router::prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;
        lfoCycles = {};
    }

    double Router::lfoCyclesAt(int lfo, double timeSamples, double beat, double blockStartSample) const
    {
        const auto index = (size_t) lfo;

        if (lfoSync[index]->load() > 0.5f)
        {
            return beat / CoreDSP::Modulation::divisionToBeats(clampedChoice(lfoDivision[index]->load(), 7));
        }

        // Free LFOs: the count at the start of the block, plus the time since then.
        const double secondsSinceBlockStart = (timeSamples - blockStartSample) / sampleRate;
        return lfoCycles[index] + (double) lfoRate[index]->load() * secondsSinceBlockStart;
    }

    Router::Offsets Router::computeOffsets(double timeSamples, double beat, double blockStartSample) const
    {
        Offsets offsets {};

        for (int slot = 0; slot < slotCount; ++slot)
        {
            const auto index = (size_t) slot;
            const float amount = modAmount[index]->load() / 100.0f;
            if (amount == 0.0f)
                continue;

            const int target = juce::roundToInt(modTarget[index]->load());
            if (target <= 0 || target > targetCount)
                continue;

            const int source = clampedChoice(modSource[index]->load(), lfoCount);
            const auto shape = static_cast<CoreDSP::Modulation::LfoShape>(getLfoShape(source));
            const float value = CoreDSP::Modulation::lfoValue(shape,
                                    lfoCyclesAt(source, timeSamples, beat, blockStartSample),
                                    lfoSeeds[(size_t) source]);

            offsets.value[(size_t) target] += amount * value;
        }

        return offsets;
    }

    float Router::modulate(int targetIndex, float base, const Offsets& offsets) const
    {
        if (targetIndex <= 0 || targetIndex > targetCount)
            return base;

        const float offset = offsets.value[(size_t) targetIndex];
        auto* parameter = targetParams[(size_t) targetIndex];

        // Unmodulated values are returned exactly, so an idle routing changes nothing.
        if (offset == 0.0f || parameter == nullptr)
            return base;

        const float normalised = parameter->convertTo0to1(base);
        float result = parameter->convertFrom0to1(CoreDSP::Modulation::modulatedNormalised(normalised, offset));

        if (targets[targetIndex - 1].wholeNumber)
            result = std::round(result);

        return result;
    }

    void Router::advanceBlock(int numSamples)
    {
        for (int lfo = 0; lfo < lfoCount; ++lfo)
        {
            // Synced LFOs follow the beat, so they are not counted here.
            if (lfoSync[(size_t) lfo]->load() <= 0.5f)
                lfoCycles[(size_t) lfo] += (double) lfoRate[(size_t) lfo]->load() * numSamples / sampleRate;
        }
    }

    void Router::publish(const Offsets& offsets, double blockStartSample, double beatAtBlockStart)
    {
        for (int t = 0; t < maxTargets; ++t)
            publishedOffset[(size_t) t].store(offsets.value[(size_t) t], std::memory_order_relaxed);

        for (int lfo = 0; lfo < lfoCount; ++lfo)
        {
            const double cycles = lfoCyclesAt(lfo, blockStartSample, beatAtBlockStart, blockStartSample);
            const auto shape = static_cast<CoreDSP::Modulation::LfoShape>(getLfoShape(lfo));

            publishedLfoPhase[(size_t) lfo].store((float) (cycles - std::floor(cycles)), std::memory_order_relaxed);
            publishedLfoValue[(size_t) lfo].store(CoreDSP::Modulation::lfoValue(shape, cycles, lfoSeeds[(size_t) lfo]),
                                                  std::memory_order_relaxed);
        }
    }

    float Router::getModulatedValue(int targetIndex) const
    {
        if (targetIndex <= 0 || targetIndex > targetCount)
            return 0.0f;

        const float base = targetRaw[(size_t) targetIndex]->load();
        const float offset = publishedOffset[(size_t) targetIndex].load(std::memory_order_relaxed);
        auto* parameter = targetParams[(size_t) targetIndex];

        if (offset == 0.0f || parameter == nullptr)
            return base;

        // The sequencer ignores Tempo while the host syncs, so the display shows it unmoved too.
        if (targets[targetIndex - 1].ignoredWhileHostSync && hostSync != nullptr && hostSync->load() > 0.5f)
            return base;

        const float normalised = parameter->convertTo0to1(base);
        float result = parameter->convertFrom0to1(CoreDSP::Modulation::modulatedNormalised(normalised, offset));

        if (targets[targetIndex - 1].wholeNumber)
            result = std::round(result);

        return result;
    }

    float Router::getTargetOffset(int targetIndex) const noexcept
    {
        if (targetIndex < 0 || targetIndex >= maxTargets)
            return 0.0f;

        return publishedOffset[(size_t) targetIndex].load(std::memory_order_relaxed);
    }

    float Router::getLfoValue(int lfo) const noexcept
    {
        if (lfo < 0 || lfo >= lfoCount)
            return 0.0f;

        return publishedLfoValue[(size_t) lfo].load(std::memory_order_relaxed);
    }

    float Router::getLfoPhase(int lfo) const noexcept
    {
        if (lfo < 0 || lfo >= lfoCount)
            return 0.0f;

        return publishedLfoPhase[(size_t) lfo].load(std::memory_order_relaxed);
    }

    int Router::getLfoShape(int lfo) const noexcept
    {
        return juce::roundToInt(lfoShape[(size_t) lfo]->load());
    }

    juce::RangedAudioParameter* Router::targetParameter(int targetIndex) const noexcept
    {
        if (targetIndex < 0 || targetIndex > targetCount)
            return nullptr;

        return targetParams[(size_t) targetIndex];
    }
}  // namespace SuiteModulation
