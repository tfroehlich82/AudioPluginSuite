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
    PLUGIN_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Resources"
    PLUGIN_RESOURCE_FILES
        "Resources/icons/*.svg"
        "Resources/textures/*.png"
)

target_sources(MicroTimingChaos PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/LerpCurveEditor.cpp
    Source/MicroTimingChaosLookAndFeel.cpp
)

suite_ui_add_to_target(MicroTimingChaos)

target_link_libraries(MicroTimingChaos PRIVATE
    CoreDSP
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
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
    )

    target_include_directories(MicroTimingChaosTests PRIVATE
        Source
        ${CMAKE_SOURCE_DIR}/tests/common
    )

    suite_ui_add_to_target(MicroTimingChaosTests)

    # The editor tests pump the message loop so timers fire.
    target_compile_definitions(MicroTimingChaosTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

    target_link_libraries(MicroTimingChaosTests PRIVATE
        CoreDSP
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    )

    add_test(NAME MicroTimingChaos_PluginTests COMMAND MicroTimingChaosTests)
endif()
