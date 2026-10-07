# Headless host that loads the built Patina.vst3 and renders test signals through it.
juce_add_console_app(PatinaHostTest PRODUCT_NAME "PatinaHostTest")

target_sources(PatinaHostTest PRIVATE HostTest.cpp)

target_compile_definitions(PatinaHostTest PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    PATINA_VST3_PATH="$<TARGET_FILE_DIR:Patina_VST3>/../..")

target_link_libraries(PatinaHostTest PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

add_dependencies(PatinaHostTest Patina_VST3)
