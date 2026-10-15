cmake_minimum_required(VERSION 3.31)
project(can_provider_link_fixture LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "CAN fixture needs source root and one assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "No host tests in ARM fixture")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "External explicit fixture main")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(can_controller_fixture SOURCES controller_main.c)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
add_custom_command(TARGET can_controller_fixture POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
        --elf "$<TARGET_FILE:can_controller_fixture>"
        --bindings "${fixture_config}/bindings.c"
        --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM)
