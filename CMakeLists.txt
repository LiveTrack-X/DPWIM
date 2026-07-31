cmake_minimum_required(VERSION 3.22)

project(DPWIM VERSION 0.3.4 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "DPWIM currently supports Windows only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /permissive-)
endif()

include(FetchContent)

set(DPWIM_JUCE_SOURCE_DIR "" CACHE PATH "Optional existing JUCE source directory")
set(DPWIM_VST2_SDK_PATH "" CACHE PATH "Optional valid VST2 SDK root")
option(DPWIM_BUILD_TESTS "Build deterministic core tests" ON)

if(DPWIM_JUCE_SOURCE_DIR)
    add_subdirectory("${DPWIM_JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/juce")
else()
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7.0.12
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

set(DPWIM_FORMATS VST3)
if(DPWIM_VST2_SDK_PATH)
    if(EXISTS "${DPWIM_VST2_SDK_PATH}/pluginterfaces/vst2.x/aeffect.h")
        juce_set_vst2_sdk_path("${DPWIM_VST2_SDK_PATH}")
        list(APPEND DPWIM_FORMATS VST)
        message(STATUS "DPWIM VST2 enabled from ${DPWIM_VST2_SDK_PATH}")
    else()
        message(FATAL_ERROR "DPWIM_VST2_SDK_PATH does not contain pluginterfaces/vst2.x/aeffect.h")
    endif()
else()
    message(STATUS "DPWIM VST2 SDK not supplied; building VST3 only")
endif()

add_library(dpwim_core STATIC
    Source/Audio/AudioRingBuffer.h
    Source/Audio/AudioRingBuffer.cpp
    Source/Audio/DryDelay.h
    Source/Audio/DryDelay.cpp
    Source/Audio/SyncTimeline.h
    Source/Audio/SyncTimeline.cpp
    Source/Platform/ProcessCatalog.h
    Source/Platform/ProcessCatalog.cpp
    Source/Platform/ProcessLoopbackCapture.h
    Source/Platform/ProcessLoopbackCapture.cpp
)

target_include_directories(dpwim_core PUBLIC Source)
target_compile_definitions(dpwim_core PUBLIC
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)
target_link_libraries(dpwim_core PUBLIC
    mmdevapi
    ole32
    uuid
)

juce_add_plugin(DPWIM
    COMPANY_NAME "LiveTrack"
    BUNDLE_ID "com.livetrack.dpwim"
    PLUGIN_MANUFACTURER_CODE DPip
    PLUGIN_CODE DPWM
    FORMATS ${DPWIM_FORMATS}
    PRODUCT_NAME "DirectPipe Windows Input Mixer"
    VERSION ${PROJECT_VERSION}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(DPWIM PRIVATE
    Source/DSP/PhaseVocoderPitchShifter.h
    Source/DSP/PhaseVocoderPitchShifter.cpp
    Source/Platform/UpdateChecker.h
    Source/Platform/UpdateChecker.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)

juce_generate_juce_header(DPWIM)

if(TARGET DPWIM_rc_lib)
    get_target_property(
        DPWIM_GENERATED_SOURCES_DIRECTORY
        DPWIM JUCE_GENERATED_SOURCES_DIRECTORY
    )
    get_target_property(DPWIM_INFO_FILE DPWIM JUCE_INFO_FILE)
    set(
        DPWIM_VERSION_RESOURCE
        "${DPWIM_GENERATED_SOURCES_DIRECTORY}/DPWIM_resources.rc"
    )
    set(
        DPWIM_VERSION_RESOURCE_STAMP
        "${DPWIM_GENERATED_SOURCES_DIRECTORY}/DPWIM_resources.version.stamp"
    )
    add_custom_command(
        OUTPUT "${DPWIM_VERSION_RESOURCE_STAMP}"
        COMMAND juce::juceaide rcfile
                "${DPWIM_INFO_FILE}" "${DPWIM_VERSION_RESOURCE}"
        COMMAND "${CMAKE_COMMAND}" -E touch
                "${DPWIM_VERSION_RESOURCE_STAMP}"
        DEPENDS "${DPWIM_INFO_FILE}" juce::juceaide
        VERBATIM
    )
    add_custom_target(
        dpwim-version-resource
        DEPENDS "${DPWIM_VERSION_RESOURCE_STAMP}"
    )
    add_dependencies(DPWIM_rc_lib dpwim-version-resource)
endif()

target_compile_definitions(DPWIM PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(DPWIM PRIVATE
    dpwim_core
    winhttp
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if(DPWIM_BUILD_TESTS)
    enable_testing()
    add_executable(dpwim-tests
        Tests/test_main.cpp
    )
    target_link_libraries(dpwim-tests PRIVATE dpwim_core)
    add_test(NAME dpwim-core-tests COMMAND dpwim-tests)

    add_executable(dpwim-ring-buffer-generation-tests
        Tests/audio_ring_buffer_generation.cpp
        Source/Audio/AudioRingBuffer.cpp
    )
    target_include_directories(
        dpwim-ring-buffer-generation-tests PRIVATE Source
    )
    target_compile_definitions(
        dpwim-ring-buffer-generation-tests PRIVATE
        DPWIM_AUDIO_RING_BUFFER_TEST_HOOKS=1
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    add_test(
        NAME dpwim-ring-buffer-generation-tests
        COMMAND dpwim-ring-buffer-generation-tests
    )

    add_executable(dpwim-loopback-smoke
        Tests/loopback_smoke.cpp
    )
    target_link_libraries(dpwim-loopback-smoke PRIVATE dpwim_core winmm)

    add_executable(dpwim-plugin-probe
        Tests/plugin_probe.cpp
    )
    target_compile_definitions(dpwim-plugin-probe PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    if(DPWIM_VST2_SDK_PATH)
        target_compile_definitions(dpwim-plugin-probe PRIVATE
            JUCE_PLUGINHOST_VST=1
        )
        target_include_directories(dpwim-plugin-probe PRIVATE
            "${DPWIM_VST2_SDK_PATH}"
        )
    else()
        target_compile_definitions(dpwim-plugin-probe PRIVATE
            JUCE_PLUGINHOST_VST=0
        )
    endif()
    target_link_libraries(dpwim-plugin-probe PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
    )

    add_executable(dpwim-editor-snapshot
        Tests/editor_snapshot.cpp
    )
    target_include_directories(dpwim-editor-snapshot PRIVATE
        Source
        "${CMAKE_CURRENT_BINARY_DIR}/DPWIM_artefacts/JuceLibraryCode"
    )
    target_link_libraries(dpwim-editor-snapshot PRIVATE
        DPWIM
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
    )

    add_executable(dpwim-processor-state-tests
        Tests/processor_state.cpp
    )
    target_include_directories(dpwim-processor-state-tests PRIVATE
        Source
        "${CMAKE_CURRENT_BINARY_DIR}/DPWIM_artefacts/JuceLibraryCode"
    )
    target_link_libraries(dpwim-processor-state-tests PRIVATE
        DPWIM
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
    )
    add_test(
        NAME dpwim-processor-state-tests
        COMMAND dpwim-processor-state-tests
    )
endif()
