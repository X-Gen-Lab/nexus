cmake_minimum_required(VERSION 3.31)
project(g474_pin_state_link_fixture LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "Pin-state fixture needs source root and one assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "No host tests in ARM fixture")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Explicit external fixture main")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(pin_state_fixture SOURCES main.c)
get_target_property(NEXUS_CONFIG_DIR Nexus::Platform NEXUS_CONFIG_DIR)
include("${NEXUS_SOURCE_ROOT}/cmake/platform/ControllerModes.cmake")
nexus_controller_modes(pin_modes gpio)
nexus_controller_modes(uart_modes uart)
if("named-states" IN_LIST pin_modes)
    target_compile_definitions(pin_state_fixture PRIVATE NX_PIN_STATE_FIXTURE)
elseif(uart_modes)
    target_compile_definitions(pin_state_fixture PRIVATE NX_G474_IO_FIXTURE)
endif()
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
if(pin_modes OR uart_modes)
    add_custom_command(TARGET pin_state_fixture POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" -B
            "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
            --elf "$<TARGET_FILE:pin_state_fixture>"
            --bindings "${NEXUS_CONFIG_DIR}/bindings.c"
            --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
        VERBATIM)
endif()
