#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

#include "CoreDSP.h"

/**
    Dynamic Saturation Companion.

    A saturation stage whose drive follows the signal envelope: the louder the input,
    the harder the stage is pushed, scaled by Sensitivity. The shaping runs at 2x
    oversampling to reduce aliasing. Its latency is reported to the host.
*/
class DynamicSaturationProcessor : public juce::AudioProcessor
{
public:
    // Core controls
    static constexpr const char* driveId = "drive";
    static constexpr const char* sensitivityId = "sensitivity";
    static constexpr const char* modeId = "mode";
    static constexpr const char* mixId = "mix";
    static constexpr const char* outputLevelId = "outputLevel";

    // Extended controls
    static constexpr const char* attackId = "attack";
    static constexpr const char* releaseId = "release";
    static constexpr const char* detectHpfId = "detectHpf";
    static constexpr const char* stereoLinkId = "stereoLink";
    static constexpr const char* biasId = "bias";
    static constexpr const char* foldId = "fold";
    static constexpr const char* toneId = "tone";
    static constexpr const char* curveId = "curve";

    static constexpr int numControls = 13;

    DynamicSaturationProcessor();
    ~DynamicSaturationProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Dynamic Saturation Companion"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    /** Linked detector level (linear), published for the editor's meter. */
    float getEnvelopeLevel() const noexcept { return envelopeLevel.load (std::memory_order_relaxed); }

    /** Latency added by the oversampler, in samples. */
    int getOversamplingLatency() const noexcept { return latencySamples; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static constexpr int maxChannels = 2;

    juce::dsp::Oversampling<float> oversampling { maxChannels, 1,
                                                  juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                                                  false, true };
    int latencySamples = 0;

    CoreDSP::Saturation::EnvelopeFollower linkedEnvelope;
    std::array<CoreDSP::Saturation::EnvelopeFollower, maxChannels> channelEnvelope;
    std::array<CoreDSP::Saturation::OnePoleFilter, maxChannels> detector;
    std::array<CoreDSP::Saturation::ToneControl, maxChannels> toneControls;

    juce::SmoothedValue<float> driveGain;
    juce::SmoothedValue<float> mixAmount;
    juce::SmoothedValue<float> outputGain;

    // Per-block scratch space, sized in prepareToPlay so the audio thread does not allocate.
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> preGainBuffer;
    std::vector<float> mixCurve;
    std::vector<float> outputCurve;

    // Dry path delayed by the oversampler latency, so the mix stays aligned.
    std::array<std::vector<float>, maxChannels> dryDelay;
    int dryDelayPosition = 0;

    std::atomic<float>* drive = nullptr;
    std::atomic<float>* sensitivity = nullptr;
    std::atomic<float>* mode = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* outputLevel = nullptr;
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* release = nullptr;
    std::atomic<float>* detectHpf = nullptr;
    std::atomic<float>* stereoLink = nullptr;
    std::atomic<float>* bias = nullptr;
    std::atomic<float>* fold = nullptr;
    std::atomic<float>* tone = nullptr;
    std::atomic<float>* curve = nullptr;

    std::atomic<float> envelopeLevel { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicSaturationProcessor)
};
