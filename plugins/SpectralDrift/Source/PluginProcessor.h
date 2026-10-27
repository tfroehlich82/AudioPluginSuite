#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

#include "SpectralEngine.h"

/**
    Spectral Drift Shifter.

    FFT overlap-add processing with an LFO that drifts the phase of every bin.
    The left and right channels run on quadrature LFOs, which decorrelates them and widens the image.
*/
class SpectralDriftProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBins = CoreDSP::Spectral::numBins;

    static constexpr const char* driftRateId = "driftRate";
    static constexpr const char* freezeId = "freeze";
    static constexpr const char* mixId = "mix";
    static constexpr const char* feedbackId = "feedback";

    /** Spectra published for the editor. Written on the audio thread, read on the message thread. */
    struct SpectrumSnapshot
    {
        std::array<std::atomic<float>, numBins> input {};
        std::array<std::atomic<float>, numBins> output {};
    };

    SpectralDriftProcessor();
    ~SpectralDriftProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Spectral Drift Shifter"; }
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

    const SpectrumSnapshot& getSpectrumSnapshot() const noexcept { return snapshot; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::array<CoreDSP::Spectral::DriftEngine, 2> engines { CoreDSP::Spectral::DriftEngine (0.0f),
                                                            CoreDSP::Spectral::DriftEngine (juce::MathConstants<float>::halfPi) };

    std::atomic<float>* driftRate = nullptr;
    std::atomic<float>* freeze = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* feedback = nullptr;

    SpectrumSnapshot snapshot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralDriftProcessor)
};
