cmake_minimum_required(VERSION 3.31)
project(timer_waveform_provider_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "Timer fixture needs a source root and explicit assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "No host tests in ARM consumer")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Consumer owns its explicit main")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
file(READ "${fixture_config}/resolved.json" fixture_plan)
string(JSON fixture_kind GET "${fixture_plan}" controllers 0 kind)
string(JSON fixture_count LENGTH "${fixture_plan}" controllers)
set(fixture_main main.c)
if(fixture_count GREATER 1)
    set(fixture_main peer_main.c)
endif()
nexus_add_firmware(timer_waveform_contract SOURCES "${fixture_main}")
nexus_target_warnings(timer_waveform_contract STRICT)
if(fixture_kind STREQUAL "pwm_group")
    target_compile_definitions(timer_waveform_contract PRIVATE
        NEXUS_WAVEFORM_GROUP=1)
elseif(fixture_kind STREQUAL "timer_pulse")
    target_compile_definitions(timer_waveform_contract PRIVATE
        NEXUS_WAVEFORM_PULSE=1)
endif()
add_custom_command(TARGET timer_waveform_contract POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
        --elf "$<TARGET_FILE:timer_waveform_contract>"
        --bindings "${fixture_config}/bindings.c"
        --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM)
