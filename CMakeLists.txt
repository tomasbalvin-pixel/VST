cmake_minimum_required(VERSION 3.22)

project(Patina VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# JUCE: use a local checkout if given (-DPATINA_JUCE_DIR=/path/to/JUCE), otherwise fetch it.
set(PATINA_JUCE_DIR "" CACHE PATH "Path to an existing JUCE checkout")
option(PATINA_BUILD_TESTS "Build the headless host test tool" OFF)

if(PATINA_JUCE_DIR)
    add_subdirectory(${PATINA_JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(PATINA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PATINA_FORMATS AU)
endif()

juce_add_plugin(Patina
    COMPANY_NAME "Tomas Balvin"
    BUNDLE_ID "com.tomasbalvin.patina"
    PLUGIN_MANUFACTURER_CODE Tbal
    PLUGIN_CODE Ptn1
    FORMATS ${PATINA_FORMATS}
    PRODUCT_NAME "Patina"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Delay Reverb Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Patina PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Patina PRIVATE Source)

# Sources are UTF-8.
target_compile_options(Patina PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_compile_definitions(Patina PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Patina
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(PATINA_BUILD_TESTS)
    add_subdirectory(tools)
endif()
