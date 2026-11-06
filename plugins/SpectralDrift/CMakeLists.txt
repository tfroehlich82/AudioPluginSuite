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

suite_ui_add_to_target(SpectralDrift)

# The brand lockup (FROTH Audio wordmark, mark and the product name) is embedded, so the header loads it from inside the plugin.
juce_add_binary_data(SpectralDriftAssets
    SOURCES ../../modules/suite_ui/brand/lockup_spectral_drift.png
)

target_link_libraries(SpectralDrift PRIVATE
    CoreDSP
    SpectralDriftAssets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
)

# Plugin tests: the real processor and editor sources, run headless with the GUI toolkit initialised.
if(SUITE_BUILD_TESTS)
    add_executable(SpectralDriftTests
        tests/PluginTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
    )

    target_include_directories(SpectralDriftTests PRIVATE
        Source
        ${CMAKE_SOURCE_DIR}/tests/common
    )

    suite_ui_add_to_target(SpectralDriftTests)

    target_link_libraries(SpectralDriftTests PRIVATE
        CoreDSP
        SpectralDriftAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    )

    # The editor tests pump the message loop so timers fire.
    target_compile_definitions(SpectralDriftTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

    add_test(NAME SpectralDrift_PluginTests COMMAND SpectralDriftTests)
endif()
