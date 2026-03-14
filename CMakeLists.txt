cmake_minimum_required(VERSION 3.22)

# Universal binary MUST be set before project()
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(ALCUBIERRE VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Fetch JUCE 7.0.9 ─────────────────────────────────────────────────────────
include(FetchContent)

set(JUCE_TAG 7.0.9)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${JUCE_TAG}
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(JUCE)

# ─── Plugin target ────────────────────────────────────────────────────────────
juce_add_plugin(ALCUBIERRE
    VERSION                     1.0.0
    COMPANY_NAME                "Finis Musicae Corp"
    PLUGIN_MANUFACTURER_CODE    FnMu
    PLUGIN_CODE                 Alcb
    FORMATS                     VST3 AU
    PRODUCT_NAME                "ALCUBIERRE"
    BUNDLE_ID                   "com.finismusicaecorp.alcubierre"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Fx" "Spatial"
    AU_MAIN_TYPE                "kAudioUnitType_Effect"
)

juce_generate_juce_header(ALCUBIERRE)

juce_add_binary_data(AlcubiereResources
    SOURCES Resources/alcubierre_bg.png
)

target_sources(ALCUBIERRE PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/GranularEngine.cpp
    Source/GlitchEngine.cpp
)

target_compile_definitions(ALCUBIERRE PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(ALCUBIERRE
    PRIVATE
        AlcubiereResources
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_graphics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
