cmake_minimum_required(VERSION 3.31)
project(u575_assembly_link_fixture LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "U575 fixture needs source root and one explicit assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "No host tests in ARM fixture")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "External explicit fixture main")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(u575_assembly SOURCES main.c)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
include("${fixture_config}/selection.cmake")
if("uart" IN_LIST NEXUS_SELECTED_KINDS)
    target_compile_definitions(u575_assembly PRIVATE NX_U575_IO_FIXTURE)
endif()
if("watchdog" IN_LIST NEXUS_SELECTED_KINDS)
    target_compile_definitions(u575_assembly PRIVATE NX_U575_WATCHDOG_FIXTURE)
endif()
get_target_property(fixture_backend Nexus::Platform NEXUS_BACKEND)
if(fixture_backend STREQUAL "freertos")
    target_compile_definitions(u575_assembly PRIVATE NX_U575_FREERTOS_FIXTURE)
endif()
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
if(NEXUS_SELECTED_KINDS)
    add_custom_command(TARGET u575_assembly POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" -B
            "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
            --elf "$<TARGET_FILE:u575_assembly>"
            --bindings "${fixture_config}/bindings.c"
            --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
        VERBATIM)
endif()
