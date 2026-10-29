#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>

#include "SpectralEngine.h"

/**
    Spectral Drift Shifter.

    FFT overlap-add processing with an LFO that drifts the phase of every bin, plus spectral
    shaping (shift, tilt, gate). The channels run on LFOs offset by the Stereo Phase control,
    which decorrelates them and widens the image.
*/
class SpectralDriftProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBins = CoreDSP::Spectral::numBins;

    // Drift and mix
    static constexpr const char* driftRateId = "driftRate";
    static constexpr const char* depthId = "depth";
    static constexpr const char* spreadId = "spread";
    static constexpr const char* stereoPhaseId = "stereoPhase";
    static constexpr const char* lfoShapeId = "lfoShape";

    // Spectral shaping
    static constexpr const char* shiftId = "shift";
    static constexpr const char* tiltId = "tilt";
    static constexpr const char* gateId = "gate";

    // Feedback and freeze
    static constexpr const char* feedbackId = "feedback";
    static constexpr const char* mixId = "mix";
    static constexpr const char* freezeId = "freeze";
    static constexpr const char* feedbackDampingId = "feedbackDamping";
    static constexpr const char* driftStartId = "driftStart";
    static constexpr const char* outputGainId = "outputGain";

    static constexpr int numControls = 14;

    /** Largest feedback the plugin allows. The engine itself can go to 0.95. */
    static constexpr float maxFeedbackPercent = 12.0f;

    /** Spectra published for the editor. Written on the audio thread, read on the message thread. */
    struct SpectrumSnapshot
    {
        std::array<std::atomic<float>, numBins> input {};
        std::array<std::atomic<float>, numBins> output {};
        std::atomic<std::uint64_t> frames { 0 };
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

    void readSettings (CoreDSP::Spectral::DriftEngine::Settings& settings) const noexcept;

    std::array<CoreDSP::Spectral::DriftEngine, 2> engines;

    std::atomic<float>* driftRate = nullptr;
    std::atomic<float>* depth = nullptr;
    std::atomic<float>* spread = nullptr;
    std::atomic<float>* stereoPhase = nullptr;
    std::atomic<float>* lfoShape = nullptr;
    std::atomic<float>* shift = nullptr;
    std::atomic<float>* tilt = nullptr;
    std::atomic<float>* gate = nullptr;
    std::atomic<float>* feedback = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* freeze = nullptr;
    std::atomic<float>* feedbackDamping = nullptr;
    std::atomic<float>* driftStart = nullptr;
    std::atomic<float>* outputGain = nullptr;

    SpectrumSnapshot snapshot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralDriftProcessor)
};
