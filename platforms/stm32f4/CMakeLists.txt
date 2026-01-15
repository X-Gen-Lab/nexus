##############################################################################
# STM32F4 Platform Support
##############################################################################
# Requirements: 11.1, 11.2, 11.3, 11.5
##############################################################################

##############################################################################
# Compiler Detection
##############################################################################
# Detect compiler type and set appropriate flags
# Supports: GCC (arm-none-eabi-gcc), Clang (armclang), IAR (iccarm)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(COMPILER_TYPE "GCC")
    set(COMPILER_IS_GCC TRUE)
    set(COMPILER_IS_CLANG FALSE)
    set(COMPILER_IS_IAR FALSE)
    message(STATUS "STM32F4: Detected GCC compiler")
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "ARMClang")
    set(COMPILER_TYPE "CLANG")
    set(COMPILER_IS_GCC FALSE)
    set(COMPILER_IS_CLANG TRUE)
    set(COMPILER_IS_IAR FALSE)
    message(STATUS "STM32F4: Detected Clang/ARMClang compiler")
elseif(CMAKE_C_COMPILER_ID STREQUAL "IAR")
    set(COMPILER_TYPE "IAR")
    set(COMPILER_IS_GCC FALSE)
    set(COMPILER_IS_CLANG FALSE)
    set(COMPILER_IS_IAR TRUE)
    message(STATUS "STM32F4: Detected IAR compiler")
else()
    set(COMPILER_TYPE "UNKNOWN")
    set(COMPILER_IS_GCC FALSE)
    set(COMPILER_IS_CLANG FALSE)
    set(COMPILER_IS_IAR FALSE)
    message(WARNING "STM32F4: Unknown compiler '${CMAKE_C_COMPILER_ID}', using default settings")
endif()

##############################################################################
# Compiler-Specific Flags
##############################################################################

# CPU flags for Cortex-M4 with FPU
if(COMPILER_IS_GCC)
    set(STM32F4_CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
    set(STM32F4_COMPILE_FLAGS "-ffunction-sections -fdata-sections -fno-common")
    set(STM32F4_LINK_FLAGS "-Wl,--gc-sections -Wl,--print-memory-usage")
    set(STM32F4_ASM_FLAGS "-x assembler-with-cpp")
elseif(COMPILER_IS_CLANG)
    set(STM32F4_CPU_FLAGS "--target=arm-none-eabi -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
    set(STM32F4_COMPILE_FLAGS "-ffunction-sections -fdata-sections -fno-common")
    set(STM32F4_LINK_FLAGS "-Wl,--gc-sections")
    set(STM32F4_ASM_FLAGS "-x assembler-with-cpp")
elseif(COMPILER_IS_IAR)
    set(STM32F4_CPU_FLAGS "--cpu=Cortex-M4 --fpu=VFPv4_sp")
    set(STM32F4_COMPILE_FLAGS "")
    set(STM32F4_LINK_FLAGS "")
    set(STM32F4_ASM_FLAGS "")
else()
    # Default flags for unknown compilers
    set(STM32F4_CPU_FLAGS "")
    set(STM32F4_COMPILE_FLAGS "")
    set(STM32F4_LINK_FLAGS "")
    set(STM32F4_ASM_FLAGS "")
endif()

##############################################################################
# Linker Script Selection
##############################################################################

if(COMPILER_IS_IAR)
    set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32F407VGTx_FLASH.icf)
    set(LINKER_SCRIPT_FLAG "--config")
else()
    # GCC and Clang use GNU-style linker scripts
    set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32F407VGTx_FLASH.ld)
    set(LINKER_SCRIPT_FLAG "-T")
endif()

message(STATUS "STM32F4: Using linker script: ${LINKER_SCRIPT}")

##############################################################################
# Official Vendor Library Paths (Git Submodules)
##############################################################################

set(CMSIS_CORE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include)
set(ST_DEVICE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include)
set(ST_HAL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vendors/st/stm32f4xx_hal_driver/Inc)
set(ST_HAL_SOURCE_DIR ${CMAKE_SOURCE_DIR}/vendors/st/stm32f4xx_hal_driver/Src)
set(ST_DEVICE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/vendors/st/cmsis_device_f4/Source/Templates)

##############################################################################
# Startup File Selection (Compiler-Specific)
##############################################################################

if(COMPILER_IS_GCC OR COMPILER_IS_CLANG)
    set(STARTUP_FILE ${ST_DEVICE_SOURCE_DIR}/gcc/startup_stm32f407xx.s)
elseif(COMPILER_IS_IAR)
    set(STARTUP_FILE ${ST_DEVICE_SOURCE_DIR}/iar/startup_stm32f407xx.s)
else()
    # Default to GCC startup
    set(STARTUP_FILE ${ST_DEVICE_SOURCE_DIR}/gcc/startup_stm32f407xx.s)
endif()

##############################################################################
# Platform Library Definition
##############################################################################

add_library(platform_stm32f4 STATIC)

##############################################################################
# Source Files
##############################################################################

# ST HAL Driver source files
set(ST_HAL_SOURCES
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_gpio.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_rcc.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_rcc_ex.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_cortex.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_pwr.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_pwr_ex.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_flash.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_flash_ex.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_exti.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_uart.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_spi.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_i2c.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_tim.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_tim_ex.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_adc.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_adc_ex.c
    ${ST_HAL_SOURCE_DIR}/stm32f4xx_hal_dma.c
)

# Nexus HAL adapter source files (platform-specific implementations)
set(NEXUS_HAL_SOURCES
    src/hal_gpio_stm32f4.c
    src/hal_uart_stm32f4.c
    src/hal_spi_stm32f4.c
    src/hal_i2c_stm32f4.c
    src/hal_timer_stm32f4.c
    src/hal_adc_stm32f4.c
    src/hal_system_stm32f4.c
)

# System and startup files
set(SYSTEM_SOURCES
    ${ST_DEVICE_SOURCE_DIR}/system_stm32f4xx.c
    ${STARTUP_FILE}
)

target_sources(platform_stm32f4
    PRIVATE
        ${SYSTEM_SOURCES}
        ${ST_HAL_SOURCES}
        ${NEXUS_HAL_SOURCES}
)

##############################################################################
# Include Directories
##############################################################################

target_include_directories(platform_stm32f4
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMSIS_CORE_INCLUDE_DIR}
        ${ST_DEVICE_INCLUDE_DIR}
        ${ST_HAL_INCLUDE_DIR}
)

##############################################################################
# Compile Definitions
##############################################################################

target_compile_definitions(platform_stm32f4
    PUBLIC
        STM32F407xx
        USE_HAL_DRIVER
        # Compiler type definitions for conditional compilation
        $<$<BOOL:${COMPILER_IS_GCC}>:COMPILER_GCC=1>
        $<$<BOOL:${COMPILER_IS_CLANG}>:COMPILER_CLANG=1>
        $<$<BOOL:${COMPILER_IS_IAR}>:COMPILER_IAR=1>
)

##############################################################################
# Compile Options
##############################################################################

# Apply compiler-specific flags
if(COMPILER_IS_GCC OR COMPILER_IS_CLANG)
    target_compile_options(platform_stm32f4
        PRIVATE
            ${STM32F4_CPU_FLAGS}
            ${STM32F4_COMPILE_FLAGS}
    )
    # Suppress warnings from vendor code
    set_source_files_properties(${ST_HAL_SOURCES} ${SYSTEM_SOURCES}
        PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-pedantic"
    )
elseif(COMPILER_IS_IAR)
    target_compile_options(platform_stm32f4
        PRIVATE
            ${STM32F4_CPU_FLAGS}
    )
endif()

##############################################################################
# Link Libraries
##############################################################################

target_link_libraries(platform_stm32f4
    PUBLIC
        hal_interface
)

##############################################################################
# Export Variables for Applications
##############################################################################

# Export linker script path
set(NEXUS_LINKER_SCRIPT ${LINKER_SCRIPT} PARENT_SCOPE)
set(NEXUS_LINKER_SCRIPT_FLAG ${LINKER_SCRIPT_FLAG} PARENT_SCOPE)

# Export compiler type
set(NEXUS_COMPILER_TYPE ${COMPILER_TYPE} PARENT_SCOPE)

# Export CPU flags for applications
set(NEXUS_CPU_FLAGS ${STM32F4_CPU_FLAGS} PARENT_SCOPE)
set(NEXUS_LINK_FLAGS ${STM32F4_LINK_FLAGS} PARENT_SCOPE)

##############################################################################
# Helper Function: Create STM32F4 Executable
##############################################################################

function(nexus_add_stm32f4_executable TARGET)
    # Parse arguments
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})

    # Create executable
    add_executable(${TARGET} ${ARG_SOURCES})

    # Link platform library and any additional libraries
    target_link_libraries(${TARGET}
        PRIVATE
            platform_stm32f4
            ${ARG_LIBS}
    )

    # Apply linker script
    if(COMPILER_IS_GCC OR COMPILER_IS_CLANG)
        target_link_options(${TARGET}
            PRIVATE
                -T${LINKER_SCRIPT}
                ${STM32F4_LINK_FLAGS}
                -specs=nano.specs
                -specs=nosys.specs
        )
    elseif(COMPILER_IS_IAR)
        target_link_options(${TARGET}
            PRIVATE
                --config ${LINKER_SCRIPT}
        )
    endif()

    # Generate binary and hex files (GCC/Clang only)
    if(COMPILER_IS_GCC OR COMPILER_IS_CLANG)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> ${TARGET}.bin
            COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET}> ${TARGET}.hex
            COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET}>
            COMMENT "Generating ${TARGET}.bin and ${TARGET}.hex"
        )
    endif()
endfunction()

##############################################################################
# Build Summary
##############################################################################

message(STATUS "")
message(STATUS "=== STM32F4 Platform Configuration ===")
message(STATUS "  Compiler:       ${COMPILER_TYPE}")
message(STATUS "  CPU Flags:      ${STM32F4_CPU_FLAGS}")
message(STATUS "  Linker Script:  ${LINKER_SCRIPT}")
message(STATUS "  Startup File:   ${STARTUP_FILE}")
message(STATUS "======================================")
message(STATUS "")
