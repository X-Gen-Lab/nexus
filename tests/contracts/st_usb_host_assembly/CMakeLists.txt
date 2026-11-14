cmake_minimum_required(VERSION 3.31)
project(st_usb_host_link_fixture LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "Host middleware fixture requires source and assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "ARM link fixture")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Explicit private integration")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(usb_host_stack_fixture SOURCES host_stack_main.c)
target_include_directories(usb_host_stack_fixture PRIVATE
    "${NEXUS_SOURCE_ROOT}/components/st_usb/private"
    "${NEXUS_SOURCE_ROOT}/soc/stm32/stm32f4/private")
target_include_directories(usb_host_stack_fixture SYSTEM PRIVATE
    "${NEXUS_SOURCE_ROOT}/vendors/st/usb_host/Core/Inc"
    "${NEXUS_SOURCE_ROOT}/vendors/st/cmsis_device_f4/Include")
include("${NEXUS_SOURCE_ROOT}/soc/stm32/stm32f4/cmake/STM32Official.cmake")
nexus_stm32f4_official(usb_host_stack_fixture)
nexus_use_cmsis_core(usb_host_stack_fixture)
target_compile_definitions(usb_host_stack_fixture PRIVATE STM32F407xx)
nexus_target_warnings(usb_host_stack_fixture STRICT)
