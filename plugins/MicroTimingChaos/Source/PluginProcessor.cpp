#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr int midiChannel = 1;

    // Synced divisions, in beats per cycle. Index order matches the Division choice parameter.
    constexpr std::array<double, 7> divisionBeats { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25 };

    /** Shows LFO rates with two decimals, such as "0.25 Hz". */
    juce::String formatHz (float value, int) { return juce::String (value, 2) + " Hz"; }

    /** A logarithmic range from start to end, so the knob moves in equal steps per octave. */
    juce::NormalisableRange<float> logRange (float start, float end)
    {
        return juce::NormalisableRange<float> (
            start, end,
            [] (float s, float e, float n) { return s * std::pow (e / s, n); },
            [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); });
    }
}

MicroTimingChaosProcessor::MicroTimingChaosProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    rate = apvts.getRawParameterValue (rateId);
    length = apvts.getRawParameterValue (lengthId);
    density = apvts.getRawParameterValue (densityId);
    seed = apvts.getRawParameterValue (seedId);
    gate = apvts.getRawParameterValue (gateId);
    swing = apvts.getRawParameterValue (swingId);
    baseVelocity = apvts.getRawParameterValue (baseVelocityId);
    root = apvts.getRawParameterValue (rootId);
    scale = apvts.getRawParameterValue (scaleId);
    octaves = apvts.getRawParameterValue (octavesId);
    chaos = apvts.getRawParameterValue (chaosId);
    timingRange = apvts.getRawParameterValue (timingRangeId);
    velocityChaos = apvts.getRawParameterValue (velocityChaosId);
    lerp = apvts.getRawParameterValue (lerpId);
    lerpXMult = apvts.getRawParameterValue (lerpXMultId);
    lerpYMult = apvts.getRawParameterValue (lerpYMultId);
    lerpZMult = apvts.getRawParameterValue (lerpZMultId);
    bpm = apvts.getRawParameterValue (bpmId);
    sync = apvts.getRawParameterValue (syncId);
    speed = apvts.getRawParameterValue (speedId);

    const char* const lfoRateIds[lfoCount] = { lfo1RateId, lfo2RateId };
    const char* const lfoShapeIds[lfoCount] = { lfo1ShapeId, lfo2ShapeId };
    const char* const lfoSyncIds[lfoCount] = { lfo1SyncId, lfo2SyncId };
    const char* const lfoDivisionIds[lfoCount] = { lfo1DivisionId, lfo2DivisionId };

    for (int i = 0; i < lfoCount; ++i)
    {
        lfoRate[(size_t) i] = apvts.getRawParameterValue (lfoRateIds[i]);
        lfoShape[(size_t) i] = apvts.getRawParameterValue (lfoShapeIds[i]);
        lfoSync[(size_t) i] = apvts.getRawParameterValue (lfoSyncIds[i]);
        lfoDivision[(size_t) i] = apvts.getRawParameterValue (lfoDivisionIds[i]);
    }

    for (int i = 0; i < slotCount; ++i)
    {
        modSource[(size_t) i] = apvts.getRawParameterValue (modSourceIds[i]);
        modTarget[(size_t) i] = apvts.getRawParameterValue (modTargetIds[i]);
        modAmount[(size_t) i] = apvts.getRawParameterValue (modAmountIds[i]);
    }

    // The parameter behind each routing target, used to convert offsets in its own range.
    targetParams[(size_t) ModTarget::Chaos] = apvts.getParameter (chaosId);
    targetParams[(size_t) ModTarget::TimingRange] = apvts.getParameter (timingRangeId);
    targetParams[(size_t) ModTarget::VelocityChaos] = apvts.getParameter (velocityChaosId);
    targetParams[(size_t) ModTarget::Lerp] = apvts.getParameter (lerpId);
    targetParams[(size_t) ModTarget::LerpX] = apvts.getParameter (lerpXMultId);
    targetParams[(size_t) ModTarget::LerpY] = apvts.getParameter (lerpYMultId);
    targetParams[(size_t) ModTarget::LerpZ] = apvts.getParameter (lerpZMultId);
    targetParams[(size_t) ModTarget::Density] = apvts.getParameter (densityId);
    targetParams[(size_t) ModTarget::Gate] = apvts.getParameter (gateId);
    targetParams[(size_t) ModTarget::Swing] = apvts.getParameter (swingId);
    targetParams[(size_t) ModTarget::Velocity] = apvts.getParameter (baseVelocityId);
    targetParams[(size_t) ModTarget::Speed] = apvts.getParameter (speedId);
    targetParams[(size_t) ModTarget::Tempo] = apvts.getParameter (bpmId);
    targetParams[(size_t) ModTarget::Seed] = apvts.getParameter (seedId);

    for (int i = 0; i < curvePointCount; ++i)
    {
        const auto point = naturalCurvePoint (i);
        curveX[(size_t) i].store (point.x);
        curveY[(size_t) i].store (point.y);
    }

    loadLerpCurveFromState();
}

juce::StringArray MicroTimingChaosProcessor::modTargetNames()
{
    // Must follow the ModTarget enum order exactly.
    return { "None", "Chaos", "Timing Range", "Velocity Chaos", "Lerp", "Lerp X Mult", "Lerp Y Mult",
             "Lerp Z Mult", "Density", "Gate", "Swing", "Velocity", "Speed", "Tempo", "Seed" };
}

//==============================================================================
float MicroTimingChaosProcessor::naturalCurveValue (float t) noexcept
{
    return std::log10 (1.0f + 9.0f * std::clamp (t, 0.0f, 1.0f));
}

juce::Point<float> MicroTimingChaosProcessor::naturalCurvePoint (int index) noexcept
{
    const float x = (float) juce::jlimit (0, curvePointCount - 1, index) / (float) (curvePointCount - 1);
    return { x, naturalCurveValue (x) };
}

std::array<juce::Point<float>, MicroTimingChaosProcessor::curvePointCount> MicroTimingChaosProcessor::getLerpCurve() const
{
    std::array<juce::Point<float>, curvePointCount> points;

    for (int i = 0; i < curvePointCount; ++i)
        points[(size_t) i] = { curveX[(size_t) i].load (std::memory_order_relaxed),
                               curveY[(size_t) i].load (std::memory_order_relaxed) };

    return points;
}

void MicroTimingChaosProcessor::setLerpCurvePoint (int index, juce::Point<float> point)
{
    if (index < 0 || index >= curvePointCount)
        return;

    constexpr float minGap = 0.02f;
    const auto i = (size_t) index;
    float x = juce::jlimit (0.0f, 1.0f, point.x);

    if (index == 0)
        x = 0.0f;
    else if (index == curvePointCount - 1)
        x = 1.0f;
    else
        x = juce::jlimit (curveX[i - 1].load() + minGap, curveX[i + 1].load() - minGap, x);

    curveX[i].store (x);
    curveY[i].store (juce::jlimit (0.0f, 1.0f, point.y));

    storeLerpCurveToState();
}

void MicroTimingChaosProcessor::resetLerpCurve()
{
    for (int i = 0; i < curvePointCount; ++i)
    {
        const auto point = naturalCurvePoint (i);
        curveX[(size_t) i].store (point.x);
        curveY[(size_t) i].store (point.y);
    }

    storeLerpCurveToState();
}

float MicroTimingChaosProcessor::evaluateLerpCurve (float chaos) const noexcept
{
    const float t = std::clamp (chaos, 0.0f, 1.0f);

    for (int i = 0; i < curvePointCount - 1; ++i)
    {
        const auto segment = (size_t) i;
        const float x0 = curveX[segment].load (std::memory_order_relaxed);
        const float x1 = curveX[segment + 1].load (std::memory_order_relaxed);

        if (t <= x1 || i == curvePointCount - 2)
        {
            const float span = x1 - x0;
            const float alpha = span > 1.0e-6f ? std::clamp ((t - x0) / span, 0.0f, 1.0f) : 1.0f;
            const float y0 = curveY[segment].load (std::memory_order_relaxed);
            const float y1 = curveY[segment + 1].load (std::memory_order_relaxed);

            return y0 + (y1 - y0) * alpha;
        }
    }

    return curveY[curvePointCount - 1].load (std::memory_order_relaxed);
}

void MicroTimingChaosProcessor::loadLerpCurveFromState()
{
    const auto encoded = apvts.state.getProperty ("lerpCurve").toString();
    const auto pairs = juce::StringArray::fromTokens (encoded, ";", "");

    if (pairs.size() != curvePointCount)
        return;

    for (int i = 0; i < curvePointCount; ++i)
    {
        const auto values = juce::StringArray::fromTokens (pairs[i], ",", "");
        if (values.size() == 2)
        {
            curveX[(size_t) i].store (juce::jlimit (0.0f, 1.0f, values[0].getFloatValue()));
            curveY[(size_t) i].store (juce::jlimit (0.0f, 1.0f, values[1].getFloatValue()));
        }
    }
}

void MicroTimingChaosProcessor::storeLerpCurveToState()
{
    juce::StringArray pairs;

    for (int i = 0; i < curvePointCount; ++i)
        pairs.add (juce::String (curveX[(size_t) i].load(), 4) + "," + juce::String (curveY[(size_t) i].load(), 4));

    apvts.state.setProperty ("lerpCurve", pairs.joinIntoString (";"), nullptr);
}

MicroTimingChaosProcessor::DriftAmounts MicroTimingChaosProcessor::getDriftAmounts() const noexcept
{
    return { driftTiming.load (std::memory_order_relaxed),
             driftPitch.load (std::memory_order_relaxed),
             driftVelocity.load (std::memory_order_relaxed) };
}

double MicroTimingChaosProcessor::getDisplayBpm() const
{
    const auto* playHead = getPlayHead();
    const auto position = playHead != nullptr ? playHead->getPosition()
                                              : juce::Optional<juce::AudioPlayHead::PositionInfo>();
    return currentBpm (position, bpm->load());
}

juce::AudioProcessorValueTreeState::ParameterLayout MicroTimingChaosProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Pattern
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { rateId, 1 }, "Rate", StringArray { "1/4", "1/8", "1/16", "1/32" }, 2));
    params.push_back (std::make_unique<AudioParameterInt> (ParameterID { lengthId, 1 }, "Pattern Length", 2, 16, 8));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { densityId, 1 }, "Density", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 70.0f, "%"));
    params.push_back (std::make_unique<AudioParameterInt> (ParameterID { seedId, 1 }, "Seed", 0, 999, 42));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { gateId, 1 }, "Gate", NormalisableRange<float> (10.0f, 100.0f, 0.1f), 60.0f, "%"));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { swingId, 1 }, "Swing", NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, "%"));
    params.push_back (std::make_unique<AudioParameterInt> (ParameterID { baseVelocityId, 1 }, "Velocity", 30, 127, 100));

    // Melody
    params.push_back (std::make_unique<AudioParameterInt> (
        ParameterID { rootId, 1 }, "Root Note", 24, 72, 48,
        AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int value, int) { return MidiMessage::getMidiNoteName (value, true, true, 3) + " (" + String (value) + ")"; })));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { scaleId, 1 }, "Scale",
        StringArray { "Chromatic", "Major", "Minor", "Minor Pentatonic", "Dorian", "Whole Tone" }, 2));
    params.push_back (std::make_unique<AudioParameterInt> (ParameterID { octavesId, 1 }, "Octaves", 1, 3, 2));

    // Chaos
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { chaosId, 1 }, "Chaos", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { timingRangeId, 1 }, "Timing Range", NormalisableRange<float> (0.0f, (float) maxTimingRangeMs, 0.01f), 20.0f, "ms"));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { velocityChaosId, 1 }, "Velocity Chaos", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, "%"));

    // Lerp Curve
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { lerpId, 1 }, "Lerp", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { lerpXMultId, 1 }, "Lerp X Mult", NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { lerpYMultId, 1 }, "Lerp Y Mult", NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { lerpZMultId, 1 }, "Lerp Z Mult", NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f));

    // Transport
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { bpmId, 1 }, "Tempo", NormalisableRange<float> (40.0f, 240.0f, 0.01f), 120.0f, "BPM"));
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { syncId, 1 }, "Host Sync", false));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { speedId, 1 }, "Speed", NormalisableRange<float> (0.5f, 2.0f, 0.01f), 1.0f));

    // LFO 1 and LFO 2
    const StringArray lfoShapes { "Sine", "Triangle", "Saw", "Square", "Sample & Hold", "Random", "Noise" };
    const StringArray divisions { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16" };

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { lfo1RateId, 1 }, "LFO 1 Rate", logRange (0.02f, 20.0f), 0.25f, "Hz",
        AudioProcessorParameter::genericParameter, formatHz, nullptr));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { lfo1ShapeId, 1 }, "LFO 1 Shape", lfoShapes, 0));
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { lfo1SyncId, 1 }, "LFO 1 Sync", false));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { lfo1DivisionId, 1 }, "LFO 1 Division", divisions, 4));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { lfo2RateId, 1 }, "LFO 2 Rate", logRange (0.02f, 20.0f), 0.25f, "Hz",
        AudioProcessorParameter::genericParameter, formatHz, nullptr));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { lfo2ShapeId, 1 }, "LFO 2 Shape", lfoShapes, 0));
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { lfo2SyncId, 1 }, "LFO 2 Sync", false));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { lfo2DivisionId, 1 }, "LFO 2 Division", divisions, 4));

    // Modulation routing
    for (int slot = 0; slot < slotCount; ++slot)
    {
        const auto number = juce::String (slot + 1);

        params.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { modSourceIds[slot], 1 }, "Mod " + number + " Source", StringArray { "LFO 1", "LFO 2" }, 0));
        params.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { modTargetIds[slot], 1 }, "Mod " + number + " Target", modTargetNames(), 0));
        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { modAmountIds[slot], 1 }, "Mod " + number + " Amount",
            NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, "%"));
    }

    return { params.begin(), params.end() };
}

void MicroTimingChaosProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    streamSample = 0;
    beatPosition = 0.0;
    lfoCycles = {};
    nextStep = 0;
    resyncNeeded = true;
    wasRunning = false;

    pending.clear();
    pendingOffs.clear();
    lastNoteOn.clear();
    sounding.clear();

    attractor.reset();
    attractorBeat = 0.0;

    latencySamples = (std::int64_t) std::llround (maxTimingRangeMs * sampleRate / 1000.0);
    setLatencySamples ((int) latencySamples);
}

bool MicroTimingChaosProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    const auto& input = layouts.getMainInputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}

juce::Optional<double> MicroTimingChaosProcessor::getHostBpm () const
{
    const auto* playHead = getPlayHead();
    if (playHead == nullptr)
        return juce::Optional<double>{};

    const auto position = playHead->getPosition();
    if (! position.hasValue())
        return juce::Optional<double>{};

    const auto hostBpm = position->getBpm();
    if (hostBpm.hasValue() && *hostBpm > 1.0)
        return *hostBpm;

    return juce::Optional<double>{};
}

double MicroTimingChaosProcessor::currentBpm (const juce::Optional<juce::AudioPlayHead::PositionInfo>& position, float freeTempo) const
{
    if (sync->load() > 0.5f && position.hasValue())
    {
        const auto hostBpm = position->getBpm();
        if (hostBpm.hasValue() && *hostBpm > 1.0)
            return *hostBpm;
    }

    return (double) freeTempo;
}

double MicroTimingChaosProcessor::lfoCyclesAt (int lfoIndex, double timeSamples, double beat) const
{
    const auto index = (size_t) lfoIndex;

    if (lfoSync[index]->load() > 0.5f)
    {
        const int division = std::clamp (juce::roundToInt (lfoDivision[index]->load()), 0, (int) divisionBeats.size() - 1);
        return beat / divisionBeats[(size_t) division];
    }

    // Free LFOs: the count at the start of this block, plus the time since then.
    const double secondsSinceBlockStart = (timeSamples - (double) streamSample) / sampleRate;
    return lfoCycles[index] + (double) lfoRate[index]->load() * secondsSinceBlockStart;
}

MicroTimingChaosProcessor::TargetOffsets MicroTimingChaosProcessor::computeOffsets (double timeSamples, double beat) const
{
    TargetOffsets offsets {};

    for (int slot = 0; slot < slotCount; ++slot)
    {
        const auto index = (size_t) slot;
        const float amount = modAmount[index]->load() / 100.0f;
        if (amount == 0.0f)
            continue;

        const int target = juce::roundToInt (modTarget[index]->load());
        if (target <= 0 || target >= (int) ModTarget::count)
            continue;

        const int source = std::clamp (juce::roundToInt (modSource[index]->load()), 0, lfoCount - 1);
        const auto shape = static_cast<CoreDSP::Modulation::LfoShape> (juce::roundToInt (lfoShape[(size_t) source]->load()));
        const float value = CoreDSP::Modulation::lfoValue (shape, lfoCyclesAt (source, timeSamples, beat), lfoSeeds[(size_t) source]);

        offsets[(size_t) target] += amount * value;
    }

    return offsets;
}

float MicroTimingChaosProcessor::modulatedValue (ModTarget target, float base, const TargetOffsets& offsets) const
{
    const auto index = (size_t) target;
    auto* parameter = targetParams[index];

    // Unmodulated values are returned exactly, so an idle routing changes nothing.
    if (offsets[index] == 0.0f || parameter == nullptr)
        return base;

    const float normalised = parameter->convertTo0to1 (base);
    return parameter->convertFrom0to1 (CoreDSP::Modulation::modulatedNormalised (normalised, offsets[index]));
}

float MicroTimingChaosProcessor::getLfoValue (int lfoIndex) const noexcept
{
    return lfoPublishedValue[(size_t) lfoIndex].load (std::memory_order_relaxed);
}

float MicroTimingChaosProcessor::getLfoPhase (int lfoIndex) const noexcept
{
    return lfoPublishedPhase[(size_t) lfoIndex].load (std::memory_order_relaxed);
}

float MicroTimingChaosProcessor::getTargetOffset (ModTarget target) const noexcept
{
    return targetOffsetPublished[(size_t) target].load (std::memory_order_relaxed);
}

float MicroTimingChaosProcessor::getModulatedValue (ModTarget target, const char* parameterId) const
{
    const float base = apvts.getRawParameterValue (parameterId)->load();
    const auto* parameter = targetParams[(size_t) target];

    if (target == ModTarget::None || parameter == nullptr || getTargetOffset (target) == 0.0f)
        return base;

    if (target == ModTarget::Tempo && sync->load() > 0.5f)
        return base;

    const float normalised = parameter->convertTo0to1 (base);
    return parameter->convertFrom0to1 (CoreDSP::Modulation::modulatedNormalised (normalised, getTargetOffset (target)));
}

void MicroTimingChaosProcessor::advanceAttractorTo (double beat, double speedValue)
{
    const double delta = beat - attractorBeat;

    if (delta > 0.0)
    {
        double time = delta * speedValue;

        while (time > 0.0)
        {
            const double chunk = std::min (time, CoreDSP::Chaos::LorenzAttractor::maxAdvance);
            attractor.advance (chunk);
            time -= chunk;
        }
    }

    attractorBeat = beat;
}

void MicroTimingChaosProcessor::publishTrailPoint() noexcept
{
    const int slot = trail.head.load (std::memory_order_relaxed);

    trail.x[(size_t) slot].store (attractor.normalisedX(), std::memory_order_relaxed);
    trail.y[(size_t) slot].store (attractor.normalisedY(), std::memory_order_relaxed);
    trail.head.store ((slot + 1) % trailLength, std::memory_order_release);
}

void MicroTimingChaosProcessor::publishStepEvent (const StepEvent& event) noexcept
{
    const int slot = logHead.load (std::memory_order_relaxed);
    auto& entry = log[(size_t) slot];

    entry.patternStep.store (event.patternStep, std::memory_order_relaxed);
    entry.offsetMs.store (event.offsetMs, std::memory_order_relaxed);
    entry.velocity.store (event.velocity, std::memory_order_relaxed);
    entry.note.store (event.note, std::memory_order_relaxed);

    logHead.store ((slot + 1) % logLength, std::memory_order_release);
}

std::vector<juce::Point<float>> MicroTimingChaosProcessor::getTrail() const
{
    std::vector<juce::Point<float>> points;
    points.reserve (trailLength);

    const int head = trail.head.load (std::memory_order_acquire);

    for (int i = 0; i < trailLength; ++i)
    {
        const auto slot = (size_t) ((head + i) % trailLength);
        points.emplace_back (trail.x[slot].load (std::memory_order_relaxed),
                             trail.y[slot].load (std::memory_order_relaxed));
    }

    return points;
}

std::vector<MicroTimingChaosProcessor::StepEvent> MicroTimingChaosProcessor::getRecentSteps() const
{
    std::vector<StepEvent> events;
    events.reserve (logLength);

    const int head = logHead.load (std::memory_order_acquire);

    for (int i = 0; i < logLength; ++i)
    {
        const auto& entry = log[(size_t) ((head + i) % logLength)];
        events.push_back ({ entry.patternStep.load (std::memory_order_relaxed),
                            entry.offsetMs.load (std::memory_order_relaxed),
                            entry.velocity.load (std::memory_order_relaxed),
                            entry.note.load (std::memory_order_relaxed) });
    }

    return events;
}

std::int64_t MicroTimingChaosProcessor::queueNoteOn (int note, int velocity, std::int64_t target)
{
    const auto found = pendingOffs.find (note);
    const auto earlier = lastNoteOn.find (note);

    if (found != pendingOffs.end() && found->second->first >= target && earlier != lastNoteOn.end())
    {
        target = std::max (target, earlier->second + 1);
        const std::int64_t cutAt = std::max (target - 1, earlier->second);

        pending.erase (found->second);
        pendingOffs.erase (found);
        pendingOffs[note] = pending.emplace (cutAt, juce::MidiMessage::noteOff (midiChannel, note));
    }

    lastNoteOn[note] = target;
    pending.emplace (target, juce::MidiMessage::noteOn (midiChannel, note, (juce::uint8) velocity));
    return target;
}

void MicroTimingChaosProcessor::queueNoteOff (int note, std::int64_t target)
{
    pendingOffs[note] = pending.emplace (target, juce::MidiMessage::noteOff (midiChannel, note));
}

void MicroTimingChaosProcessor::stopAllNotes()
{
    pending.clear();
    pendingOffs.clear();
    lastNoteOn.clear();

    for (const int note : sounding)
        pending.emplace (streamSample, juce::MidiMessage::noteOff (midiChannel, note));
}

void MicroTimingChaosProcessor::scheduleStep (std::int64_t stepIndex, double stepBeat, double blockStartBeat,
                                               double samplesPerBeat)
{
    using namespace CoreDSP::Sequencer;

    // Order of work for one step:
    //   1. LFO offsets for this step's time and beat.
    //   2. Every control, with its offset applied (unmodulated values are returned unchanged).
    //   3. The lerp curve, evaluated on the modulated Chaos.
    //   4. The step itself.
    const double stepSample = (double) streamSample + (stepBeat - blockStartBeat) * samplesPerBeat;
    const auto offsets = computeOffsets (stepSample, stepBeat);

    const int lengthValue = patternLength.load (std::memory_order_relaxed);
    const int patternIndex = (int) (((stepIndex % lengthValue) + lengthValue) % lengthValue);
    const auto seedValue = (std::uint32_t) juce::roundToInt (modulatedValue (ModTarget::Seed, seed->load(), offsets));
    const float densityValue = modulatedValue (ModTarget::Density, density->load(), offsets) / 100.0f;
    const float effectiveDensity = densityValue > 0.0f ? log1p (densityValue * 0.99f) / log1p (0.99f) : 0.0f;
    const double stepLength = stepBeats (juce::roundToInt (rate->load()));

    const float speedValue = modulatedValue (ModTarget::Speed, speed->load(), offsets);
    advanceAttractorTo (stepBeat, speedValue);
    publishTrailPoint();

    const float chaosValue = modulatedValue (ModTarget::Chaos, chaos->load(), offsets) / 100.0f;
    const float rangeMs = modulatedValue (ModTarget::TimingRange, timingRange->load(), offsets);

    // Hybrid lerp model: each axis drifts by curve(chaos) * its multiplier, scaled by the Lerp amount.
    const float curve = evaluateLerpCurve (chaosValue);
    const float lerpAmount = std::clamp (modulatedValue (ModTarget::Lerp, lerp->load(), offsets) / 100.0f, 0.0f, 1.0f);
    const float xMult = modulatedValue (ModTarget::LerpX, lerpXMult->load(), offsets);
    const float yMult = modulatedValue (ModTarget::LerpY, lerpYMult->load(), offsets);
    const float zMult = modulatedValue (ModTarget::LerpZ, lerpZMult->load(), offsets);
    const float timingAmount = std::clamp (curve * xMult, 0.0f, 1.0f) * lerpAmount;
    const float pitchAmount = std::clamp (curve * yMult, 0.0f, 1.0f) * lerpAmount;
    const float velocityAmount = std::clamp (curve * zMult, 0.0f, 1.0f) * lerpAmount;

    driftTiming.store (timingAmount, std::memory_order_relaxed);
    driftPitch.store (pitchAmount, std::memory_order_relaxed);
    driftVelocity.store (velocityAmount, std::memory_order_relaxed);

    const float x = attractor.normalisedX();
    const float y = attractor.normalisedY();
    const float z = attractor.normalisedZ();

    if (! stepFires (seedValue, patternIndex, effectiveDensity))
        return;

    const int scaleValue = juce::roundToInt (scale->load());
    const int octavesValue = juce::roundToInt (octaves->load());
    const int rootValue = juce::roundToInt (root->load());
    const int count = degreeCount (scaleValue, octavesValue);

    const int hashDegree = degreeFromUnit (2.0f * hashUnit (seedValue, 1000u + (std::uint32_t) patternIndex) - 1.0f,
                                           scaleValue, octavesValue);
    const int lorenzDegree = degreeFromUnit (y, scaleValue, octavesValue);
    const int degree = blendDegree (hashDegree, lorenzDegree, pitchAmount, count);
    const int note = notePitch (rootValue, scaleValue, degree);

    const int velocity = velocityFor (juce::roundToInt (modulatedValue (ModTarget::Velocity, baseVelocity->load(), offsets)),
                                      modulatedValue (ModTarget::VelocityChaos, velocityChaos->load(), offsets) / 100.0f * velocityAmount, z);

    const float timingOffset = timingOffsetMs (timingAmount, rangeMs, x);

    const double offsetSamples = (std::int64_t) std::llround (timingOffset * sampleRate / 1000.0);
    const double swingSamples = swingBeats (stepIndex, modulatedValue (ModTarget::Swing, swing->load(), offsets) / 100.0f, stepLength) * samplesPerBeat;

    const double gridSample = (double) streamSample + (stepBeat - blockStartBeat) * samplesPerBeat;
    const auto onTarget = std::max<std::int64_t> (streamSample,
                                                  (std::int64_t) std::llround (gridSample + swingSamples)
                                                      + latencySamples + offsetSamples);

    const double gateSamples = gateBeats (stepLength, modulatedValue (ModTarget::Gate, gate->load(), offsets) / 100.0f, pitchAmount, y) * samplesPerBeat;
    const auto actualOn = queueNoteOn (note, velocity, onTarget);
    queueNoteOff (note, actualOn + std::max<std::int64_t> (1, (std::int64_t) std::llround (gateSamples)));

    publishStepEvent ({ patternIndex, timingOffset, (float) velocity, note });
}

void MicroTimingChaosProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    const auto* playHead = getPlayHead();
    const auto position = playHead != nullptr ? playHead->getPosition()
                                              : juce::Optional<juce::AudioPlayHead::PositionInfo>();
    const bool followHost = sync->load() > 0.5f && position.hasValue();
    const bool running = ! followHost || position->getIsPlaying();

    const int lengthValue = juce::roundToInt (length->load());
    patternLength.store (lengthValue, std::memory_order_relaxed);

    const int rateIndex = juce::roundToInt (rate->load());
    const double stepLength = CoreDSP::Sequencer::stepBeats (rateIndex);

    // Follow the host position first, so the block-start LFO and tempo values use the right beat.
    if (running && followHost)
    {
        const auto ppq = position->getPpqPosition();

        if (ppq.hasValue() && (! wasRunning || std::abs (*ppq - beatPosition) > 1.0e-3))
        {
            beatPosition = *ppq;
            resyncNeeded = true;
        }
    }

    // Block-start values: the LFO positions shown in the UI, and the free tempo (Tempo can only move
    // the tempo while Host Sync is off, which currentBpm() enforces).
    const auto blockOffsets = computeOffsets ((double) streamSample, beatPosition);

    // Published once per block, so the controls' displays follow the LFOs in real time.
    for (size_t target = 0; target < blockOffsets.size(); ++target)
        targetOffsetPublished[target].store (blockOffsets[target], std::memory_order_relaxed);

    for (int i = 0; i < lfoCount; ++i)
    {
        const auto index = (size_t) i;
        const double cycles = lfoCyclesAt (i, (double) streamSample, beatPosition);
        const auto shape = static_cast<CoreDSP::Modulation::LfoShape> (juce::roundToInt (lfoShape[index]->load()));

        lfoPublishedPhase[index].store ((float) (cycles - std::floor (cycles)), std::memory_order_relaxed);
        lfoPublishedValue[index].store (CoreDSP::Modulation::lfoValue (shape, cycles, lfoSeeds[index]), std::memory_order_relaxed);
    }

    const float freeTempo = modulatedValue (ModTarget::Tempo, bpm->load(), blockOffsets);
    const double samplesPerBeat = 60.0 / currentBpm (position, freeTempo) * sampleRate;

    if (! running)
    {
        if (wasRunning)
            stopAllNotes();

        wasRunning = false;
        playheadStep.store (-1, std::memory_order_relaxed);
    }
    else
    {
        wasRunning = true;

        const double blockStart = beatPosition;
        const double blockEnd = blockStart + numSamples / samplesPerBeat;

        const double expected = (double) nextStep * stepLength;
        if (resyncNeeded || expected > blockEnd + stepLength || expected < blockStart - stepLength)
        {
            nextStep = (std::int64_t) std::ceil (blockStart / stepLength - 1.0e-9);
            resyncNeeded = false;
        }

        while ((double) nextStep * stepLength < blockEnd)
        {
            const double stepBeat = (double) nextStep * stepLength;

            if (stepBeat >= blockStart - 1.0e-9)
                scheduleStep (nextStep, stepBeat, blockStart, samplesPerBeat);

            ++nextStep;
        }

        const auto playStep = (std::int64_t) std::floor (blockStart / stepLength + 1.0e-9);
        playheadStep.store ((int) (((playStep % lengthValue) + lengthValue) % lengthValue), std::memory_order_relaxed);

        beatPosition = blockEnd;
    }

    for (const auto metadata : midi)
        pending.emplace (streamSample + metadata.samplePosition + latencySamples, metadata.getMessage());

    midi.clear();

    // Free LFOs keep running, whether or not the transport is moving (their phase never resets).
    for (int i = 0; i < lfoCount; ++i)
        if (lfoSync[(size_t) i]->load() <= 0.5f)
            lfoCycles[(size_t) i] += (double) lfoRate[(size_t) i]->load() * numSamples / sampleRate;

    const std::int64_t blockEndSample = streamSample + numSamples;

    for (auto it = pending.begin(); it != pending.end() && it->first < blockEndSample;)
    {
        const auto& message = it->second;
        const auto offset = (int) std::max<std::int64_t> (0, it->first - streamSample);

        if (message.isNoteOn())
        {
            sounding.insert (message.getNoteNumber());
        }
        else if (message.isNoteOff())
        {
            sounding.erase (message.getNoteNumber());

            const auto found = pendingOffs.find (message.getNoteNumber());
            if (found != pendingOffs.end() && found->second == it)
                pendingOffs.erase (found);
        }

        midi.addEvent (message, offset);
        it = pending.erase (it);
    }

    streamSample = blockEndSample;
}

juce::AudioProcessorEditor* MicroTimingChaosProcessor::createEditor()
{
    return new MicroTimingChaosEditor (*this);
}

void MicroTimingChaosProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MicroTimingChaosProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        loadLerpCurveFromState();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MicroTimingChaosProcessor();
}
