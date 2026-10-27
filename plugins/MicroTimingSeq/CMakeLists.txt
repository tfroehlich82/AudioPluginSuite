juce_add_plugin(MicroTimingSeq
    COMPANY_NAME "AudioSuite"
    BUNDLE_ID com.audiosuite.microtimingseq
    PLUGIN_MANUFACTURER_CODE Asui
    PLUGIN_CODE Mtsq
    PLUGIN_NAME "Micro-Timing Chaos Sequencer"
    PRODUCT_NAME "Micro-Timing Chaos Sequencer"
    FORMATS ${SUITE_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(MicroTimingSeq PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_link_libraries(MicroTimingSeq PRIVATE
    CoreDSP
    SuiteUI
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
)
