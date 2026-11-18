cmake_minimum_required(VERSION 3.31)
project(nexus_spi_endpoint_cost LANGUAGES C ASM)
if(NOT NEXUS_SOURCE_ROOT OR NOT COST_ENDPOINTS MATCHES "^[0124]$")
    message(
        FATAL_ERROR
        "Select an explicit source tree and 0, 1, 2 or 4 endpoints"
    )
endif()
set(NEXUS_ASSEMBLY_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/endpoints-${COST_ENDPOINTS}.toml"
    CACHE FILEPATH
    "Exact software fixture configuration"
    FORCE
)
find_package(
    Nexus
    CONFIG
    REQUIRED
    PATHS "${NEXUS_SOURCE_ROOT}/cmake"
    NO_DEFAULT_PATH
)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_firmware(endpoint_cost SOURCES main.c)
target_compile_definitions(
    endpoint_cost
    PRIVATE COST_ENDPOINTS=${COST_ENDPOINTS}
)
nexus_target_warnings(endpoint_cost STRICT)
