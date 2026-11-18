cmake_minimum_required(VERSION 3.31)
project(ethernet_provider_link_fixture LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(
        FATAL_ERROR
        "Ethernet fixture needs source root and one explicit assembly"
    )
endif()
find_package(
    Nexus
    CONFIG
    REQUIRED
    PATHS "${NEXUS_SOURCE_ROOT}/cmake"
    NO_DEFAULT_PATH
)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_firmware(ethernet_controller_fixture SOURCES controller_main.c)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
add_custom_command(
    TARGET ethernet_controller_fixture
    POST_BUILD
    COMMAND
        "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py" --elf
        "$<TARGET_FILE:ethernet_controller_fixture>" --bindings
        "${fixture_config}/bindings.c" --output
        "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM
)
set_property(
    TARGET ethernet_controller_fixture
    APPEND
    PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/verify_memory.py"
)
add_custom_command(
    TARGET ethernet_controller_fixture
    POST_BUILD
    COMMAND
        "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/verify_memory.py" --elf
        "$<TARGET_FILE:ethernet_controller_fixture>" --resolved
        "${fixture_config}/resolved.json" --output
        "${CMAKE_CURRENT_BINARY_DIR}/bin/ethernet-memory.json"
    VERBATIM
)
