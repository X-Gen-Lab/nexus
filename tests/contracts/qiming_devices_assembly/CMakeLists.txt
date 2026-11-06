cmake_minimum_required(VERSION 3.31)
project(qiming_devices_public_consumer LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "Qiming fixture requires source root and one assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "No host framework in ARM fixture")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Explicit link-contract consumer")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(qiming_devices_controller_fixture SOURCES controller_main.c
    LIBRARIES Nexus::SoftwareSPI Nexus::XPT2046 Nexus::SoftwareI2C
        Nexus::AT24C02 Nexus::W25 Nexus::ILI9341)
nexus_target_warnings(qiming_devices_controller_fixture STRICT)
# Verification-only roots retain the documented APIs even in SRAM-only mode.
# This is an upper-bound API linkage fixture, not an application size claim.
foreach(symbol
        nx_software_spi_init nx_software_spi_endpoint nx_software_spi_port
        nx_software_spi_faulted nx_xpt2046_init nx_xpt2046_sample nx_xpt2046_stop
        nx_software_i2c_init nx_software_i2c_endpoint nx_software_i2c_port
        nx_software_i2c_recover nx_software_i2c_stop nx_software_i2c_faulted
        nx_software_i2c_cleanup_result nx_software_sccb_init
        nx_software_sccb_write_register nx_software_sccb_read_register
        nx_at24c02_init nx_at24c02_read nx_at24c02_write nx_at24c02_sync
        nx_at24c02_stop nx_w25_init nx_w25_read nx_w25_program_start
        nx_w25_erase_4k_start nx_w25_service nx_w25_get_status nx_w25_stop
        nx_ili9341_initialize nx_ili9341_service nx_ili9341_write nx_ili9341_stop)
    target_link_options(qiming_devices_controller_fixture PRIVATE
        "LINKER:--undefined=${symbol}")
endforeach()
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
set_property(TARGET qiming_devices_controller_fixture APPEND PROPERTY LINK_DEPENDS
    "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py")
add_custom_command(TARGET qiming_devices_controller_fixture POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
        --elf "$<TARGET_FILE:qiming_devices_controller_fixture>"
        --bindings "${fixture_config}/bindings.c"
        --resolved "${fixture_config}/resolved.json"
        --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM)
