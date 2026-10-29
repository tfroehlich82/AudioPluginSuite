// Loads a built VST3 bundle through JUCE's own plugin host, the same way a DAW does:
// scan, instantiate, open the editor, prepare, and process audio and MIDI.
//
// Usage: PluginHostTests <path-to-.vst3-bundle>
// Exit code 0 means every step succeeded.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

namespace
{
    int fail (const char* step, const juce::String& detail = {})
    {
        std::printf ("[FAIL] %s%s%s\n", step, detail.isEmpty() ? "" : ": ", detail.toRawUTF8());
        return 1;
    }
}

int main (int argc, char** argv)
{
    if (argc < 2)
        return fail ("usage", "PluginHostTests <bundle>");

    const juce::File bundle { juce::String (argv[1]) };

    if (! bundle.exists())
        return fail ("bundle", "does not exist: " + bundle.getFullPathName());

    // The editor needs the GUI toolkit initialised, as it is inside a DAW.
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioPluginFormatManager formats;
    formats.addDefaultFormats();

    juce::OwnedArray<juce::PluginDescription> types;

    for (int i = 0; i < formats.getNumFormats(); ++i)
    {
        auto* format = formats.getFormat (i);

        if (format->getName() == "VST3")
            format->findAllTypesForFile (types, bundle.getFullPathName());
    }

    if (types.isEmpty())
        return fail ("scan", "no plugin types found in bundle");

    const auto& description = *types.getFirst();
    std::printf ("[INFO] %s | category=%s | audio in=%d out=%d | instrument=%d\n",
                 description.name.toRawUTF8(), description.category.toRawUTF8(),
                 description.numInputChannels, description.numOutputChannels,
                 description.isInstrument ? 1 : 0);

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    juce::String error;

    auto instance = formats.createPluginInstance (description, sampleRate, blockSize, error);

    if (instance == nullptr)
        return fail ("instantiate", error);

    std::printf ("[INFO] buses: audio in=%d out=%d | accepts MIDI=%d | produces MIDI=%d\n",
                 instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels(),
                 instance->acceptsMidi() ? 1 : 0, instance->producesMidi() ? 1 : 0);

    if (instance->hasEditor())
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorIfNeeded());

        if (editor == nullptr)
            return fail ("editor", "createEditorIfNeeded returned nullptr");

        std::printf ("[INFO] editor created: %d x %d\n", editor->getWidth(), editor->getHeight());
    }

    instance->prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio (juce::jmax (1, instance->getTotalNumInputChannels()), blockSize);
    audio.clear();

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 256);

    for (int block = 0; block < 8; ++block)
    {
        if (block == 0)
            instance->processBlock (audio, midi);
        else
        {
            juce::MidiBuffer empty;
            instance->processBlock (audio, empty);
        }
    }

    instance->releaseResources();

    std::printf ("[PASS] %s loads, opens its editor, prepares and processes\n", description.name.toRawUTF8());
    return 0;
}
