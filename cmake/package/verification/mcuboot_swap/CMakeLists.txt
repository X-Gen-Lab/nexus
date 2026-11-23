# Exact scratch software reference; layout owns linker and Flash region facts.
# Author: Nexus Team
cmake_minimum_required(VERSION 3.31)
project(NexusMCUbootScratchReference C ASM)
if(NOT DEFINED NEXUS_PLATFORM_ROOT)
    message(FATAL_ERROR "Explicit Nexus source/SDK root is required")
endif()
set(reference "${CMAKE_CURRENT_LIST_DIR}")
find_package(
    Nexus
    CONFIG
    REQUIRED
    PATHS "${NEXUS_PLATFORM_ROOT}/cmake"
    NO_DEFAULT_PATH
)
nexus_add_platform(ASSEMBLY "${reference}/assembly.toml")
nexus_add_mcuboot_swap_boot()
get_target_property(source Nexus::MCUBootSwapControl NEXUS_MCUBOOT_SOURCE)
file(READ "${reference}/layout.json" layout)
string(JSON count LENGTH "${layout}" regions)
math(EXPR last "${count} - 1")
set(regions "/* Static reference regions from the authored layout. */\n")
foreach(index RANGE 0 ${last})
    string(JSON id GET "${layout}" regions ${index} id)
    if(
        id STREQUAL "primary"
        OR id STREQUAL "secondary"
        OR id STREQUAL "scratch"
    )
        string(JSON origin GET "${layout}" regions ${index} origin)
        string(JSON size GET "${layout}" regions ${index} size)
        string(TOUPPER "${id}" role)
        string(APPEND regions "#define REFERENCE_${role}_ORIGIN ${origin}U\n")
        string(APPEND regions "#define REFERENCE_${role}_SIZE ${size}U\n")
    endif()
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/reference_regions.h" "${regions}")
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS "${reference}/layout.json"
)
nexus_add_firmware(mcuboot_swap_reference
    SOURCES "${reference}/main.c" "${reference}/../mcuboot_boot/public_key.c"
    LIBRARIES Nexus::MCUBootSwapBootStorage Nexus::MCUBootSwapPort
)
nexus_target_warnings(mcuboot_swap_reference STRICT)
target_include_directories(
    mcuboot_swap_reference
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
)
target_include_directories(
    mcuboot_swap_reference
    SYSTEM
    PRIVATE "${source}/boot/bootutil/include"
)
if(NEXUS_VERIFY_MCUBOOT_MIXED)
    nexus_add_mcuboot_control()
    target_link_libraries(mcuboot_swap_reference PRIVATE Nexus::MCUBootControl)
endif()
