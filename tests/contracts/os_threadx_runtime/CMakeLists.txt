# Genuine production platform linkage, without physical execution claims.
cmake_minimum_required(VERSION 3.31)
project(nexus_threadx_platform_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
get_filename_component(nexus_source "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "No firmware host tests" FORCE)
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Explicit contract image only" FORCE)
add_subdirectory("${nexus_source}" nexus EXCLUDE_FROM_ALL)
set(sources main.c)
if(NEXUS_THREADX_CONTRACT_EXTERNAL_HOOK)
    list(APPEND sources external_tick.c)
endif()
nexus_add_firmware(nexus_threadx_runtime SOURCES ${sources}
    LIBRARIES Nexus::OSThreadX)
