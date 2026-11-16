cmake_minimum_required(VERSION 3.22)

project(Serene VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS: build a universal binary (Apple Silicon + Intel)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# Use a local JUCE checkout if JUCE_DIR is given, otherwise download it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(SERENE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SERENE_FORMATS AU)
endif()

juce_add_plugin(Serene
    COMPANY_NAME "Yaniv"
    PLUGIN_MANUFACTURER_CODE Yanv
    PLUGIN_CODE Srn1
    FORMATS ${SERENE_FORMATS}
    PRODUCT_NAME "Serene"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Serene PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SpectralEngine.cpp
    Source/SpectrumView.cpp)

target_compile_definitions(Serene PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Serene
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
