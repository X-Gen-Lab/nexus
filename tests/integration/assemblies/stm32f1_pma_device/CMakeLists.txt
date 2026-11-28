cmake_minimum_required(VERSION 3.31)
project(nexus_f1_pma_usb_fixture LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(NEXUS_ASSEMBLY_FILE
    "${CMAKE_CURRENT_LIST_DIR}/f103rb.toml"
    CACHE FILEPATH
    "F103RB EP0 PMA device fixture assembly"
)

if(NEXUS_SOURCE_ROOT)
    set(Nexus_DIR "${NEXUS_SOURCE_ROOT}/cmake")
endif()
find_package(Nexus CONFIG REQUIRED)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
get_target_property(fixture_source Nexus::Config NEXUS_SOURCE_DIR)
if(TARGET Nexus::STUSBDevice)
    nexus_add_firmware(nexus_f1_pma_usb SOURCES st_main.c st_bulk.c)
    target_include_directories(
        nexus_f1_pma_usb
        PRIVATE "${fixture_source}/components/protocols/st_usb_device/private"
    )
    target_include_directories(
        nexus_f1_pma_usb
        SYSTEM
        PRIVATE "${fixture_source}/vendors/st/usb_device/Core/Inc"
    )
    target_compile_definitions(
        nexus_st_usb_device_core
        PRIVATE USBD_SELF_POWERED=1U
    )
else()
    nexus_add_firmware(nexus_f1_pma_usb SOURCES main.c)
endif()
nexus_target_warnings(nexus_f1_pma_usb STRICT)

find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
set(retention_check "${fixture_source}/tools/measurement/retention.py")
set(retention_report "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json")
set_property(
    TARGET nexus_f1_pma_usb
    APPEND
    PROPERTY LINK_DEPENDS "${retention_check}"
)
add_custom_command(
    TARGET nexus_f1_pma_usb
    POST_BUILD
    BYPRODUCTS "${retention_report}"
    COMMAND
        "${Python3_EXECUTABLE}" -B "${retention_check}" --elf
        "$<TARGET_FILE:nexus_f1_pma_usb>" --bindings
        "${fixture_config}/bindings.c" --resolved
        "${fixture_config}/resolved.json" --output "${retention_report}"
    VERBATIM
)
