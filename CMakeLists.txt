# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(SIDSynth VERSION 0.5.1 LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "SIDSynth currently supports Linux only")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(SIDSYNTH_SANITIZERS "Instrument the plugin, core and tests with ASan/UBSan" OFF)
if(SIDSYNTH_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "SIDSYNTH_SANITIZERS requires GCC or Clang")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
include(CTest)
include(cmake/Dependencies.cmake)

add_library(sid_engine STATIC src/SidEngine.cpp src/SidBackend.cpp)
target_include_directories(sid_engine PUBLIC src)
target_link_libraries(sid_engine PRIVATE libresidfp)
target_compile_options(sid_engine PRIVATE -Wall -Wextra -Wpedantic)

juce_add_plugin(SIDSynth
    COMPANY_NAME "SIDSynth"
    BUNDLE_ID "org.sidsynth.instrument"
    PLUGIN_MANUFACTURER_CODE Sids
    PLUGIN_CODE Sydi
    PRODUCT_NAME "SIDSynth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    FORMATS VST3)
target_sources(SIDSynth PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp src/MidiRouter.cpp)
target_include_directories(SIDSynth PUBLIC src)
target_compile_definitions(SIDSynth PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SIDSynth PRIVATE sid_engine juce::juce_audio_processors juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)
target_compile_options(SIDSynth PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
    add_executable(sidsynth_offline tests/OfflineTests.cpp)
    target_link_libraries(sidsynth_offline PRIVATE SIDSynth juce::juce_audio_processors libresidfp)
    target_link_options(sidsynth_offline PRIVATE
        -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free -Wl,--wrap=pthread_mutex_lock)
    add_test(NAME offline_audio COMMAND sidsynth_offline "${CMAKE_BINARY_DIR}/stage5-a4.wav")
endif()

install(DIRECTORY "${CMAKE_BINARY_DIR}/SIDSynth_artefacts/$<CONFIG>/VST3/SIDSynth.vst3"
    DESTINATION "." COMPONENT Plugin)
install(FILES LICENSE third_party/NOTICES.md third_party/JUCE.spdx.json DESTINATION SIDSynth.vst3/Contents/Resources
    COMPONENT Plugin)
install(DIRECTORY third_party/licenses DESTINATION SIDSynth.vst3/Contents/Resources
    COMPONENT Plugin)
