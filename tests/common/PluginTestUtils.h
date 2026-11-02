#pragma once

// Shared helpers for the plugin test executables.
//
// NDEBUG is undefined before <cassert> so the checks also run in Release, as in the DSP tests.

#undef NDEBUG
#include <cassert>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>

namespace PluginTest
{
    constexpr double pi = 3.14159265358979323846;

    inline void pass (const char* name)
    {
        std::printf ("[PASS] %s\n", name);
    }

    /** Sets a parameter by its denormalised value, the way a user would dial it in. */
    inline void setParameter (juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
    {
        auto* parameter = apvts.getParameter (id);
        assert (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    /** Reads a parameter's current denormalised value. */
    inline float getParameter (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        return apvts.getRawParameterValue (id)->load();
    }

    /** Checks that every id exists, and that the count matches the spec, so no control goes missing. */
    inline void checkParameterIds (juce::AudioProcessor& processor, juce::AudioProcessorValueTreeState& apvts,
                                   std::initializer_list<const char*> ids, int expectedCount)
    {
        assert ((int) processor.getParameters().size() == expectedCount);

        for (auto* id : ids)
            assert (apvts.getParameter (id) != nullptr);
    }

    /** Checks a parameter's range and its default value. */
    inline void checkRange (juce::AudioProcessorValueTreeState& apvts, const char* id,
                            float expectedMin, float expectedMax, float expectedDefault)
    {
        auto* parameter = apvts.getParameter (id);
        assert (parameter != nullptr);

        const auto range = parameter->getNormalisableRange();
        assert (std::abs (range.start - expectedMin) < 1e-4f);
        assert (std::abs (range.end - expectedMax) < 1e-4f);

        const float defaultValue = range.convertFrom0to1 (parameter->getDefaultValue());
        assert (std::abs (defaultValue - expectedDefault) < 1e-3f);
    }

    /** Saves the state, changes a parameter, restores the state and checks the parameter is back. */
    inline void checkStateRoundTrip (juce::AudioProcessor& processor, juce::AudioProcessorValueTreeState& apvts,
                                     const char* id, float alternativeValue)
    {
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        assert (state.getSize() > 0);

        const float original = getParameter (apvts, id);
        setParameter (apvts, id, alternativeValue);
        assert (std::abs (getParameter (apvts, id) - alternativeValue) < 1e-3f);

        processor.setStateInformation (state.getData(), (int) state.getSize());
        assert (std::abs (getParameter (apvts, id) - original) < 1e-3f);
    }

    /** Creates the editor, lets timers run for a moment and paints it into an image. */
    inline void checkEditor (juce::AudioProcessor& processor)
    {
        assert (processor.hasEditor());

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorIfNeeded());
        assert (editor != nullptr);
        assert (editor->getWidth() > 0 && editor->getHeight() > 0);

        // Runs the message loop, so the editor's 60 Hz timer fires and reads the processor.
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);

        juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
        juce::Graphics g (image);
        editor->paintEntireComponent (g, true);
    }

    inline bool allFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                if (! std::isfinite (buffer.getSample (channel, n)))
                    return false;

        return true;
    }

    inline float peak (const juce::AudioBuffer<float>& buffer)
    {
        float result = 0.0f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            result = juce::jmax (result, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));

        return result;
    }

    /** Adds a sine of the given frequency and amplitude, using the global sample index for phase. */
    inline void addSine (juce::AudioBuffer<float>& buffer, int channel, double sampleRate, double hz,
                         float amplitude, int64_t firstSample)
    {
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const double t = (double) (firstSample + n) / sampleRate;
            buffer.addSample (channel, n, amplitude * (float) std::sin (2.0 * pi * hz * t));
        }
    }
}
