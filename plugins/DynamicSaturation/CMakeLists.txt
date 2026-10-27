juce_add_plugin(DynamicSaturation
    COMPANY_NAME "AudioSuite"
    BUNDLE_ID com.audiosuite.dynamicsaturation
    PLUGIN_MANUFACTURER_CODE Asui
    PLUGIN_CODE Dsat
    PLUGIN_NAME "Dynamic Saturation Companion"
    PRODUCT_NAME "Dynamic Saturation Companion"
    FORMATS ${SUITE_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(DynamicSaturation PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_link_libraries(DynamicSaturation PRIVATE
    CoreDSP
    SuiteUI
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
)
