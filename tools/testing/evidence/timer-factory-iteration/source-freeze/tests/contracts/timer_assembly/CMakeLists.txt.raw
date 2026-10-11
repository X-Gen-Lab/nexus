cmake_minimum_required(VERSION 3.31)
project(timer_provider_link_fixture LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "Timer fixture requires source root and explicit assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "No host tests in ARM fixture")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Explicit external timer caller")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(timer_controller_fixture SOURCES controller_main.c)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
file(READ "${fixture_config}/resolved.json" fixture_resolved)
string(JSON fixture_controller_count LENGTH "${fixture_resolved}" controllers)
string(JSON fixture_device_count LENGTH "${fixture_resolved}" devices)
# An empty authored selection is measured by the matrix GC observer.
if(fixture_controller_count GREATER 0 OR fixture_device_count GREATER 0)
    add_custom_command(TARGET timer_controller_fixture POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" -B
            "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
            --elf "$<TARGET_FILE:timer_controller_fixture>"
            --bindings "${fixture_config}/bindings.c"
            --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
        VERBATIM)
endif()
