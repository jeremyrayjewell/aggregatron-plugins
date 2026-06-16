cmake_minimum_required(VERSION 3.21)

project(AggregaKeysDPF LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DPF_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../external/DPF"
    CACHE PATH
    "Path to a DPF checkout")

set(JUCE_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../JUCE"
    CACHE PATH
    "Path to a JUCE checkout")

if(NOT EXISTS "${DPF_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "DPF checkout not found at '${DPF_SOURCE_DIR}'. Initialize external/DPF or set DPF_SOURCE_DIR.")
endif()

if(NOT EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE checkout not found at '${JUCE_SOURCE_DIR}'. Set JUCE_SOURCE_DIR to a JUCE checkout.")
endif()

set(DPF_EXAMPLES OFF CACHE BOOL "Build DPF example targets" FORCE)

add_subdirectory("${DPF_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/external-dpf")
add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/external-juce")

dpf_add_plugin(AggregaKeysVST2Probe
    TARGETS vst2
    FILES_DSP
        probe/AggregaKeysVST2Probe.cpp
)

target_include_directories(AggregaKeysVST2Probe-dsp
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/probe"
)

target_include_directories(AggregaKeysVST2Probe
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/probe"
)

target_include_directories(AggregaKeysVST2Probe-vst2
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/probe"
)

dpf_add_plugin(AggregaKeysDPFVST2
    TARGETS vst2
    FILES_DSP
        aggregakeys/AggregaKeysVST2.cpp
        ../src/AggregaKeysEngine.cpp
)

target_include_directories(AggregaKeysDPFVST2-dsp
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/aggregakeys"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)

target_include_directories(AggregaKeysDPFVST2
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/aggregakeys"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)

target_include_directories(AggregaKeysDPFVST2-vst2
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/aggregakeys"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)

target_link_libraries(AggregaKeysDPFVST2-dsp
    PRIVATE
        juce::juce_audio_basics
        juce::juce_dsp
)
