# Host-level tests: load the built VST3 bundles through JUCE's plugin host.
# Only meaningful when the plugins are built.
add_executable(PluginHostTests PluginHostTests.cpp)

target_compile_definitions(PluginHostTests PRIVATE
    JUCE_PLUGINHOST_VST3=1     # the host is compiled out by default in JUCE 7
)

target_link_libraries(PluginHostTests PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_core
)

# Each plugin's VST3 bundle sits three levels above the binary in Contents/<arch>/.
function(suite_add_host_test plugin)
    add_test(NAME ${plugin}_VST3_HostLoad
             COMMAND PluginHostTests "$<TARGET_FILE_DIR:${plugin}_VST3>/../.." )
    add_dependencies(PluginHostTests ${plugin}_VST3)
endfunction()

suite_add_host_test(SpectralDrift)
suite_add_host_test(DynamicSaturation)
suite_add_host_test(MicroTimingChaos)
