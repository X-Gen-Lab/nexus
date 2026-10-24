cmake_minimum_required(VERSION 3.31)
project(nexus_spi_endpoint_cost LANGUAGES C ASM)
if(NOT NEXUS_SOURCE_ROOT OR NOT COST_ENDPOINTS MATCHES "^[12]$")
    message(FATAL_ERROR "Select an explicit source tree and one/two endpoints")
endif()
set(NEXUS_ASSEMBLY_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/endpoints-${COST_ENDPOINTS}.toml"
    CACHE FILEPATH "Exact software fixture configuration" FORCE)
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "Firmware allocation fixture" FORCE)
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Explicit fixture consumer" FORCE)
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk EXCLUDE_FROM_ALL)
nexus_add_firmware(endpoint_cost SOURCES main.c)
target_compile_definitions(endpoint_cost PRIVATE COST_ENDPOINTS=${COST_ENDPOINTS})
nexus_target_warnings(endpoint_cost STRICT)
