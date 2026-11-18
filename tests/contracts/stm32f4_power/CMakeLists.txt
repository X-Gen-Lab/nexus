cmake_minimum_required(VERSION 3.31)
project(nexus_f4_power_public_consumer C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(
        FATAL_ERROR
        "Power reference requires a source root and exact assembly"
    )
endif()
find_package(
    Nexus
    CONFIG
    REQUIRED
    PATHS "${NEXUS_SOURCE_ROOT}/cmake"
    NO_DEFAULT_PATH
)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_stm32f4_sleep(selected_power)
nexus_add_firmware(nexus_f4_power_consumer SOURCES consumer.c
    LIBRARIES selected_power
)
nexus_target_warnings(nexus_f4_power_consumer STRICT)
