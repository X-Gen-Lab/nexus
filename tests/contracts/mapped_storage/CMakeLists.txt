# Link the optional common mechanism through each selected production platform.
cmake_minimum_required(VERSION 3.31)
project(nexus_mapped_storage_cost LANGUAGES C ASM)
get_filename_component(
    nexus_source
    "${CMAKE_CURRENT_LIST_DIR}/../../.."
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
nexus_add_firmware(mapped_storage_cost SOURCES probe.c
    LIBRARIES Nexus::MappedStorage
)
