# CoreDSP: header-only DSP primitives shared by every plugin.
add_library(CoreDSP INTERFACE)
target_include_directories(CoreDSP INTERFACE include)
target_link_libraries(CoreDSP INTERFACE juce::juce_dsp)

if(SUITE_BUILD_TESTS)
    # Run with: ctest --test-dir build -C Release --output-on-failure
    add_executable(CoreDSPTests tests/DSPTests.cpp)
    target_link_libraries(CoreDSPTests PRIVATE CoreDSP)
    add_test(NAME CoreDSPTests COMMAND CoreDSPTests)
endif()
