juce_add_plugin(SpectralDrift
    COMPANY_NAME "AudioSuite"
    BUNDLE_ID com.audiosuite.spectraldrift
    PLUGIN_MANUFACTURER_CODE Asui
    PLUGIN_CODE Sdft
    PLUGIN_NAME "Spectral Drift Shifter"
    PRODUCT_NAME "Spectral Drift Shifter"
    FORMATS ${SUITE_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(SpectralDrift PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_link_libraries(SpectralDrift PRIVATE
    CoreDSP
    SuiteUI
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
)
