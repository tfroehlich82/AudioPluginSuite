# SuiteUI has no library target. See SuiteUI.cmake and suite_ui_add_to_target().
# Its unit tests live here.
if(SUITE_BUILD_TESTS)
    add_executable(SuiteUITests tests/SuiteUITests.cpp)

    suite_ui_add_to_target(SuiteUITests)

    target_link_libraries(SuiteUITests PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_core
    )

    add_test(NAME SuiteUI_Tests COMMAND SuiteUITests)
endif()
