# Explicit public-contract ARM link reference; no physical qualification.
cmake_minimum_required(VERSION 3.31)
project(f407_qiming_network_reference LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_REFERENCE_ROOT)
    get_filename_component(NEXUS_REFERENCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.."
        ABSOLUTE)
endif()
set(NEXUS_ASSEMBLY_FILE "${CMAKE_CURRENT_LIST_DIR}/assembly.toml")
set(NEXUS_BUILD_TESTS OFF)
set(NEXUS_BUILD_WORKLOAD OFF)
add_subdirectory("${NEXUS_REFERENCE_ROOT}" nexus)
nexus_add_tls()
nexus_add_coremqtt()
nexus_add_stm32f407_entropy(reference_entropy)
set(public_services
    "${NEXUS_REFERENCE_ROOT}/cmake/package/verification/secure_services")
add_library(reference_application STATIC "${public_services}/f407/application.c")
target_include_directories(reference_application PUBLIC "${public_services}/f407")
target_link_libraries(reference_application PRIVATE
    Nexus::TLS Nexus::CoreMQTT nexus_build_options)
nexus_apply_target_policy(reference_application)
nexus_target_warnings(reference_application STRICT)
nexus_add_firmware(f407_qiming_network_reference SOURCES main.c
    LIBRARIES reference_application reference_entropy Nexus::DP83848
        Nexus::TLSStorage Nexus::CoreMQTTStorage Nexus::LwIPStorage
        Nexus::LwIPTCPStorage)
target_include_directories(f407_qiming_network_reference PRIVATE "${public_services}")
nexus_target_warnings(f407_qiming_network_reference STRICT)
