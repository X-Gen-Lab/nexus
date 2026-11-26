cmake_minimum_required(VERSION 3.31)
project(declared_io_link_fixture LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "Fixture needs explicit source root and one assembly")
endif()
find_package(
    Nexus
    CONFIG
    REQUIRED
    PATHS "${NEXUS_SOURCE_ROOT}/cmake"
    NO_DEFAULT_PATH
)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_firmware(declared_io_fixture SOURCES main.c)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
# Linked methods and their actual provider bytes are checked after section GC.
set_property(
    TARGET declared_io_fixture
    APPEND
    PROPERTY LINK_DEPENDS "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
)
add_custom_command(
    TARGET declared_io_fixture
    POST_BUILD
    COMMAND
        "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py" --elf
        "$<TARGET_FILE:declared_io_fixture>" --bindings
        "${fixture_config}/bindings.c" --resolved
        "${fixture_config}/resolved.json" --output
        "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM
)
