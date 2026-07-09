cmake_minimum_required(VERSION 3.22)
project(Motes VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)

# --- Auto-apply local JUCE patches (vendored, not upstream) -----------------
# The JUCE submodule is pinned to the clean upstream 7.0.12 tag commit so a
# fresh clone / `git submodule update --init` always works. The macOS 15 SDK
# CGWindowListCreateImage fix (needed because that tag predates the SDK 15
# obsoletion annotation) lives only as a patch file and is applied here,
# before add_subdirectory(JUCE), so every configure produces a patched tree.
# Guarded with `git apply --reverse --check` so re-running `cmake -B build`
# on an already-patched tree is a no-op instead of an error.
set(KGRN_JUCE_PATCH "${CMAKE_SOURCE_DIR}/patches/0001-juce-7.0.12-macos15-sdk-cgwindowlistcreateimage.patch")

execute_process(
    COMMAND git apply --reverse --check "${KGRN_JUCE_PATCH}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/JUCE"
    RESULT_VARIABLE KGRN_JUCE_PATCH_ALREADY_APPLIED
    OUTPUT_QUIET
    ERROR_QUIET)

if(KGRN_JUCE_PATCH_ALREADY_APPLIED EQUAL 0)
    message(STATUS "kinetic-granular: JUCE macOS15 SDK patch already applied, skipping")
else()
    execute_process(
        COMMAND git apply --check "${KGRN_JUCE_PATCH}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/JUCE"
        RESULT_VARIABLE KGRN_JUCE_PATCH_CAN_APPLY
        OUTPUT_QUIET
        ERROR_QUIET)

    if(NOT KGRN_JUCE_PATCH_CAN_APPLY EQUAL 0)
        message(FATAL_ERROR "kinetic-granular: JUCE macOS15 SDK patch (${KGRN_JUCE_PATCH}) does not apply cleanly to JUCE/. The submodule may not be at the expected 7.0.12 upstream commit, or the JUCE working tree is dirty. Aborting configure rather than building against an unpatched/unknown tree.")
    endif()

    execute_process(
        COMMAND git apply "${KGRN_JUCE_PATCH}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/JUCE"
        RESULT_VARIABLE KGRN_JUCE_PATCH_APPLY_RESULT)

    if(NOT KGRN_JUCE_PATCH_APPLY_RESULT EQUAL 0)
        message(FATAL_ERROR "kinetic-granular: failed to apply JUCE macOS15 SDK patch (${KGRN_JUCE_PATCH})")
    endif()

    message(STATUS "kinetic-granular: applied JUCE macOS15 SDK CGWindowListCreateImage patch")
endif()

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
juce_add_plugin(Motes
    COMPANY_NAME "closestfriend"     # vendor shown in the DAW's plugin-manufacturer list (was defaulting to "yourcompany")
    PLUGIN_MANUFACTURER_CODE Hntr
    PLUGIN_CODE Mote
    FORMATS AU VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE   # auto-install AU+VST3 to ~/Library/Audio/Plug-Ins on every build (no manual copy → no stale installs)
    PRODUCT_NAME "Motes")

target_sources(Motes PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
    src/plugin/MotesLookAndFeel.cpp)
target_link_libraries(Motes PRIVATE
    granular_core
    juce::juce_audio_utils
    juce::juce_dsp)
target_compile_definitions(Motes PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

# Portable C core test harness
add_executable(test_core tests/test_core.c tests/wav_fixture.c)
target_link_libraries(test_core PRIVATE granular_core)
target_include_directories(test_core PRIVATE tests)
# CMAKE_BUILD_TYPE=Release defines NDEBUG, which would strip the assert()
# calls the test relies on (turning the test into a silent no-op / crash on
# uninitialized data). Force asserts active for this target regardless of
# build type so the test harness actually exercises the code under test.
target_compile_options(test_core PRIVATE -UNDEBUG)
enable_testing()
add_test(NAME core COMMAND test_core)
