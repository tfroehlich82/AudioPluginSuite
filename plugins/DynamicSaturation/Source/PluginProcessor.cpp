#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr double gainSmoothingSeconds = 0.05;
}

DynamicSaturationProcessor::DynamicSaturationProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    drive = apvts.getRawParameterValue (driveId);
    sensitivity = apvts.getRawParameterValue (sensitivityId);
    mode = apvts.getRawParameterValue (modeId);
    mix = apvts.getRawParameterValue (mixId);
    outputLevel = apvts.getRawParameterValue (outputLevelId);
}

juce::AudioProcessorValueTreeState::ParameterLayout DynamicSaturationProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveId, 1 }, "Drive", juce::NormalisableRange<float> (0.0f, 24.0f, 0.01f), 6.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { sensitivityId, 1 }, "Envelope Sensitivity", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { modeId, 1 }, "Saturation Mode", juce::StringArray { "Tube", "Tape", "Diode" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputLevelId, 1 }, "Output Level", juce::NormalisableRange<float> (-12.0f, 6.0f, 0.01f), 0.0f, "dB"));

    return { params.begin(), params.end() };
}

void DynamicSaturationProcessor::prepareToPlay (double sampleRate, int)
{
    envelope.prepare (sampleRate);
    envelope.reset();

    driveGain.reset (sampleRate, gainSmoothingSeconds);
    mixAmount.reset (sampleRate, gainSmoothingSeconds);
    outputGain.reset (sampleRate, gainSmoothingSeconds);

    driveGain.setCurrentAndTargetValue (CoreDSP::decibelsToGain (drive->load()));
    mixAmount.setCurrentAndTargetValue (mix->load() / 100.0f);
    outputGain.setCurrentAndTargetValue (CoreDSP::decibelsToGain (outputLevel->load()));
}

bool DynamicSaturationProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    const auto& input = layouts.getMainInputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}

void DynamicSaturationProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    driveGain.setTargetValue (CoreDSP::decibelsToGain (drive->load()));
    mixAmount.setTargetValue (mix->load() / 100.0f);
    outputGain.setTargetValue (CoreDSP::decibelsToGain (outputLevel->load()));

    const float sens = sensitivity->load();
    const auto shaping = static_cast<CoreDSP::Saturation::Mode> (juce::roundToInt (mode->load()));

    // The envelope is linked across channels, so both channels get the same drive and the stereo image is kept.
    for (int n = 0; n < numSamples; ++n)
    {
        float linked = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
            linked = juce::jmax (linked, std::abs (buffer.getSample (channel, n)));

        const float env = envelope.process (linked);
        const float preGain = CoreDSP::Saturation::dynamicDrive (driveGain.getNextValue(), sens, env);
        const float mixValue = mixAmount.getNextValue();
        const float outGain = outputGain.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float dry = buffer.getSample (channel, n);
            const float wet = CoreDSP::Saturation::shape (dry * preGain, shaping);
            buffer.setSample (channel, n, (dry + mixValue * (wet - dry)) * outGain);
        }
    }

    envelopeLevel.store (envelope.getLevel(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* DynamicSaturationProcessor::createEditor()
{
    return new DynamicSaturationEditor (*this);
}

void DynamicSaturationProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DynamicSaturationProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DynamicSaturationProcessor();
}
