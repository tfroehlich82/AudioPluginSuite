# suite_modulation has no standalone library target. Its sources are added to consumers via
# suite_modulation_add_to_target() in suite_modulation.cmake.
#
# The module's own tests build only with the plugins (the panel needs SuiteUI). This directory is
# added from the root CMakeLists.txt inside the SUITE_BUILD_PLUGINS block.
if(SUITE_BUILD_TESTS)
    add_executable(SuiteModulationTests
        tests/SuiteModulationTests.cpp
    )

    suite_ui_add_to_target(SuiteModulationTests)
    suite_modulation_add_to_target(SuiteModulationTests)

    target_link_libraries(SuiteModulationTests PRIVATE
        CoreDSP
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    )

    add_test(NAME SuiteModulation_Tests COMMAND SuiteModulationTests)
endif()
