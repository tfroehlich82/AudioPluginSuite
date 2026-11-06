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

suite_ui_add_to_target(DynamicSaturation)

# The brand lockup (FROTH Audio wordmark, mark and the product name) is embedded, so the header loads it from inside the plugin.
juce_add_binary_data(DynamicSaturationAssets
    SOURCES ../../modules/suite_ui/brand/lockup_dynamic_saturation.png
)

target_link_libraries(DynamicSaturation PRIVATE
    CoreDSP
    DynamicSaturationAssets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
)

# Plugin tests: the real processor and editor sources, run headless with the GUI toolkit initialised.
if(SUITE_BUILD_TESTS)
    add_executable(DynamicSaturationTests
        tests/PluginTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
    )

    target_include_directories(DynamicSaturationTests PRIVATE
        Source
        ${CMAKE_SOURCE_DIR}/tests/common
    )

    suite_ui_add_to_target(DynamicSaturationTests)

    target_link_libraries(DynamicSaturationTests PRIVATE
        CoreDSP
        DynamicSaturationAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    )

    # The editor tests pump the message loop so timers fire.
    target_compile_definitions(DynamicSaturationTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

    add_test(NAME DynamicSaturation_PluginTests COMMAND DynamicSaturationTests)
endif()
