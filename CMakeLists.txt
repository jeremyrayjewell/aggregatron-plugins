cmake_minimum_required(VERSION 3.22)

project(AggregatronPlugins VERSION 0.3.0)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/bg.png
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/knob.png
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/slider.png
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/font.ttf
)

juce_add_plugin(AggregatronKeys
    COMPANY_NAME "Aggregatron"
    BUNDLE_ID "com.aggregatron.aggregakeys.v0_2"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Aggr
    PLUGIN_CODE Aky2
    FORMATS Standalone VST3
    PRODUCT_NAME "AggregaKeys v0.3"
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

add_executable(AggregaMapDebug
    samplemap/MapAnalysis.cpp
    samplemap/MapAnalysis.h
    samplemap/MapDebug.cpp
)

target_compile_definitions(AggregaMapDebug
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(AggregaMapDebug
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_core
        juce::juce_events
)

juce_add_plugin(AggregaScale
    COMPANY_NAME "Aggregatron"
    BUNDLE_ID "com.aggregatron.aggregascale.v0_1"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Aggr
    PLUGIN_CODE Ags1
    FORMATS Standalone VST3
    PRODUCT_NAME "AggregaScale v0.1"
)

juce_generate_juce_header(AggregaScale)

target_sources(AggregaScale
    PRIVATE
        scalescope/ScaleScopeEditor.cpp
        scalescope/ScaleScopeEditor.h
        scalescope/ScaleScopeProcessor.cpp
        scalescope/ScaleScopeProcessor.h
)

target_compile_definitions(AggregaScale
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AggregaScale
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

juce_add_gui_app(AggregaScaleApp
    COMPANY_NAME "Aggregatron"
    BUNDLE_ID "com.aggregatron.aggregascaleapp.v0_1"
    PRODUCT_NAME "AggregaScale"
)

juce_generate_juce_header(AggregaScaleApp)

target_sources(AggregaScaleApp
    PRIVATE
        scalescope/ScaleScopeApp.cpp
        scalescope/ScaleScopeMainComponent.cpp
        scalescope/ScaleScopeMainComponent.h
        scalescope/ScaleScopeState.cpp
        scalescope/ScaleScopeState.h
)

target_compile_definitions(AggregaScaleApp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(AggregaScaleApp
    PRIVATE
        AggregatronAssets
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
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

juce_add_gui_app(April10App
    COMPANY_NAME "Aggregatron"
    BUNDLE_ID "com.aggregatron.april10.v0_1"
    PRODUCT_NAME "April 10"
)

juce_generate_juce_header(April10App)

target_sources(April10App
    PRIVATE
        scalescope/April10App.cpp
        scalescope/April10MainComponent.cpp
        scalescope/April10MainComponent.h
        scalescope/ScaleScopeState.cpp
        scalescope/ScaleScopeState.h
)

target_compile_definitions(April10App
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_compile_options(April10App
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/FS>
)

target_link_libraries(April10App
    PRIVATE
        AggregatronAssets
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
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

juce_add_plugin(AggregaVocoder
    COMPANY_NAME "Aggregatron"
    BUNDLE_ID "com.aggregatron.aggregavocoder.v0_2"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Aggr
    PLUGIN_CODE Agv1
    FORMATS Standalone VST3
    PRODUCT_NAME "AggregaVocoder v0.2"
)

juce_generate_juce_header(AggregaVocoder)

target_sources(AggregaVocoder
    PRIVATE
        vocoder/VocoderEditor.cpp
        vocoder/VocoderEditor.h
        vocoder/VocoderProcessor.cpp
        vocoder/VocoderProcessor.h
)

target_compile_definitions(AggregaVocoder
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AggregaVocoder
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

juce_add_plugin(SimpleVocoder
    COMPANY_NAME "Aggregatron"
    BUNDLE_ID "com.aggregatron.simplevocoder.v0_1"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Aggr
    PLUGIN_CODE Svc1
    FORMATS Standalone VST3
    PRODUCT_NAME "SimpleVocoder v0.1"
)

juce_generate_juce_header(SimpleVocoder)

target_sources(SimpleVocoder
    PRIVATE
        simplevocoder/SimpleVocoderEditor.cpp
        simplevocoder/SimpleVocoderEditor.h
        simplevocoder/SimpleVocoderProcessor.cpp
        simplevocoder/SimpleVocoderProcessor.h
)

target_compile_definitions(SimpleVocoder
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SimpleVocoder
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
