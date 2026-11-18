cmake_minimum_required(VERSION 3.31)
project(usb_otg_link_fixture LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(
        FATAL_ERROR
        "OTG fixture needs source root and one explicit assembly"
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
nexus_add_firmware(usb_otg_fixture SOURCES otg_main.c)
nexus_target_warnings(usb_otg_fixture STRICT)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
add_custom_command(
    TARGET usb_otg_fixture
    POST_BUILD
    COMMAND
        "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py" --elf
        "$<TARGET_FILE:usb_otg_fixture>" --bindings
        "${fixture_config}/bindings.c" --output
        "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM
)
