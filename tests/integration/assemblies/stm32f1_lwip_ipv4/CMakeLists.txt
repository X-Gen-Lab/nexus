cmake_minimum_required(VERSION 3.31)
project(nexus_f1_lwip_ipv4_fixture LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(NEXUS_ASSEMBLY_FILE
    "${CMAKE_CURRENT_LIST_DIR}/f107vc.toml"
    CACHE FILEPATH
    "F107 MB784 IPv4 MII software assembly"
)

if(NEXUS_SOURCE_ROOT)
    set(Nexus_DIR "${NEXUS_SOURCE_ROOT}/cmake")
endif()
find_package(Nexus CONFIG REQUIRED)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
get_target_property(components Nexus::Config NEXUS_SELECTED_COMPONENTS)
if("network" IN_LIST components)
    set(network_storage Nexus::LwIPStorage Nexus::LwIPTCPStorage)
else()
    set(network_storage Nexus::LwIPIPv4Storage)
endif()
nexus_add_firmware(
    nexus_f1_lwip_ipv4_consumer
    SOURCES main.c raw_frames.c
    LIBRARIES ${network_storage} Nexus::DP83848
)
nexus_target_warnings(nexus_f1_lwip_ipv4_consumer STRICT)

find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_source Nexus::Config NEXUS_SOURCE_DIR)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
set(retention_check "${fixture_source}/tools/measurement/retention.py")
set(retention_report "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json")
set_property(
    TARGET nexus_f1_lwip_ipv4_consumer
    APPEND
    PROPERTY LINK_DEPENDS "${retention_check}"
)
add_custom_command(
    TARGET nexus_f1_lwip_ipv4_consumer
    POST_BUILD
    BYPRODUCTS "${retention_report}"
    COMMAND
        "${Python3_EXECUTABLE}" -B "${retention_check}" --elf
        "$<TARGET_FILE:nexus_f1_lwip_ipv4_consumer>" --bindings
        "${fixture_config}/bindings.c" --resolved
        "${fixture_config}/resolved.json" --output "${retention_report}"
    VERBATIM
)
