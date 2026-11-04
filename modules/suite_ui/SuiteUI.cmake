# SuiteUI: shared look and feel and visualiser widgets.
#
# The UI code is compiled into each target that uses it, rather than built as a library. A library
# bundles the JUCE modules it links to, and those then clash with the same modules linked into the
# consumer (duplicate symbols at link time). Each consumer links its JUCE modules itself.

set(SUITE_UI_ROOT "${CMAKE_CURRENT_LIST_DIR}")

function(suite_ui_add_to_target target)
    target_sources(${target} PRIVATE
        ${SUITE_UI_ROOT}/Source/SuiteLookAndFeel.cpp
        ${SUITE_UI_ROOT}/Source/SuiteWidgets.cpp
    )

    target_include_directories(${target} PRIVATE ${SUITE_UI_ROOT}/include)
endfunction()
