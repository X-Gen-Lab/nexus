cmake_minimum_required(VERSION 3.31)
project(qiming_sdio_public_consumer LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "SDIO fixture needs explicit source root and assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "No host test framework in firmware")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Explicit read-only SDIO consumer")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(qiming_sdio_controller_fixture SOURCES controller_main.c)
nexus_target_warnings(qiming_sdio_controller_fixture STRICT)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
set_property(TARGET qiming_sdio_controller_fixture APPEND PROPERTY LINK_DEPENDS
    "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py")
add_custom_command(TARGET qiming_sdio_controller_fixture POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
        --elf "$<TARGET_FILE:qiming_sdio_controller_fixture>"
        --bindings "${fixture_config}/bindings.c"
        --resolved "${fixture_config}/resolved.json"
        --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM)
