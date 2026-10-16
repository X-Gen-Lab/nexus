cmake_minimum_required(VERSION 3.31)
project(stm32f103_assembly_link LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "F103 fixture requires explicit root and assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "Firmware never includes GoogleTest")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Explicit link fixture main")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(stm32f103_assembly SOURCES main.c)
get_target_property(f1_backend Nexus::Platform NEXUS_BACKEND)
if(f1_backend STREQUAL "freertos")
    target_compile_definitions(stm32f103_assembly PRIVATE NX_F103_FREERTOS_FIXTURE)
endif()
get_target_property(f1_config Nexus::Platform NEXUS_CONFIG_DIR)
include("${f1_config}/selection.cmake")
if("uart" IN_LIST NEXUS_SELECTED_KINDS)
    target_compile_definitions(stm32f103_assembly PRIVATE NX_F103_IO_FIXTURE)
endif()
if("spi" IN_LIST NEXUS_SELECTED_KINDS)
    target_compile_definitions(stm32f103_assembly PRIVATE NX_F103_SPI_FIXTURE)
endif()
if("watchdog" IN_LIST NEXUS_SELECTED_KINDS)
    target_compile_definitions(stm32f103_assembly PRIVATE NX_F103_WATCHDOG_FIXTURE)
endif()
if("exti" IN_LIST NEXUS_SELECTED_KINDS)
    target_compile_definitions(stm32f103_assembly PRIVATE NX_F103_EXTI_FIXTURE)
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_target_property(f1_config Nexus::Platform NEXUS_CONFIG_DIR)
add_custom_command(TARGET stm32f103_assembly POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
        --elf "$<TARGET_FILE:stm32f103_assembly>"
        --bindings "${f1_config}/bindings.c"
        --resolved "${f1_config}/resolved.json"
        --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM)
