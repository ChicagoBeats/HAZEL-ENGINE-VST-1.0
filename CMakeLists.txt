cmake_minimum_required(VERSION 3.15)
project(F1nzEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static C/C++ runtime: nothing extra to install on the target PC (important for old Windows).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
# JUCE 6.1.6: last release line that still targets Windows 7/8-era systems comfortably.
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        6.1.6
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(juce)

juce_add_binary_data(F1nzAssets SOURCES
    Assets/Samples/kick_airy.wav
    Assets/Samples/kick_444_222.wav
    Assets/Samples/rim_kaisa.wav
    Assets/Samples/perc_blocc.wav
    Assets/Samples/kick_u019c.wav
    Assets/Images/rim.png
    Assets/Images/car.png
    Assets/Images/bg_night.png
    Assets/Images/bg_day.png)

juce_add_plugin(F1nzEngine
    COMPANY_NAME            "f1nz"
    PLUGIN_MANUFACTURER_CODE F1nz
    PLUGIN_CODE             EngV
    FORMATS                 VST3 Standalone
    PRODUCT_NAME            "F1nz Engine"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         Instrument Synth)

target_sources(F1nzEngine PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(F1nzEngine PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(F1nzEngine
    PRIVATE
        F1nzAssets
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
