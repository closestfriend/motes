cmake_minimum_required(VERSION 3.22)
project(KineticGranular VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)

add_subdirectory(JUCE)

# Portable C DSP core (no JUCE)
add_library(granular_core STATIC
    src/core/granular_engine.c
    src/core/voice.c
    src/core/grain.c
    src/core/window.c
    src/core/wav.c)
target_include_directories(granular_core PUBLIC src/core)
target_link_libraries(granular_core PRIVATE m)

# Plugin
juce_add_plugin(KineticGranular
    PLUGIN_MANUFACTURER_CODE Hntr
    PLUGIN_CODE Kgrn
    FORMATS AU VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    PRODUCT_NAME "Kinetic Granular")

target_sources(KineticGranular PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp)
target_link_libraries(KineticGranular PRIVATE
    granular_core
    juce::juce_audio_utils
    juce::juce_dsp)
target_compile_definitions(KineticGranular PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
