cmake_minimum_required(VERSION 3.31)
project(adc_group_provider_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "ADC fixture needs a source root and explicit assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "No host tests in ARM consumer")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Consumer owns its explicit main")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(adc_group_contract SOURCES main.c)
nexus_target_warnings(adc_group_contract STRICT)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
add_custom_command(TARGET adc_group_contract POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
        --elf "$<TARGET_FILE:adc_group_contract>"
        --bindings "${fixture_config}/bindings.c"
        --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM)
