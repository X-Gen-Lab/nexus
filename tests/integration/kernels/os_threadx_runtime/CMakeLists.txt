# Genuine production platform linkage, without physical execution claims.
cmake_minimum_required(VERSION 3.31)
project(nexus_threadx_platform_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
get_filename_component(
    nexus_source
    "${CMAKE_CURRENT_LIST_DIR}/../../../.."
    ABSOLUTE
)
find_package(
    Nexus
    CONFIG
    REQUIRED
    PATHS "${nexus_source}/cmake"
    NO_DEFAULT_PATH
)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
set(sources main.c)
if(NEXUS_THREADX_CONTRACT_EXTERNAL_HOOK)
    list(APPEND sources external_tick.c)
endif()
nexus_add_firmware(nexus_threadx_runtime SOURCES ${sources}
    LIBRARIES Nexus::OSThreadX
)
