cmake_minimum_required(VERSION 3.31)
project(mainstream_assembly_link LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "Mainstream fixture requires explicit root and assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "No host test framework in firmware")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Explicit external public consumer")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(mainstream_assembly SOURCES main.c)
get_target_property(mainstream_config Nexus::Platform NEXUS_CONFIG_DIR)
get_target_property(mainstream_backend Nexus::Platform NEXUS_BACKEND)
include("${mainstream_config}/selection.cmake")
foreach(kind gpio uart spi watchdog exti adc pwm flash)
    if(kind IN_LIST NEXUS_SELECTED_KINDS)
        string(TOUPPER "${kind}" feature)
        target_compile_definitions(mainstream_assembly PRIVATE
            "NX_MAINSTREAM_${feature}")
    endif()
endforeach()
if(mainstream_backend STREQUAL "freertos")
    target_compile_definitions(mainstream_assembly PRIVATE NX_MAINSTREAM_FREERTOS)
endif()
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
set_property(TARGET mainstream_assembly APPEND PROPERTY LINK_DEPENDS
    "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py")
add_custom_command(TARGET mainstream_assembly POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
        --elf "$<TARGET_FILE:mainstream_assembly>"
        --bindings "${mainstream_config}/bindings.c"
        --resolved "${mainstream_config}/resolved.json"
        --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM)
