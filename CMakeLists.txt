cmake_minimum_required(VERSION 3.22)
project(AudioPluginSuite VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SUITE_BUILD_PLUGINS "Build the VST3 / AU / Standalone plugins" ON)
option(SUITE_BUILD_TESTS "Build and register the unit tests" ON)

# JUCE is pinned to a release tag so that builds are reproducible.
include(FetchContent)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Applied to every target, including JUCE's own modules.
add_compile_definitions(
    JUCE_VST3_CAN_REPLACE_VST2=0   # no legacy VST2 flags
    JUCE_WEB_BROWSER=0             # the suite does not use web views or curl
    JUCE_USE_CURL=0
)

# AU is macOS-only. Windows builds VST3 and Standalone.
if(APPLE)
    set(SUITE_PLUGIN_FORMATS VST3 AU Standalone)
else()
    set(SUITE_PLUGIN_FORMATS VST3 Standalone)
endif()

enable_testing()

if(SUITE_BUILD_TESTS OR SUITE_BUILD_PLUGINS)
    add_subdirectory(modules/core_dsp)
endif()

if(SUITE_BUILD_PLUGINS)
    add_subdirectory(modules/suite_ui)
    add_subdirectory(plugins/SpectralDrift)
    add_subdirectory(plugins/DynamicSaturation)
    add_subdirectory(plugins/MicroTimingSeq)
endif()
