cmake_minimum_required(VERSION 3.31)
project(f407_jpeg_blocks_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "JPEG fixture needs an explicit source and TOML")
endif()
find_package(
    Nexus
    CONFIG
    REQUIRED
    PATHS "${NEXUS_SOURCE_ROOT}/cmake"
    NO_DEFAULT_PATH
)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_firmware(jpeg_blocks_contract SOURCES main.c)
nexus_target_warnings(jpeg_blocks_contract STRICT)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
add_custom_command(
    TARGET jpeg_blocks_contract
    POST_BUILD
    COMMAND
        "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py" --elf
        "$<TARGET_FILE:jpeg_blocks_contract>" --bindings
        "${fixture_config}/bindings.c" --output
        "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM
)
