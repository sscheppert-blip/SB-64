cmake_minimum_required(VERSION 3.22)

project(SB64 VERSION 0.1.0 LANGUAGES C CXX)

option(SB64_BUILD_TESTS "Build the offline tests" ON)
option(SB64_COPY_PLUGIN "Copy the built plugins into the user's plugin folders" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# Dependencies are fetched into the build tree, never committed.
include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.3
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL)

FetchContent_Declare(clap_juce_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        7adee3a1bd4684d4caa5601100e364abccff4b4d
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(JUCE clap_juce_extensions)

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
juce_add_plugin(SB64
    PRODUCT_NAME             "SB-64"
    COMPANY_NAME             "sscheppert-blip"
    COMPANY_WEBSITE          "https://github.com/sscheppert-blip"
    BUNDLE_ID                "io.github.sscheppert-blip.sb64"
    PLUGIN_MANUFACTURER_CODE Sbli
    PLUGIN_CODE              Sb64
    ICON_BIG                 "${CMAKE_CURRENT_SOURCE_DIR}/design/icon.png"
    FORMATS                  AU VST3 Standalone
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  ${SB64_COPY_PLUGIN}
    VST3_CATEGORIES          Instrument Synth Mono
    AU_MAIN_TYPE             kAudioUnitType_MusicDevice
    DESCRIPTION              "Analog-style acid bassline synthesizer with a 64-step sequencer")

set(SB64_SOURCES
    Source/dsp/Voice.cpp
    Source/dsp/Lfo.cpp
    Source/Menu.cpp
    Source/Parameters.cpp
    Source/PluginProcessor.cpp
    Source/StandaloneApp.cpp
    Source/PluginEditor.cpp
    Source/ui/Widgets.cpp)

target_sources(SB64 PRIVATE ${SB64_SOURCES})

# Michroma (labels, title) and DSEG14 Classic (segment display), both SIL Open Font License
# (design/fonts/OFL-*.txt)
juce_add_binary_data(SB64Fonts
    HEADER_NAME SB64Fonts.h
    NAMESPACE   SB64Fonts
    SOURCES     design/fonts/Michroma-Regular.ttf
                design/fonts/DSEG14Classic-Bold.ttf)

target_include_directories(SB64 PRIVATE Source)

target_compile_definitions(SB64
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)   # Source/StandaloneApp.cpp: native title bar

target_link_libraries(SB64
    PRIVATE
        SB64Fonts
        clap_juce_extensions
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(TARGET SB64
    CLAP_ID       "io.github.sscheppert-blip.sb64"
    CLAP_FEATURES instrument synthesizer mono)

# JUCE writes moduleinfo.json into the VST3 after signing, and the CLAP and AU bundles are
# not signed at all; Apple Silicon hosts refuse to load bundles without a valid signature.
if(APPLE)
    # The copy into ~/Library/Audio/Plug-Ins happens before this step, so the
    # installed copy is re-signed as well.
    foreach(format VST3 CLAP AU)
        set(folder ${format})
        if(format STREQUAL "AU")
            set(folder Components)
        endif()
        set(installed "$ENV{HOME}/Library/Audio/Plug-Ins/${folder}/$<TARGET_BUNDLE_DIR_NAME:SB64_${format}>")
        add_custom_command(TARGET SB64_${format} POST_BUILD
            COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:SB64_${format}>"
            COMMAND test ! -d "${installed}" || codesign --force --deep --sign - "${installed}"
            VERBATIM)
    endforeach()

    # The linker signs only the Standalone binary, not its resources, so sign the whole app;
    # with SB64_COPY_PLUGIN the signed app also replaces ~/Applications/SB-64.app.
    add_custom_command(TARGET SB64_Standalone POST_BUILD
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:SB64_Standalone>"
        VERBATIM)
    if(SB64_COPY_PLUGIN)
        set(installedApp "$ENV{HOME}/Applications/$<TARGET_BUNDLE_DIR_NAME:SB64_Standalone>")
        add_custom_command(TARGET SB64_Standalone POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${installedApp}"
            COMMAND ditto "$<TARGET_BUNDLE_DIR:SB64_Standalone>" "${installedApp}"
            COMMAND ${CMAKE_COMMAND} -E echo "Installed ${installedApp}"
            VERBATIM)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(SB64_BUILD_TESTS)
    juce_add_console_app(SB64Tests PRODUCT_NAME "SB64Tests")

    target_sources(SB64Tests PRIVATE ${SB64_SOURCES} tests/Tests.cpp)
    target_include_directories(SB64Tests PRIVATE Source)
    target_compile_definitions(SB64Tests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_MODAL_LOOPS_PERMITTED=1
            JucePlugin_Name="SB-64")

    target_link_libraries(SB64Tests
        PRIVATE
            SB64Fonts
            clap_juce_extensions
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    enable_testing()
    add_test(NAME SB64Tests COMMAND SB64Tests)
endif()
