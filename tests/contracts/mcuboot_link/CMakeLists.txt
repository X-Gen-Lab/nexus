# CMakeLists.txt
# Separate normal boot/link reference using the official core source graph.
# Author: Nexus Team
cmake_minimum_required(VERSION 3.31)
project(NexusMCUbootReference C ASM)
if(NOT DEFINED NEXUS_PLATFORM_ROOT)
    message(FATAL_ERROR "Explicit Nexus and pinned MCUboot sources are required")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "")
add_subdirectory("${NEXUS_PLATFORM_ROOT}" nexus EXCLUDE_FROM_ALL)

if(NOT NEXUS_MCUBOOT_COMPONENT_ROOT)
    set(NEXUS_MCUBOOT_COMPONENT_ROOT
        "${NEXUS_PLATFORM_ROOT}/components/mcuboot")
endif()
set(port "${NEXUS_MCUBOOT_COMPONENT_ROOT}")
include("${port}/CMakeLists.txt")
nexus_add_mcuboot_boot()
get_target_property(MCUBOOT_SOURCE_ROOT Nexus::MCUBootControl NEXUS_MCUBOOT_SOURCE)

# Both linker ownership and C region data come from this one layout document.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/layout.json" layout)
string(JSON count LENGTH "${layout}" regions)
math(EXPR last "${count} - 1")
set(regions "/* Static product reference facts from layout.json. */\n")
foreach(index RANGE 0 ${last})
    string(JSON id GET "${layout}" regions ${index} id)
    if(id STREQUAL "primary" OR id STREQUAL "secondary")
        string(JSON origin GET "${layout}" regions ${index} origin)
        string(JSON size GET "${layout}" regions ${index} size)
        string(TOUPPER "${id}" role)
        string(APPEND regions "#define REFERENCE_${role}_ORIGIN ${origin}U\n")
        string(APPEND regions "#define REFERENCE_${role}_SIZE ${size}U\n")
    endif()
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/reference_regions.h" "${regions}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/layout.json")

nexus_add_firmware(mcuboot_reference SOURCES main.c public_key.c
    LIBRARIES Nexus::MCUBootBootStorage)
nexus_target_warnings(mcuboot_reference STRICT)
target_include_directories(mcuboot_reference PRIVATE
    "${port}/port" "${port}/profile" "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(mcuboot_reference SYSTEM PRIVATE
    "${MCUBOOT_SOURCE_ROOT}/boot/bootutil/include")

nexus_add_firmware(mcuboot_application_control SOURCES control_link.c
    LIBRARIES Nexus::MCUBootControl Nexus::MCUBootPort)
nexus_target_warnings(mcuboot_application_control STRICT)
target_include_directories(mcuboot_application_control PRIVATE
    "${port}/include" "${port}/port" "${port}/profile")
nexus_add_firmware(mcuboot_application_payload SOURCES payload.c)
nexus_target_warnings(mcuboot_application_payload STRICT)
