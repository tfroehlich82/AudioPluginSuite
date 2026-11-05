# SuiteModulation: shared LFO engine, routing, modulation display, and LFO scope.
#
# The module sources compile into each consumer plugin, following the same pattern
# as suite_ui. A static library would bundle JUCE modules and cause duplicate
# symbol errors at link time, so sources are compiled directly.

set(SUITE_MODULATION_ROOT "${CMAKE_CURRENT_LIST_DIR}")

function(suite_modulation_add_to_target target)
    target_sources(${target} PRIVATE
        ${SUITE_MODULATION_ROOT}/Source/SuiteModulationRouter.cpp
        ${SUITE_MODULATION_ROOT}/Source/SuiteModulationScope.cpp
        ${SUITE_MODULATION_ROOT}/Source/SuiteModulationPanel.cpp
    )

    target_include_directories(${target} PRIVATE ${SUITE_MODULATION_ROOT}/include)
endfunction()
