#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

#include "CoreDSP.h"

/**
    Dynamic Saturation Companion.

    Waveshaping whose drive follows the signal envelope: the louder the input,
    the harder the stage is pushed, scaled by the Sensitivity control.
*/
class DynamicSaturationProcessor : public juce::AudioProcessor
{
public:
    static constexpr const char* driveId = "drive";
    static constexpr const char* sensitivityId = "sensitivity";
    static constexpr const char* modeId = "mode";
    static constexpr const char* mixId = "mix";
    static constexpr const char* outputLevelId = "outputLevel";

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

    /** Linked envelope level (linear, 0 .. 1 for a full-scale signal), published for the editor's meter. */
    float getEnvelopeLevel() const noexcept { return envelopeLevel.load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    CoreDSP::Saturation::EnvelopeFollower envelope;

    juce::SmoothedValue<float> driveGain;
    juce::SmoothedValue<float> mixAmount;
    juce::SmoothedValue<float> outputGain;

    std::atomic<float>* drive = nullptr;
    std::atomic<float>* sensitivity = nullptr;
    std::atomic<float>* mode = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* outputLevel = nullptr;

    std::atomic<float> envelopeLevel { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicSaturationProcessor)
};
