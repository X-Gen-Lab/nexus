cmake_minimum_required(VERSION 3.31)
project(rtc_bypass_public_consumer LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "RTC fixture requires source root and assembly")
endif()
find_package(
    Nexus
    CONFIG
    REQUIRED
    PATHS "${NEXUS_SOURCE_ROOT}/cmake"
    NO_DEFAULT_PATH
)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_firmware(rtc_bypass_fixture SOURCES main.c)
nexus_target_warnings(rtc_bypass_fixture STRICT)
