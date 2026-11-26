cmake_minimum_required(VERSION 3.31)
project(f407_freertos_stop_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "STOP fixture needs source root and assembly")
endif()
find_package(
    Nexus
    CONFIG
    REQUIRED
    PATHS "${NEXUS_SOURCE_ROOT}/cmake"
    NO_DEFAULT_PATH
)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_stm32f4_freertos_stop(stop_adapter)
nexus_add_firmware(f407_freertos_stop_contract SOURCES main.c LIBRARIES stop_adapter)
nexus_target_warnings(f407_freertos_stop_contract STRICT)
