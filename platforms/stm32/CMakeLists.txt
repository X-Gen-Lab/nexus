#-----------------------------------------------------------------------------
# STM32 Platform CMake Configuration
#-----------------------------------------------------------------------------
# This file configures the STM32 platform build system with support for:
#   - All STM32 series (C0, F0-F7, G0/G4, H5/H7, L0-L5, U0/U3/U5, WB/WL)
#   - Multiple toolchains (ARM GCC, ARM Clang, IAR)
#   - Kconfig-driven configuration
#   - Automatic HAL module selection
#   - Vendor CMSIS and HAL driver integration
#   - Modular source organization (boot, clock, interrupt, system, peripheral)
#
# Directory structure:
#   src/boot/       - Boot and initialization code
#   src/clock/      - Clock configuration code
#   src/interrupt/  - Interrupt handlers
#   src/system/     - System-level code (error handling, etc.)
#   src/peripheral/ - Peripheral drivers
#   src/platform/   - Platform common code
#   include/        - Header files organized by module
#
# Author: Nexus Team
# Version: 2.0.0
#-----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.15)

message(STATUS "=== STM32 Platform Configuration ===")

#-----------------------------------------------------------------------------
# Toolchain Detection
#-----------------------------------------------------------------------------

# Include toolchain abstraction layer (auto-initializes)
include(NexusToolchain)

# Set vendor-specific directory names based on detected toolchain
if(NEXUS_TOOLCHAIN_IS_GCC)
    set(VENDOR_TOOLCHAIN_DIR "gcc")
elseif(NEXUS_TOOLCHAIN_IS_CLANG)
    set(VENDOR_TOOLCHAIN_DIR "arm")
elseif(NEXUS_TOOLCHAIN_IS_IAR)
    set(VENDOR_TOOLCHAIN_DIR "iar")
else()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_C_COMPILER_ID}")
endif()

#-----------------------------------------------------------------------------
# STM32 Series Detection from Kconfig
#-----------------------------------------------------------------------------

# Detect STM32 series and set variables
if(CONFIG_STM32C0)
    set(STM32_SERIES "stm32c0")
    set(STM32_SERIES_SHORT "c0")
    set(STM32_FAMILY "STM32C0xx")
    set(STM32_SERIES_UPPER "C0")
elseif(CONFIG_STM32F0)
    set(STM32_SERIES "stm32f0")
    set(STM32_SERIES_SHORT "f0")
    set(STM32_FAMILY "STM32F0xx")
    set(STM32_SERIES_UPPER "F0")
elseif(CONFIG_STM32F1)
    set(STM32_SERIES "stm32f1")
    set(STM32_SERIES_SHORT "f1")
    set(STM32_FAMILY "STM32F1xx")
    set(STM32_SERIES_UPPER "F1")
elseif(CONFIG_STM32F2)
    set(STM32_SERIES "stm32f2")
    set(STM32_SERIES_SHORT "f2")
    set(STM32_FAMILY "STM32F2xx")
    set(STM32_SERIES_UPPER "F2")
elseif(CONFIG_STM32F3)
    set(STM32_SERIES "stm32f3")
    set(STM32_SERIES_SHORT "f3")
    set(STM32_FAMILY "STM32F3xx")
    set(STM32_SERIES_UPPER "F3")
elseif(CONFIG_STM32F4)
    set(STM32_SERIES "stm32f4")
    set(STM32_SERIES_SHORT "f4")
    set(STM32_FAMILY "STM32F4xx")
    set(STM32_SERIES_UPPER "F4")
elseif(CONFIG_STM32F7)
    set(STM32_SERIES "stm32f7")
    set(STM32_SERIES_SHORT "f7")
    set(STM32_FAMILY "STM32F7xx")
    set(STM32_SERIES_UPPER "F7")
elseif(CONFIG_STM32G0)
    set(STM32_SERIES "stm32g0")
    set(STM32_SERIES_SHORT "g0")
    set(STM32_FAMILY "STM32G0xx")
    set(STM32_SERIES_UPPER "G0")
elseif(CONFIG_STM32G4)
    set(STM32_SERIES "stm32g4")
    set(STM32_SERIES_SHORT "g4")
    set(STM32_FAMILY "STM32G4xx")
    set(STM32_SERIES_UPPER "G4")
elseif(CONFIG_STM32H5)
    set(STM32_SERIES "stm32h5")
    set(STM32_SERIES_SHORT "h5")
    set(STM32_FAMILY "STM32H5xx")
    set(STM32_SERIES_UPPER "H5")
elseif(CONFIG_STM32H7)
    set(STM32_SERIES "stm32h7")
    set(STM32_SERIES_SHORT "h7")
    set(STM32_FAMILY "STM32H7xx")
    set(STM32_SERIES_UPPER "H7")
elseif(CONFIG_STM32L0)
    set(STM32_SERIES "stm32l0")
    set(STM32_SERIES_SHORT "l0")
    set(STM32_FAMILY "STM32L0xx")
    set(STM32_SERIES_UPPER "L0")
elseif(CONFIG_STM32L1)
    set(STM32_SERIES "stm32l1")
    set(STM32_SERIES_SHORT "l1")
    set(STM32_FAMILY "STM32L1xx")
    set(STM32_SERIES_UPPER "L1")
elseif(CONFIG_STM32L4)
    set(STM32_SERIES "stm32l4")
    set(STM32_SERIES_SHORT "l4")
    set(STM32_FAMILY "STM32L4xx")
    set(STM32_SERIES_UPPER "L4")
elseif(CONFIG_STM32L5)
    set(STM32_SERIES "stm32l5")
    set(STM32_SERIES_SHORT "l5")
    set(STM32_FAMILY "STM32L5xx")
    set(STM32_SERIES_UPPER "L5")
elseif(CONFIG_STM32U0)
    set(STM32_SERIES "stm32u0")
    set(STM32_SERIES_SHORT "u0")
    set(STM32_FAMILY "STM32U0xx")
    set(STM32_SERIES_UPPER "U0")
elseif(CONFIG_STM32U3)
    set(STM32_SERIES "stm32u3")
    set(STM32_SERIES_SHORT "u3")
    set(STM32_FAMILY "STM32U3xx")
    set(STM32_SERIES_UPPER "U3")
elseif(CONFIG_STM32U5)
    set(STM32_SERIES "stm32u5")
    set(STM32_SERIES_SHORT "u5")
    set(STM32_FAMILY "STM32U5xx")
    set(STM32_SERIES_UPPER "U5")
elseif(CONFIG_STM32WB)
    set(STM32_SERIES "stm32wb")
    set(STM32_SERIES_SHORT "wb")
    set(STM32_FAMILY "STM32WBxx")
    set(STM32_SERIES_UPPER "WB")
elseif(CONFIG_STM32WL)
    set(STM32_SERIES "stm32wl")
    set(STM32_SERIES_SHORT "wl")
    set(STM32_FAMILY "STM32WLxx")
    set(STM32_SERIES_UPPER "WL")
else()
    message(FATAL_ERROR "No STM32 series selected in Kconfig. Please run 'menuconfig'.")
endif()

message(STATUS "STM32 Series: ${STM32_SERIES_UPPER}")

#-----------------------------------------------------------------------------
# Chip Variant Detection from Kconfig
#-----------------------------------------------------------------------------

# Get chip name from Kconfig
if(DEFINED CONFIG_STM32_CHIP_NAME)
    set(STM32_DEVICE ${CONFIG_STM32_CHIP_NAME})
    message(STATUS "STM32 Device: ${STM32_DEVICE}")
else()
    message(FATAL_ERROR "No STM32 chip variant selected in Kconfig")
endif()

# Extract variant name for linker script (e.g., STM32F407xx -> stm32f407)
string(TOLOWER ${STM32_DEVICE} STM32_VARIANT_LOWER)
string(REGEX REPLACE "xx$" "" STM32_VARIANT ${STM32_VARIANT_LOWER})

# Set default linker chip name (can be overridden by specific variants)
set(STM32_LINKER_CHIP ${STM32_VARIANT})

#-----------------------------------------------------------------------------
# Memory Configuration from Kconfig
#-----------------------------------------------------------------------------

if(DEFINED CONFIG_STM32_FLASH_SIZE)
    set(STM32_FLASH_SIZE ${CONFIG_STM32_FLASH_SIZE})
    message(STATUS "Flash Size: ${STM32_FLASH_SIZE} bytes")
endif()

if(DEFINED CONFIG_STM32_SRAM_SIZE)
    set(STM32_SRAM_SIZE ${CONFIG_STM32_SRAM_SIZE})
    message(STATUS "SRAM Size: ${STM32_SRAM_SIZE} bytes")
endif()

#-----------------------------------------------------------------------------
# Vendor Paths Configuration
#-----------------------------------------------------------------------------

# CMSIS Core path (ARM CMSIS_5)
set(CMSIS_CORE_DIR ${CMAKE_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core)

# CMSIS Device path (ST device files)
set(CMSIS_DEVICE_DIR ${CMAKE_SOURCE_DIR}/vendors/st/cmsis_device_${STM32_SERIES_SHORT})

# HAL Driver path (ST HAL driver)
set(HAL_DRIVER_DIR ${CMAKE_SOURCE_DIR}/vendors/st/${STM32_SERIES}xx_hal_driver)

# Verify vendor paths exist
if(NOT EXISTS ${CMSIS_CORE_DIR})
    message(WARNING "CMSIS Core not found at: ${CMSIS_CORE_DIR}")
    message(WARNING "Please ensure vendor submodules are initialized")
endif()

if(NOT EXISTS ${CMSIS_DEVICE_DIR})
    message(WARNING "CMSIS Device not found at: ${CMSIS_DEVICE_DIR}")
    message(WARNING "Please ensure vendor submodules are initialized")
endif()

if(NOT EXISTS ${HAL_DRIVER_DIR})
    message(WARNING "HAL Driver not found at: ${HAL_DRIVER_DIR}")
    message(WARNING "Please ensure vendor submodules are initialized")
endif()

#-----------------------------------------------------------------------------
# Startup File Selection
#-----------------------------------------------------------------------------

# Construct path to vendor-provided startup file
# Note: Startup files use the full device name (e.g., startup_stm32f407xx.s)
set(VENDOR_STARTUP_FILE
    ${CMSIS_DEVICE_DIR}/Source/Templates/${VENDOR_TOOLCHAIN_DIR}/startup_${STM32_VARIANT_LOWER}.s)

# Verify startup file exists
if(EXISTS ${VENDOR_STARTUP_FILE})
    message(STATUS "Startup file: ${VENDOR_STARTUP_FILE}")
else()
    message(WARNING "Vendor startup file not found: ${VENDOR_STARTUP_FILE}")
    message(WARNING "Build may fail if startup file is missing")
endif()

#-----------------------------------------------------------------------------
# System Initialization File
#-----------------------------------------------------------------------------

# Vendor-provided system initialization file
set(VENDOR_SYSTEM_FILE
    ${CMSIS_DEVICE_DIR}/Source/Templates/system_${STM32_SERIES}xx.c)

# Verify system file exists
if(EXISTS ${VENDOR_SYSTEM_FILE})
    message(STATUS "System file: ${VENDOR_SYSTEM_FILE}")
else()
    message(WARNING "Vendor system file not found: ${VENDOR_SYSTEM_FILE}")
endif()

#-----------------------------------------------------------------------------
# Linker Script Selection
#-----------------------------------------------------------------------------

# Determine linker script extension based on toolchain
if(NEXUS_TOOLCHAIN_IS_IAR)
    set(LINKER_SCRIPT_EXT "icf")
elseif(NEXUS_TOOLCHAIN_IS_CLANG)
    set(LINKER_SCRIPT_EXT "sct")
else()
    set(LINKER_SCRIPT_EXT "ld")
endif()

# Construct path to linker script
set(LINKER_SCRIPT_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/linker/${STM32_SERIES}/${VENDOR_TOOLCHAIN_DIR}/${STM32_LINKER_CHIP}.${LINKER_SCRIPT_EXT})

# Verify linker script exists
if(EXISTS ${LINKER_SCRIPT_PATH})
    message(STATUS "Linker script: ${LINKER_SCRIPT_PATH}")
else()
    message(WARNING "Linker script not found: ${LINKER_SCRIPT_PATH}")
    message(WARNING "You may need to create a linker script for this variant")
endif()

#-----------------------------------------------------------------------------
# Include Directories
#-----------------------------------------------------------------------------

# CMSIS include directories
set(CMSIS_INCLUDE_DIRS
    ${CMSIS_CORE_DIR}/Include
    ${CMSIS_DEVICE_DIR}/Include
)

# HAL include directories
set(HAL_INCLUDE_DIRS
    ${HAL_DRIVER_DIR}/Inc
    ${HAL_DRIVER_DIR}/Inc/Legacy
)

# Platform include directories
# Note: We add the include/ root directory so that code can use
#       #include "boot/stm32_boot.h" style includes
set(PLATFORM_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_SOURCE_DIR}  # For nexus_config.h
)

#-----------------------------------------------------------------------------
# HAL Driver Source Files
#-----------------------------------------------------------------------------

# Core HAL modules (always included)
set(HAL_SOURCES
    ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal.c
    ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_cortex.c
    ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_rcc.c
    ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_rcc_ex.c
    ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_gpio.c
    ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_flash.c
    ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_flash_ex.c
    ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_pwr.c
    ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_pwr_ex.c
    ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_dma.c
)

# Optional HAL modules based on Kconfig
if(CONFIG_STM32_UART_ENABLE)
    list(APPEND HAL_SOURCES
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_uart.c
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_usart.c
    )
    message(STATUS "HAL Module: UART enabled")
endif()

if(CONFIG_STM32_SPI_ENABLE)
    list(APPEND HAL_SOURCES
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_spi.c
    )
    message(STATUS "HAL Module: SPI enabled")
endif()

if(CONFIG_STM32_I2C_ENABLE)
    list(APPEND HAL_SOURCES
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_i2c.c
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_i2c_ex.c
    )
    message(STATUS "HAL Module: I2C enabled")
endif()

if(CONFIG_STM32_TIMER_ENABLE)
    list(APPEND HAL_SOURCES
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_tim.c
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_tim_ex.c
    )
    message(STATUS "HAL Module: Timer enabled")
endif()

if(CONFIG_STM32_ADC_ENABLE)
    list(APPEND HAL_SOURCES
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_adc.c
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_adc_ex.c
    )
    message(STATUS "HAL Module: ADC enabled")
endif()

if(CONFIG_STM32_DAC_ENABLE)
    list(APPEND HAL_SOURCES
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_dac.c
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_dac_ex.c
    )
    message(STATUS "HAL Module: DAC enabled")
endif()

if(CONFIG_STM32_RTC_ENABLE)
    list(APPEND HAL_SOURCES
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_rtc.c
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_rtc_ex.c
    )
    message(STATUS "HAL Module: RTC enabled")
endif()

if(CONFIG_STM32_CAN_ENABLE)
    list(APPEND HAL_SOURCES
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_can.c
    )
    message(STATUS "HAL Module: CAN enabled")
endif()

if(CONFIG_STM32_USB_ENABLE)
    list(APPEND HAL_SOURCES
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_pcd.c
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_pcd_ex.c
    )
    message(STATUS "HAL Module: USB enabled")
endif()

if(CONFIG_STM32_ETHERNET_ENABLE)
    list(APPEND HAL_SOURCES
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_eth.c
    )
    message(STATUS "HAL Module: Ethernet enabled")
endif()

if(CONFIG_STM32_SDIO_ENABLE)
    list(APPEND HAL_SOURCES
        ${HAL_DRIVER_DIR}/Src/${STM32_SERIES}xx_hal_sd.c
    )
    message(STATUS "HAL Module: SDIO enabled")
endif()

#-----------------------------------------------------------------------------
# Create CMSIS Device Interface Library
#-----------------------------------------------------------------------------

add_library(cmsis_device_${STM32_SERIES} INTERFACE)

target_include_directories(cmsis_device_${STM32_SERIES} INTERFACE
    ${CMSIS_INCLUDE_DIRS}
)

target_compile_definitions(cmsis_device_${STM32_SERIES} INTERFACE
    ${STM32_DEVICE}
)

#-----------------------------------------------------------------------------
# Create HAL Driver Static Library
#-----------------------------------------------------------------------------

add_library(${STM32_SERIES}xx_hal_driver STATIC ${HAL_SOURCES})

target_include_directories(${STM32_SERIES}xx_hal_driver PUBLIC
    ${HAL_INCLUDE_DIRS}
    ${CMSIS_INCLUDE_DIRS}
    ${PLATFORM_INCLUDE_DIRS}
)

target_compile_definitions(${STM32_SERIES}xx_hal_driver PUBLIC
    ${STM32_DEVICE}
    USE_HAL_DRIVER
)

# Link CMSIS device
target_link_libraries(${STM32_SERIES}xx_hal_driver PUBLIC
    cmsis_device_${STM32_SERIES}
)

#-----------------------------------------------------------------------------
# Platform Source Files
#-----------------------------------------------------------------------------

# Vendor-provided files
set(PLATFORM_SOURCES
    ${VENDOR_SYSTEM_FILE}
)

# Handle startup file compilation based on toolchain
if(NEXUS_TOOLCHAIN_IS_CLANG)
    # ARM Clang: armasm requires separate compilation
    set(STARTUP_OBJ_FILE ${CMAKE_CURRENT_BINARY_DIR}/startup_${STM32_VARIANT_LOWER}.o)

    add_custom_command(
        OUTPUT ${STARTUP_OBJ_FILE}
        COMMAND ${CMAKE_ASM_COMPILER} --cpu=Cortex-M4.fp -g -c
                -o ${STARTUP_OBJ_FILE}
                ${VENDOR_STARTUP_FILE}
        DEPENDS ${VENDOR_STARTUP_FILE}
        COMMENT "Assembling ${VENDOR_STARTUP_FILE}"
        VERBATIM
    )

    list(APPEND PLATFORM_SOURCES ${STARTUP_OBJ_FILE})
else()
    # GCC and IAR: use normal compilation
    list(APPEND PLATFORM_SOURCES ${VENDOR_STARTUP_FILE})
endif()

# Nexus platform implementation files organized by module
# Boot module
file(GLOB BOOT_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/boot/*.c)
list(APPEND PLATFORM_SOURCES ${BOOT_SRC_FILES})

# Clock module
file(GLOB CLOCK_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/clock/*.c)
list(APPEND PLATFORM_SOURCES ${CLOCK_SRC_FILES})

# Interrupt module
file(GLOB INTERRUPT_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/interrupt/*.c)
list(APPEND PLATFORM_SOURCES ${INTERRUPT_SRC_FILES})

# System module
file(GLOB SYSTEM_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/system/*.c)
list(APPEND PLATFORM_SOURCES ${SYSTEM_SRC_FILES})

# Peripheral module
file(GLOB PERIPHERAL_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/peripheral/*.c)
list(APPEND PLATFORM_SOURCES ${PERIPHERAL_SRC_FILES})

# Platform common files
file(GLOB PLATFORM_COMMON_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/boot/*.c)
list(APPEND PLATFORM_SOURCES ${PLATFORM_COMMON_FILES})

#-----------------------------------------------------------------------------
# Create Platform Static Library
#-----------------------------------------------------------------------------

add_library(platform_stm32 STATIC ${PLATFORM_SOURCES})

target_include_directories(platform_stm32 PUBLIC
    ${PLATFORM_INCLUDE_DIRS}
    ${CMSIS_INCLUDE_DIRS}
    ${HAL_INCLUDE_DIRS}
)

target_compile_definitions(platform_stm32 PUBLIC
    ${STM32_DEVICE}
    USE_HAL_DRIVER
)

# Link CMSIS and HAL
target_link_libraries(platform_stm32 PUBLIC
    cmsis_device_${STM32_SERIES}
    ${STM32_SERIES}xx_hal_driver
)

#-----------------------------------------------------------------------------
# Toolchain-Specific Compiler Flags
#-----------------------------------------------------------------------------

if(NEXUS_TOOLCHAIN_IS_GCC)
    # ARM GCC specific flags
    set(TOOLCHAIN_COMPILE_FLAGS
        -ffunction-sections
        -fdata-sections
        -fno-common
        -fno-builtin
        -Wall
        -Wextra
        -Wpedantic
    )

    set(TOOLCHAIN_LINK_FLAGS
        -Wl,--gc-sections
        -Wl,--print-memory-usage
        -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map
    )

elseif(NEXUS_TOOLCHAIN_IS_CLANG)
    # ARM Clang specific flags
    set(TOOLCHAIN_COMPILE_FLAGS
        -ffunction-sections
        -fdata-sections
        -fno-common
        -fno-builtin
        -Wall
        -Wextra
    )

    # ARM Linker (armlink) uses different syntax than GCC
    set(TOOLCHAIN_LINK_FLAGS
        # Linker flags are set in CMAKE_EXE_LINKER_FLAGS_INIT
    )

elseif(NEXUS_TOOLCHAIN_IS_IAR)
    # IAR specific flags
    set(TOOLCHAIN_COMPILE_FLAGS
        --no_cse
        --no_unroll
        --no_inline
        --no_code_motion
        --no_tbaa
        --no_clustering
        --no_scheduling
        --endian=little
    )

    set(TOOLCHAIN_LINK_FLAGS
        --map ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map
    )
endif()

# Apply toolchain-specific flags
target_compile_options(platform_stm32 PRIVATE ${TOOLCHAIN_COMPILE_FLAGS})
target_compile_options(${STM32_SERIES}xx_hal_driver PRIVATE ${TOOLCHAIN_COMPILE_FLAGS})

# For ARM Clang, prevent C compiler flags from being passed to armasm
if(NEXUS_TOOLCHAIN_IS_CLANG)
    # Override ASM compile flags to include CPU and debug flags
    set(CMAKE_ASM_FLAGS "--cpu=Cortex-M4.fp -g" CACHE STRING "" FORCE)
endif()

#-----------------------------------------------------------------------------
# Linker Script Configuration
#-----------------------------------------------------------------------------

if(EXISTS ${LINKER_SCRIPT_PATH})
    if(NEXUS_TOOLCHAIN_IS_IAR)
        # IAR uses --config flag
        list(APPEND TOOLCHAIN_LINK_FLAGS --config ${LINKER_SCRIPT_PATH})
    elseif(NEXUS_TOOLCHAIN_IS_CLANG)
        # ARM Clang uses --scatter flag
        list(APPEND TOOLCHAIN_LINK_FLAGS "LINKER:--scatter=${LINKER_SCRIPT_PATH}")
        # Note: Do NOT set --entry=Reset_Handler when using standard C library
        # The default entry point __main is required for proper C library initialization
        # Only set custom entry point when using microlib (__MICROLIB defined)
    else()
        # GCC uses -T flag
        list(APPEND TOOLCHAIN_LINK_FLAGS -T${LINKER_SCRIPT_PATH})
    endif()
endif()

# Apply linker flags
target_link_options(platform_stm32 PUBLIC ${TOOLCHAIN_LINK_FLAGS})

#-----------------------------------------------------------------------------
# Compile Definitions from Kconfig
#-----------------------------------------------------------------------------

# HSE value (external oscillator frequency)
if(DEFINED CONFIG_STM32_HSE_VALUE)
    target_compile_definitions(platform_stm32 PUBLIC
        HSE_VALUE=${CONFIG_STM32_HSE_VALUE}
    )
    target_compile_definitions(${STM32_SERIES}xx_hal_driver PUBLIC
        HSE_VALUE=${CONFIG_STM32_HSE_VALUE}
    )
    message(STATUS "HSE Value: ${CONFIG_STM32_HSE_VALUE} Hz")
endif()

# System clock frequency
if(DEFINED CONFIG_STM32_SYSCLK_FREQ)
    target_compile_definitions(platform_stm32 PUBLIC
        SYSCLK_FREQ=${CONFIG_STM32_SYSCLK_FREQ}
    )
    message(STATUS "System Clock: ${CONFIG_STM32_SYSCLK_FREQ} Hz")
endif()

# Stack and heap sizes
if(DEFINED CONFIG_STACK_SIZE)
    target_compile_definitions(platform_stm32 PUBLIC
        STACK_SIZE=${CONFIG_STACK_SIZE}
    )
    # Pass to linker via preprocessor
    list(APPEND TOOLCHAIN_LINK_FLAGS
        -Wl,--defsym,_Min_Stack_Size=${CONFIG_STACK_SIZE}
    )
    message(STATUS "Stack Size: ${CONFIG_STACK_SIZE} bytes")
endif()

if(DEFINED CONFIG_HEAP_SIZE)
    target_compile_definitions(platform_stm32 PUBLIC
        HEAP_SIZE=${CONFIG_HEAP_SIZE}
    )
    # Pass to linker via preprocessor
    list(APPEND TOOLCHAIN_LINK_FLAGS
        -Wl,--defsym,_Min_Heap_Size=${CONFIG_HEAP_SIZE}
    )
    message(STATUS "Heap Size: ${CONFIG_HEAP_SIZE} bytes")
endif()

# Debug mode and assertions
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Note: USE_FULL_ASSERT is disabled by default to avoid linker issues
    # Enable it only if you have implemented assert_failed() properly
    # target_compile_definitions(platform_stm32 PUBLIC USE_FULL_ASSERT)
    # target_compile_definitions(${STM32_SERIES}xx_hal_driver PUBLIC USE_FULL_ASSERT)
    message(STATUS "Assertions: Disabled (enable USE_FULL_ASSERT in CMakeLists.txt if needed)")
else()
    message(STATUS "Assertions: Disabled (Release build)")
endif()

# LL driver support
if(CONFIG_STM32_USE_LL_DRIVER)
    target_compile_definitions(platform_stm32 PUBLIC
        USE_FULL_LL_DRIVER
    )
    message(STATUS "LL Driver: Enabled")
endif()

# RTOS support
if(CONFIG_STM32_ENABLE_RTOS_SUPPORT)
    target_compile_definitions(platform_stm32 PUBLIC
        USE_RTOS
    )
    message(STATUS "RTOS Support: Enabled")
endif()

#-----------------------------------------------------------------------------
# Configuration Summary
#-----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "STM32 Platform Configuration Summary:")
message(STATUS "  Series:         ${STM32_SERIES_UPPER}")
message(STATUS "  Device:         ${STM32_DEVICE}")
message(STATUS "  Variant:        ${STM32_VARIANT}")
message(STATUS "  Toolchain:      ${NEXUS_TOOLCHAIN_NAME}")
if(DEFINED STM32_FLASH_SIZE)
    message(STATUS "  Flash:          ${STM32_FLASH_SIZE} bytes")
endif()
if(DEFINED STM32_SRAM_SIZE)
    message(STATUS "  SRAM:           ${STM32_SRAM_SIZE} bytes")
endif()
message(STATUS "  CMSIS Device:   ${CMSIS_DEVICE_DIR}")
message(STATUS "  HAL Driver:     ${HAL_DRIVER_DIR}")
message(STATUS "=== STM32 Platform Configuration Complete ===")
message(STATUS "")

#-----------------------------------------------------------------------------
# End of STM32 Platform CMake Configuration
#-----------------------------------------------------------------------------
