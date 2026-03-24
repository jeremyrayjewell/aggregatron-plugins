cmake_minimum_required(VERSION 3.22)

project(AggregatronPlugins VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(JUCE_DIR "" CACHE PATH "Path to the JUCE source tree")

if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    message(FATAL_ERROR
        "JUCE not found. Set JUCE_DIR to your JUCE checkout, or clone JUCE into ${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
endif()

juce_add_binary_data(AggregatronAssets
    SOURCES
        bg.png
        knob.png
        slider.png
        font.ttf
)

juce_add_plugin(AggregatronKeys
    COMPANY_NAME "Aggregatron"
    BUNDLE_ID "com.aggregatron.aggregakeys.v0_2"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Aggr
    PLUGIN_CODE Aky2
    FORMATS Standalone VST3
    PRODUCT_NAME "AggregaKeys v0.2"
)

juce_generate_juce_header(AggregatronKeys)

target_sources(AggregatronKeys
    PRIVATE
        src/PluginEditor.cpp
        src/PluginEditor.h
        src/PluginProcessor.cpp
        src/PluginProcessor.h
)

target_compile_definitions(AggregatronKeys
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AggregatronKeys
    PRIVATE
        AggregatronAssets
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(AggregaMap
    COMPANY_NAME "Aggregatron"
    BUNDLE_ID "com.aggregatron.aggregamap.v0_1"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Aggr
    PLUGIN_CODE Agm1
    FORMATS Standalone VST3
    PRODUCT_NAME "AggregaMap v0.1"
)

juce_generate_juce_header(AggregaMap)

target_sources(AggregaMap
    PRIVATE
        samplemap/SampleMapEditor.cpp
        samplemap/SampleMapEditor.h
        samplemap/SampleMapProcessor.cpp
        samplemap/SampleMapProcessor.h
)

target_compile_definitions(AggregaMap
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AggregaMap
    PRIVATE
        AggregatronAssets
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
