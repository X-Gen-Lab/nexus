cmake_minimum_required(VERSION 3.31)
project(legacy_transport_assembly LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "Legacy transport requires explicit source and assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "Host framework stays outside firmware")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Explicit external common consumer")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(legacy_transport SOURCES main.c)
get_target_property(legacy_config Nexus::Platform NEXUS_CONFIG_DIR)
get_target_property(legacy_backend Nexus::Platform NEXUS_BACKEND)
include("${legacy_config}/selection.cmake")
foreach(kind uart i2c)
    if(kind IN_LIST NEXUS_SELECTED_KINDS)
        string(TOUPPER "${kind}" feature)
        target_compile_definitions(legacy_transport PRIVATE "NX_LEGACY_${feature}")
    endif()
endforeach()
file(READ "${legacy_config}/resolved.json" legacy_resolved)
string(JSON legacy_mode GET "${legacy_resolved}" controllers 0 mode)
if(legacy_mode STREQUAL "dma-duplex")
    target_compile_definitions(legacy_transport PRIVATE NX_LEGACY_BLOCKS)
endif()
if(legacy_backend STREQUAL "freertos")
    target_compile_definitions(legacy_transport PRIVATE NX_LEGACY_FREERTOS)
endif()
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
add_custom_command(TARGET legacy_transport POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
        --elf "$<TARGET_FILE:legacy_transport>"
        --bindings "${legacy_config}/bindings.c"
        --resolved "${legacy_config}/resolved.json"
        --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM)
