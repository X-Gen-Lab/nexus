cmake_minimum_required(VERSION 3.31)
project(nexus_f4_family_reference C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT IS_DIRECTORY "${NEXUS_SOURCE_ROOT}/io")
    message(FATAL_ERROR "Select a Nexus checkout or verified Source SDK")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "" FORCE)
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(nexus_f4_family_consumer
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../crc_assembly/consumer.c")
nexus_target_warnings(nexus_f4_family_consumer STRICT)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(family_config Nexus::Platform NEXUS_CONFIG_DIR)
# The original provider gate independently verifies these real linked methods.
add_custom_command(TARGET nexus_f4_family_consumer POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
        --elf "$<TARGET_FILE:nexus_f4_family_consumer>"
        --bindings "${family_config}/bindings.c"
        --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM)
