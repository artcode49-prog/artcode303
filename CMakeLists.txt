cmake_minimum_required(VERSION 3.22)

project(Acid303 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
endif()

# Build a universal plugin on macOS so it loads in both native and Rosetta
# hosts (an x86_64-only cmake would otherwise produce an Intel-only binary).
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Target architectures" FORCE)
    message(STATUS "Building universal binary: ${CMAKE_OSX_ARCHITECTURES}")
endif()

#-------------------------------------------------------------------------------
# JUCE: use a local checkout if JUCE_PATH is given, otherwise fetch it.
#-------------------------------------------------------------------------------
set(JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout")

if(JUCE_PATH STREQUAL "")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
else()
    add_subdirectory("${JUCE_PATH}" JUCE)
endif()

#-------------------------------------------------------------------------------
# Embedded GUI assets (logo, knob filmstrip, wooden side cheeks)
#-------------------------------------------------------------------------------
juce_add_binary_data(Acid303Assets SOURCES
    Source/gui/logo.png
    Source/gui/knob.png
    Source/gui/sidewood.png)

#-------------------------------------------------------------------------------
# The plugin itself
#-------------------------------------------------------------------------------
juce_add_plugin(Acid303
    COMPANY_NAME              "AcidLab"
    PLUGIN_MANUFACTURER_CODE  Acid
    PLUGIN_CODE               Tb03
    PRODUCT_NAME              "Acid303"
    DESCRIPTION               "TB-303 emulation: 4 pole diode ladder VCF, 16 step sequencer"
    FORMATS                   VST3
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Instrument" "Synth"
)

juce_generate_juce_header(Acid303)

target_sources(Acid303 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Acid303 PRIVATE Source)

target_compile_definitions(Acid303 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(Acid303 PRIVATE
    Acid303Assets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

#-------------------------------------------------------------------------------
# Offline renderer for the DSP core (no JUCE needed at all)
#-------------------------------------------------------------------------------
add_executable(tb303_render Tools/tb303_render.cpp)
target_include_directories(tb303_render PRIVATE Source/dsp)

#-------------------------------------------------------------------------------
# End to end check: loads the built VST3 with JUCE's plugin host and renders it
# to a wav file, so the packaged plugin itself can be verified without a DAW.
#-------------------------------------------------------------------------------
juce_add_console_app(tb303_hosttest PRODUCT_NAME "tb303_hosttest")

juce_generate_juce_header(tb303_hosttest)

target_sources(tb303_hosttest PRIVATE Tools/host_test.cpp)

target_compile_definitions(tb303_hosttest PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(tb303_hosttest PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags)

#-------------------------------------------------------------------------------
# Renders the plugin's editor to a PNG (GUI smoke test)
#-------------------------------------------------------------------------------
juce_add_console_app(tb303_screenshot PRODUCT_NAME "tb303_screenshot")

juce_generate_juce_header(tb303_screenshot)

target_sources(tb303_screenshot PRIVATE
    Tools/screenshot.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(tb303_screenshot PRIVATE Source)

target_compile_definitions(tb303_screenshot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_MODAL_LOOPS_PERMITTED=1)

target_link_libraries(tb303_screenshot PRIVATE
    Acid303Assets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags)
