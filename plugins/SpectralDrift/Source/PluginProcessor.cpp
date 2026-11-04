#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float defaultDepthPercent = 19.1f;   // 0.6 rad, the engine default, as a percentage of pi
}

SpectralDriftProcessor::SpectralDriftProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    driftRate = apvts.getRawParameterValue (driftRateId);
    depth = apvts.getRawParameterValue (depthId);
    spread = apvts.getRawParameterValue (spreadId);
    stereoPhase = apvts.getRawParameterValue (stereoPhaseId);
    lfoShape = apvts.getRawParameterValue (lfoShapeId);
    shift = apvts.getRawParameterValue (shiftId);
    tilt = apvts.getRawParameterValue (tiltId);
    gate = apvts.getRawParameterValue (gateId);
    feedback = apvts.getRawParameterValue (feedbackId);
    mix = apvts.getRawParameterValue (mixId);
    freeze = apvts.getRawParameterValue (freezeId);
    feedbackDamping = apvts.getRawParameterValue (feedbackDampingId);
    driftStart = apvts.getRawParameterValue (driftStartId);
    outputGain = apvts.getRawParameterValue (outputGainId);
}

juce::AudioProcessorValueTreeState::ParameterLayout SpectralDriftProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Logarithmic sweep: equal distance per octave, so 1 Hz sits in the middle of the knob's travel.
    const juce::NormalisableRange<float> rateRange (
        0.1f, 10.0f,
        [] (float start, float end, float normalised) { return start * std::pow (end / start, normalised); },
        [] (float start, float end, float value) { return std::log (value / start) / std::log (end / start); });

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driftRateId, 1 }, "Drift Rate", rateRange, 1.0f, "Hz"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { depthId, 1 }, "Drift Depth", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaultDepthPercent, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { spreadId, 1 }, "Bin Spread", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), CoreDSP::Spectral::defaultBinSpread, "rad"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { stereoPhaseId, 1 }, "Stereo Phase", juce::NormalisableRange<float> (0.0f, 180.0f, 0.1f), 90.0f, "deg"));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { lfoShapeId, 1 }, "LFO Shape", juce::StringArray { "Sine", "Triangle", "Saw", "Square" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shiftId, 1 }, "Spectral Shift", juce::NormalisableRange<float> (-32.0f, 32.0f, 0.01f), 0.0f, "bins"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { tiltId, 1 }, "Spectral Tilt", juce::NormalisableRange<float> (-2.0f, 2.0f, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gateId, 1 }, "Spectral Gate", juce::NormalisableRange<float> (CoreDSP::Spectral::gateOffDb, 0.0f, 0.1f), CoreDSP::Spectral::gateOffDb, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { feedbackId, 1 }, "Feedback", juce::NormalisableRange<float> (0.0f, 20.0f, 0.01f), 0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { freezeId, 1 }, "Freeze", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { feedbackDampingId, 1 }, "Feedback Damping", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driftStartId, 1 }, "Drift Start", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputGainId, 1 }, "Output Gain", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, "dB"));

    return { params.begin(), params.end() };
}

void SpectralDriftProcessor::readSettings (CoreDSP::Spectral::DriftEngine::Settings& settings) const noexcept
{
    settings.driftRateHz = driftRate->load();
    settings.driftDepth = depth->load() / 100.0f * juce::MathConstants<float>::pi;
    settings.binSpread = spread->load();
    settings.lfoShape = static_cast<CoreDSP::Spectral::LfoShape> (juce::roundToInt (lfoShape->load()));
    settings.shiftBins = shift->load();
    settings.tilt = tilt->load();
    settings.gateDb = gate->load();
    // Clamped, so sessions saved with the old, larger feedback range come back within the new limit.
    settings.feedback = juce::jmin (feedback->load(), maxFeedbackPercent) / 100.0f;
    settings.feedbackDamping = feedbackDamping->load() / 100.0f;
    settings.driftStart = driftStart->load() / 100.0f;
    settings.mix = mix->load() / 100.0f;
    settings.frozen = freeze->load() > 0.5f;
}

void SpectralDriftProcessor::prepareToPlay (double sampleRate, int)
{
    for (auto& engine : engines)
        engine.prepare (sampleRate);

    setLatencySamples (CoreDSP::Spectral::DriftEngine::getLatencySamples());
}

bool SpectralDriftProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    const auto& input = layouts.getMainInputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}

void SpectralDriftProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin (buffer.getNumChannels(), (int) engines.size());
    const int numSamples = buffer.getNumSamples();

    CoreDSP::Spectral::DriftEngine::Settings settings;
    readSettings (settings);

    // The right channel's LFO is offset by the Stereo Phase control; the left channel is the reference.
    const float stereoOffset = stereoPhase->load() / 180.0f * juce::MathConstants<float>::pi;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto& engine = engines[(size_t) channel];

        settings.stereoPhase = channel == 0 ? 0.0f : stereoOffset;
        engine.setSettings (settings);

        auto* data = buffer.getWritePointer (channel);

        for (int n = 0; n < numSamples; ++n)
            data[n] = engine.processSample (data[n]);
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    // Output gain is applied after the engines, so it scales the wet and dry signal alike.
    const float gain = juce::Decibels::decibelsToGain (outputGain->load());

    for (int channel = 0; channel < numChannels; ++channel)
        buffer.applyGain (channel, 0, numSamples, gain);

    // The editor shows the left channel only.
    if (numChannels > 0)
    {
        const auto& in = engines[0].getInputMagnitudes();
        const auto& out = engines[0].getOutputMagnitudes();

        for (int k = 0; k < numBins; ++k)
        {
            snapshot.input[(size_t) k].store (in[(size_t) k], std::memory_order_relaxed);
            snapshot.output[(size_t) k].store (out[(size_t) k], std::memory_order_relaxed);
        }

        snapshot.frames.store (engines[0].getFrameCount(), std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* SpectralDriftProcessor::createEditor()
{
    return new SpectralDriftEditor (*this);
}

void SpectralDriftProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SpectralDriftProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectralDriftProcessor();
}
