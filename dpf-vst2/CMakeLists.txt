cmake_minimum_required(VERSION 3.21)

project(AggregaKeysVST2Probe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DPF_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../external/DPF"
    CACHE PATH
    "Path to a DPF checkout")

if(NOT EXISTS "${DPF_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "DPF checkout not found at '${DPF_SOURCE_DIR}'. Initialize external/DPF or set DPF_SOURCE_DIR.")
endif()

set(DPF_EXAMPLES OFF CACHE BOOL "Build DPF example targets" FORCE)

add_subdirectory("${DPF_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/external-dpf")

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
