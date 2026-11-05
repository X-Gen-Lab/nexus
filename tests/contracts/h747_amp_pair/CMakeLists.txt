# Independent external-boot link fixtures, not a production SoC/startup port.
cmake_minimum_required(VERSION 3.31)
project(h747_external_boot_reference LANGUAGES C ASM)
if(NOT NEXUS_REFERENCE_CORE MATCHES "^CM[47]$" OR NOT NEXUS_SOURCE_ROOT)
    message(FATAL_ERROR "Select exact external reference core CM7 or CM4")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(NEXUS_SOURCE_DIR "${NEXUS_SOURCE_ROOT}")
set(NEXUS_SOC_FAMILY stm32h747)
set(NEXUS_BACKEND baremetal)
if(NEXUS_REFERENCE_CORE STREQUAL "CM7")
    set(NEXUS_CPU_ARCH cortex-m7)
    set(reference_fpu fpv5-d16)
    set(NEXUS_ARCH_DCACHE_LINE_BYTES 32)
    set(NEXUS_ARCH_ICACHE_LINE_BYTES 32)
    set(reference_flash 0x08000000)
    set(reference_ram 0x24000000)
else()
    set(NEXUS_CPU_ARCH cortex-m4)
    set(reference_fpu fpv4-sp-d16)
    set(NEXUS_ARCH_DCACHE_LINE_BYTES 0)
    set(NEXUS_ARCH_ICACHE_LINE_BYTES 0)
    set(reference_flash 0x08100000)
    set(reference_ram 0x30000000)
endif()
set(NEXUS_ARCH_HAS_DWT_CYCCNT 1)
set(NEXUS_ARCH_MPU_VERSION 7)
set(NEXUS_ARCH_SECURITY_STATE 0)
set(NEXUS_ARCH_HAS_SAU 0)
add_library(nexus_config INTERFACE)
add_library(Nexus::Config ALIAS nexus_config)
set_property(TARGET nexus_config PROPERTY NEXUS_LTO_REQUIRED FALSE)
include("${NEXUS_SOURCE_ROOT}/cmake/platform/TargetPolicy.cmake")
include("${NEXUS_SOURCE_ROOT}/cmake/platform/CMSISCore.cmake")
add_library(nexus_abi_options INTERFACE)
target_compile_options(nexus_abi_options INTERFACE
    -mcpu=${NEXUS_CPU_ARCH} -mthumb -mfpu=${reference_fpu} -mfloat-abi=hard
    -fshort-enums)
target_compile_definitions(nexus_abi_options INTERFACE
    STM32H747xx CORE_${NEXUS_REFERENCE_CORE})
add_library(nexus_build_options INTERFACE)
target_link_libraries(nexus_build_options INTERFACE nexus_abi_options)
target_compile_options(nexus_build_options INTERFACE
    -Os -ffunction-sections -fdata-sections)
add_library(nexus_owned_warnings INTERFACE)
target_compile_options(nexus_owned_warnings INTERFACE
    -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
    -Wstrict-prototypes -Wmissing-prototypes)
add_subdirectory("${NEXUS_SOURCE_ROOT}/core" core)
add_subdirectory("${NEXUS_SOURCE_ROOT}/arch" arch)
add_subdirectory("${NEXUS_SOURCE_ROOT}/components/amp_ipc" amp)
add_subdirectory(
    "${NEXUS_SOURCE_ROOT}/components/soc/stm32/stm32h747_amp" h747)
configure_file(memory.ld.in memory.ld @ONLY)
add_executable(reference main.c external_boot.c
    "${NEXUS_SOURCE_ROOT}/vendors/st/cmsis_device_h7/Source/Templates/gcc/startup_stm32h747xx.s")
target_link_libraries(reference PRIVATE Nexus::H747AMP Nexus::Arch
    nexus_owned_warnings)
target_link_options(reference PRIVATE
    -mcpu=${NEXUS_CPU_ARCH} -mthumb -mfpu=${reference_fpu} -mfloat-abi=hard
    -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_BINARY_DIR}/memory.ld"
    "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/reference.map"
    -Wl,--no-gc-sections -Wl,--fatal-warnings)
set_target_properties(reference PROPERTIES SUFFIX ".elf")
