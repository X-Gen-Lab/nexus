# Link the optional common mechanism through each selected production platform.
cmake_minimum_required(VERSION 3.31)
project(nexus_mapped_storage_cost LANGUAGES C ASM)
get_filename_component(nexus_source "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "Build host tests")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Build workloads")
add_subdirectory("${nexus_source}" nexus EXCLUDE_FROM_ALL)
nexus_add_firmware(mapped_storage_cost SOURCES probe.c
    LIBRARIES Nexus::MappedStorage)
