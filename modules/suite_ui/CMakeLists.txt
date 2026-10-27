# SuiteUI: shared look and feel and visualiser widgets used by every plugin editor.
add_library(SuiteUI STATIC
    Source/SuiteLookAndFeel.cpp
    Source/SuiteWidgets.cpp
)

target_include_directories(SuiteUI PUBLIC include)
target_link_libraries(SuiteUI
    PUBLIC
        juce::juce_audio_utils
        juce::juce_gui_basics
        juce::juce_gui_extra
)
