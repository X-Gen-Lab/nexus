# CMakeLists.txt
# Optional two-family link/resource probe over an unconnected software endpoint.
# Author: Nexus Team
cmake_minimum_required(VERSION 3.31)
project(nexus_tmp117_link C ASM)
if(NOT DEFINED NEXUS_PLATFORM_ROOT)
    message(FATAL_ERROR "NEXUS_PLATFORM_ROOT must name the Nexus source root")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "")
add_subdirectory("${NEXUS_PLATFORM_ROOT}" nexus EXCLUDE_FROM_ALL)
nexus_add_firmware(tmp117_unselected SOURCES empty.c
    LIBRARIES Nexus::TMP117I2C)
nexus_target_warnings(tmp117_unselected STRICT)
nexus_add_firmware(tmp117_selected SOURCES selected.c
    LIBRARIES Nexus::TMP117I2C)
nexus_target_warnings(tmp117_selected STRICT)
# layout.c is separately compiled with the actual component compile command;
# its sizeof symbols never inflate either resource image.
