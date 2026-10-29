#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr double gainSmoothingSeconds = 0.05;
    constexpr float detectorDefaultHz = 20.0f;
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
    attack = apvts.getRawParameterValue (attackId);
    release = apvts.getRawParameterValue (releaseId);
    detectHpf = apvts.getRawParameterValue (detectHpfId);
    stereoLink = apvts.getRawParameterValue (stereoLinkId);
    bias = apvts.getRawParameterValue (biasId);
    fold = apvts.getRawParameterValue (foldId);
    tone = apvts.getRawParameterValue (toneId);
    curve = apvts.getRawParameterValue (curveId);
}

juce::AudioProcessorValueTreeState::ParameterLayout DynamicSaturationProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Core controls
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

    // Extended controls
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { attackId, 1 }, "Attack", juce::NormalisableRange<float> (0.1f, 50.0f, 0.01f, 0.35f), 5.0f, "ms"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { releaseId, 1 }, "Release", juce::NormalisableRange<float> (10.0f, 1000.0f, 0.1f, 0.4f), 150.0f, "ms"));

    // Log sweep, so a 100 Hz step is as big as a 400 Hz one.
    const juce::NormalisableRange<float> hpfRange (
        20.0f, 800.0f,
        [] (float start, float end, float normalised) { return start * std::pow (end / start, normalised); },
        [] (float start, float end, float value) { return std::log (value / start) / std::log (end / start); });

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { detectHpfId, 1 }, "Detect HPF", hpfRange, detectorDefaultHz, "Hz"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { stereoLinkId, 1 }, "Stereo Link", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { biasId, 1 }, "Tube Bias", juce::NormalisableRange<float> (0.0f, 0.5f, 0.001f), CoreDSP::Saturation::defaultTubeBias));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { foldId, 1 }, "Fold", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { toneId, 1 }, "Tone", juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { curveId, 1 }, "Envelope Curve", juce::NormalisableRange<float> (0.5f, 2.0f, 0.001f), 1.0f));

    return { params.begin(), params.end() };
}

void DynamicSaturationProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();

    latencySamples = juce::roundToInt (oversampling.getLatencyInSamples());
    setLatencySamples (latencySamples);

    linkedEnvelope.prepare (sampleRate);

    for (int channel = 0; channel < maxChannels; ++channel)
    {
        channelEnvelope[(size_t) channel].prepare (sampleRate);
        detector[(size_t) channel].prepare (sampleRate, detectorDefaultHz);
        toneControls[(size_t) channel].prepare (sampleRate);
        dryDelay[(size_t) channel].assign ((size_t) latencySamples, 0.0f);
    }

    dryDelayPosition = 0;

    driveGain.reset (sampleRate, gainSmoothingSeconds);
    mixAmount.reset (sampleRate, gainSmoothingSeconds);
    outputGain.reset (sampleRate, gainSmoothingSeconds);

    driveGain.setCurrentAndTargetValue (CoreDSP::decibelsToGain (drive->load()));
    mixAmount.setCurrentAndTargetValue (mix->load() / 100.0f);
    outputGain.setCurrentAndTargetValue (CoreDSP::decibelsToGain (outputLevel->load()));

    dryBuffer.setSize (maxChannels, samplesPerBlock);
    preGainBuffer.setSize (maxChannels, samplesPerBlock);
    mixCurve.assign ((size_t) samplesPerBlock, 1.0f);
    outputCurve.assign ((size_t) samplesPerBlock, 1.0f);
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

    const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);
    const int numSamples = buffer.getNumSamples();

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    if (numChannels == 0 || numSamples == 0)
        return;

    // Block-rate parameters.
    const float sens = sensitivity->load();
    const float curveValue = curve->load();
    const float biasValue = bias->load();
    const float foldAmount = fold->load() / 100.0f;
    const float toneValue = tone->load();
    const float link = stereoLink->load() / 100.0f;
    const auto shaping = static_cast<CoreDSP::Saturation::Mode> (juce::roundToInt (mode->load()));
    const double sampleRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;

    linkedEnvelope.setTimes (attack->load(), release->load());

    for (int channel = 0; channel < maxChannels; ++channel)
    {
        channelEnvelope[(size_t) channel].setTimes (attack->load(), release->load());
        detector[(size_t) channel].setCutoff (sampleRate, detectHpf->load());
    }

    driveGain.setTargetValue (CoreDSP::decibelsToGain (drive->load()));
    mixAmount.setTargetValue (mix->load() / 100.0f);
    outputGain.setTargetValue (CoreDSP::decibelsToGain (outputLevel->load()));

    // Pass 1, base rate: envelope detection and per-sample pre-gain for each channel.
    for (int n = 0; n < numSamples; ++n)
    {
        std::array<float, maxChannels> detected {};
        float linkedPeak = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            detected[(size_t) channel] = detector[(size_t) channel].highpass (buffer.getSample (channel, n));
            linkedPeak = juce::jmax (linkedPeak, std::abs (detected[(size_t) channel]));
        }

        const float linked = linkedEnvelope.process (linkedPeak);
        const float baseDrive = driveGain.getNextValue();
        mixCurve[(size_t) n] = mixAmount.getNextValue();
        outputCurve[(size_t) n] = outputGain.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float own = channelEnvelope[(size_t) channel].process (std::abs (detected[(size_t) channel]));
            const float envelope = link * linked + (1.0f - link) * own;
            const float preGain = CoreDSP::Saturation::dynamicDrive (baseDrive, sens,
                                                                     CoreDSP::Saturation::shapeEnvelope (envelope, curveValue));
            preGainBuffer.setSample (channel, n, preGain);
        }
    }

    envelopeLevel.store (linkedEnvelope.getLevel(), std::memory_order_relaxed);

    // Keep the dry signal from before shaping for the mix.
    for (int channel = 0; channel < numChannels; ++channel)
        dryBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    // Pass 2, 2x oversampled: shape and fold. Each base-rate pre-gain covers two oversampled samples.
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
    const juce::dsp::AudioBlock<const float> input (block);
    auto upsampled = oversampling.processSamplesUp (input);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data = upsampled.getChannelPointer ((size_t) channel);
        const auto* gain = preGainBuffer.getReadPointer (channel);

        for (size_t j = 0; j < upsampled.getNumSamples(); ++j)
        {
            const float shaped = CoreDSP::Saturation::shape (data[j] * gain[j / 2], shaping, biasValue);
            data[j] = CoreDSP::Saturation::fold (shaped, foldAmount);
        }
    }

    oversampling.processSamplesDown (block);

    // Pass 3, base rate: tone, dry/wet mix with the aligned dry signal, and output level.
    const bool hasLatency = latencySamples > 0;

    for (int n = 0; n < numSamples; ++n)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto ch = (size_t) channel;
            // The oversampler and the tone control can overshoot slightly, so the stage ceiling is enforced here (+/-1).
            const float wet = juce::jlimit (-1.0f, 1.0f, toneControls[ch].process (buffer.getSample (channel, n), toneValue));

            const float dry = dryBuffer.getSample (channel, n);
            float aligned = dry;

            if (hasLatency)
            {
                aligned = dryDelay[ch][(size_t) dryDelayPosition];
                dryDelay[ch][(size_t) dryDelayPosition] = dry;
            }

            const float mixed = aligned + mixCurve[(size_t) n] * (wet - aligned);
            buffer.setSample (channel, n, mixed * outputCurve[(size_t) n]);
        }

        if (hasLatency)
            dryDelayPosition = (dryDelayPosition + 1) % latencySamples;
    }
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
