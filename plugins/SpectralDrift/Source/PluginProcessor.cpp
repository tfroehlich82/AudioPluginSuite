#include "PluginProcessor.h"
#include "PluginEditor.h"

SpectralDriftProcessor::SpectralDriftProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    driftRate = apvts.getRawParameterValue (driftRateId);
    freeze = apvts.getRawParameterValue (freezeId);
    mix = apvts.getRawParameterValue (mixId);
    feedback = apvts.getRawParameterValue (feedbackId);
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

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { freezeId, 1 }, "Freeze", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { feedbackId, 1 }, "Feedback", juce::NormalisableRange<float> (0.0f, 95.0f, 0.1f), 0.0f, "%"));

    return { params.begin(), params.end() };
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
    settings.driftRateHz = driftRate->load();
    settings.feedback = feedback->load() / 100.0f;
    settings.mix = mix->load() / 100.0f;
    settings.frozen = freeze->load() > 0.5f;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto& engine = engines[(size_t) channel];
        engine.setSettings (settings);

        auto* data = buffer.getWritePointer (channel);

        for (int n = 0; n < numSamples; ++n)
            data[n] = engine.processSample (data[n]);
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

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
