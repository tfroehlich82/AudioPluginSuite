juce_add_plugin(MicroTimingChaos
    COMPANY_NAME "AudioSuite"
    BUNDLE_ID com.audiosuite.microtimingchaos
    PLUGIN_MANUFACTURER_CODE Asui
    PLUGIN_CODE Mtch
    PLUGIN_NAME "Micro-Timing Chaos"
    PRODUCT_NAME "Micro-Timing Chaos"
    FORMATS ${SUITE_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

set(MICRO_TIMING_CHAOS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(MICRO_TIMING_CHAOS_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/LerpCurveEditor.cpp
    Source/LerpCurveEditor.h
    Source/LerpPreview.cpp
    Source/LerpPreview.h
    Source/LfoScope.cpp
    Source/LfoScope.h
    Source/ModulationPanel.cpp
    Source/ModulationPanel.h
    Source/MicroTimingChaosLookAndFeel.cpp
)

# Icons and textures are embedded in the binary, so they load from inside the plugin bundle.
juce_add_binary_data(MicroTimingChaosAssets
    SOURCES
        Resources/icons/bpm_sync_active.svg
        Resources/icons/bpm_sync_inactive.svg
        Resources/textures/bpm_sync_4k.png
        Resources/textures/sequencer_bg_4k.png
        Resources/textures/sequencer_grid_4k.png
)

target_sources(MicroTimingChaos PRIVATE ${MICRO_TIMING_CHAOS_SOURCES})

target_include_directories(MicroTimingChaos PRIVATE
    ${MICRO_TIMING_CHAOS_INCLUDE_DIR}
    ${MICRO_TIMING_CHAOS_INCLUDE_DIR}/Source
)

suite_ui_add_to_target(MicroTimingChaos)

target_link_libraries(MicroTimingChaos PRIVATE
    CoreDSP
    MicroTimingChaosAssets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
)

# Plugin tests: the real processor and editor sources, run headless with the GUI toolkit initialised.
if(SUITE_BUILD_TESTS)
    add_executable(MicroTimingChaosTests
        tests/PluginTests.cpp
        ${MICRO_TIMING_CHAOS_SOURCES}
    )

    target_include_directories(MicroTimingChaosTests PRIVATE
        ${MICRO_TIMING_CHAOS_INCLUDE_DIR}
        ${MICRO_TIMING_CHAOS_INCLUDE_DIR}/Source
        ${CMAKE_SOURCE_DIR}/tests/common
    )

    suite_ui_add_to_target(MicroTimingChaosTests)

    # The editor tests pump the message loop so timers fire.
    target_compile_definitions(MicroTimingChaosTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

    target_link_libraries(MicroTimingChaosTests PRIVATE
        CoreDSP
        MicroTimingChaosAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    )

    add_test(NAME MicroTimingChaos_PluginTests COMMAND MicroTimingChaosTests)
endif()
