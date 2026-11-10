cmake_minimum_required(VERSION 3.22)
project(SemitoneStudio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()
if(MSVC)
    # Static runtime: the plugin needs no Visual C++ redistributable on the user's PC
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# The interface is the same single-file web app as the online version (web/semitone-studio.html),
# wrapped into a full HTML document and compiled into the plugin.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(ST_WEB_OUT ${CMAKE_BINARY_DIR}/web/index.html)
add_custom_command(
    OUTPUT ${ST_WEB_OUT}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/wrap_web.py ${CMAKE_SOURCE_DIR}/web/semitone-studio.html ${ST_WEB_OUT}
    DEPENDS ${CMAKE_SOURCE_DIR}/web/semitone-studio.html ${CMAKE_SOURCE_DIR}/tools/wrap_web.py
    COMMENT "Wrapping the Semitone web interface")
add_custom_target(SemitoneWebWrap DEPENDS ${ST_WEB_OUT})

option(ST_LOCAL_CHECK "Compile-check only (no webkit on this machine)" OFF)
if(ST_LOCAL_CHECK)
    set(ST_LINUX_WEBVIEW FALSE)
else()
    set(ST_LINUX_WEBVIEW TRUE)
endif()

if(APPLE)
    set(ST_FORMATS VST3 AU Standalone)
else()
    set(ST_FORMATS VST3 Standalone)
endif()

juce_add_plugin(SemitoneStudio
    COMPANY_NAME "Semitone Studio"
    BUNDLE_ID com.semitonestudio.plugin
    PLUGIN_MANUFACTURER_CODE Smtn
    PLUGIN_CODE Smt1
    FORMATS ${ST_FORMATS}
    PRODUCT_NAME "Semitone Studio"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon-1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon-256.png"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEBVIEW2 TRUE
    NEEDS_WEB_BROWSER ${ST_LINUX_WEBVIEW}
    VST3_CATEGORIES Instrument Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Semitone records your voice or instrument and listens to find notes and keys.")

juce_add_binary_data(SemitoneWeb SOURCES ${ST_WEB_OUT})
add_dependencies(SemitoneWeb SemitoneWebWrap)

target_sources(SemitoneStudio PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

if(APPLE)
    target_sources(SemitoneStudio PRIVATE Source/MacDrag.mm)
endif()

target_compile_definitions(SemitoneStudio PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1)

target_link_libraries(SemitoneStudio
    PRIVATE
        SemitoneWeb
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
