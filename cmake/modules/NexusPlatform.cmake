# NexusPlatform.cmake
# Nexus Platform Detection and Configuration
# Author: Nexus Team

# ---------------------------------------------------------------------------
# Platform Detection and Configuration Module
# ---------------------------------------------------------------------------

# This module provides comprehensive platform detection and
#                  configuration for the Nexus build system. It detects the
#                  host platform, target platform, compiler, toolchain, and
#                  generator, and configures appropriate build settings.

include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# Host Platform Detection
# ---------------------------------------------------------------------------

# Detect host platform (Windows/Linux/macOS)
# Sets NEXUS_HOST_* variables based on CMAKE_HOST_SYSTEM_NAME
#                  Validates: Requirements 4.1
function(nexus_detect_host_platform)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(NEXUS_HOST_WINDOWS TRUE PARENT_SCOPE)
        set(NEXUS_HOST_PLATFORM "Windows" PARENT_SCOPE)
        message(VERBOSE "Detected host platform: Windows")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        set(NEXUS_HOST_LINUX TRUE PARENT_SCOPE)
        set(NEXUS_HOST_PLATFORM "Linux" PARENT_SCOPE)
        message(VERBOSE "Detected host platform: Linux")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(NEXUS_HOST_MACOS TRUE PARENT_SCOPE)
        set(NEXUS_HOST_PLATFORM "macOS" PARENT_SCOPE)
        message(VERBOSE "Detected host platform: macOS")
    else()
        set(NEXUS_HOST_PLATFORM "Unknown" PARENT_SCOPE)
        message(WARNING "Unknown host platform: ${CMAKE_HOST_SYSTEM_NAME}")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Target Platform Detection
# ---------------------------------------------------------------------------

# Detect target platform from Kconfig or CMake variables
# Detects STM32, ESP32, nRF52, GD32, or native platform
#                  Validates: Requirements 4.2
function(nexus_detect_target_platform)
    # Check if platform is already set via CMake variable
    if(DEFINED NEXUS_TARGET_PLATFORM)
        set(PLATFORM_NAME ${NEXUS_TARGET_PLATFORM})
        message(VERBOSE "Target platform set via CMake: ${PLATFORM_NAME}")
    # Check Kconfig configuration
    elseif(DEFINED CONFIG_PLATFORM_NAME)
        set(PLATFORM_NAME ${CONFIG_PLATFORM_NAME})
        message(VERBOSE "Target platform from Kconfig: ${PLATFORM_NAME}")
    # Check individual platform flags
    elseif(CONFIG_PLATFORM_STM32)
        set(PLATFORM_NAME "stm32")
    elseif(CONFIG_PLATFORM_ESP32)
        set(PLATFORM_NAME "esp32")
    elseif(CONFIG_PLATFORM_NRF52)
        set(PLATFORM_NAME "nrf52")
    elseif(CONFIG_PLATFORM_GD32)
        set(PLATFORM_NAME "gd32")
    elseif(CONFIG_PLATFORM_NATIVE)
        set(PLATFORM_NAME "native")
    # Check CMAKE_SYSTEM_NAME for cross-compilation
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Generic")
        # Generic embedded system - try to detect from toolchain
        if(CMAKE_C_COMPILER MATCHES "arm-none-eabi")
            set(PLATFORM_NAME "arm-embedded")
            message(VERBOSE "Detected ARM embedded platform from toolchain")
        else()
            set(PLATFORM_NAME "generic")
            message(WARNING "Generic platform detected, specific platform unknown")
        endif()
    else()
        # Default to native platform
        set(PLATFORM_NAME "native")
        message(VERBOSE "Defaulting to native platform")
    endif()

    # Set platform-specific variables
    string(TOUPPER ${PLATFORM_NAME} PLATFORM_UPPER)
    set(NEXUS_TARGET_PLATFORM ${PLATFORM_NAME} PARENT_SCOPE)
    set(NEXUS_TARGET_${PLATFORM_UPPER} TRUE PARENT_SCOPE)

    # Set platform family
    if(PLATFORM_NAME MATCHES "^(stm32|stm32f4|stm32h7|stm32l4|gd32|nrf52|esp32)$")
        set(NEXUS_TARGET_EMBEDDED TRUE PARENT_SCOPE)
        set(NEXUS_TARGET_FAMILY "embedded" PARENT_SCOPE)
    elseif(PLATFORM_NAME STREQUAL "native")
        set(NEXUS_TARGET_NATIVE TRUE PARENT_SCOPE)
        set(NEXUS_TARGET_FAMILY "native" PARENT_SCOPE)
    else()
        set(NEXUS_TARGET_FAMILY "unknown" PARENT_SCOPE)
    endif()

    message(VERBOSE "Target platform: ${PLATFORM_NAME}, Family: ${NEXUS_TARGET_FAMILY}")
endfunction()

# ---------------------------------------------------------------------------
# Compiler and Toolchain Detection
# ---------------------------------------------------------------------------

# Detect compiler type and version
# Detects MSVC, GCC, Clang, ARM GCC, ARM Clang, IAR
#                  Validates: Requirements 4.2
function(nexus_detect_compiler)
    # Detect compiler ID
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        set(NEXUS_COMPILER_MSVC TRUE PARENT_SCOPE)
        set(NEXUS_COMPILER_NAME "MSVC" PARENT_SCOPE)
        set(NEXUS_COMPILER_FAMILY "msvc" PARENT_SCOPE)
    elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # Check if it's ARM GCC
        if(CMAKE_C_COMPILER MATCHES "arm-none-eabi-gcc")
            set(NEXUS_COMPILER_ARM_GCC TRUE PARENT_SCOPE)
            set(NEXUS_COMPILER_NAME "ARM GCC" PARENT_SCOPE)
            set(NEXUS_COMPILER_FAMILY "gcc" PARENT_SCOPE)
            set(NEXUS_TOOLCHAIN_NAME "arm-none-eabi-gcc" PARENT_SCOPE)
        else()
            set(NEXUS_COMPILER_GCC TRUE PARENT_SCOPE)
            set(NEXUS_COMPILER_NAME "GCC" PARENT_SCOPE)
            set(NEXUS_COMPILER_FAMILY "gcc" PARENT_SCOPE)
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        # Check if it's ARM Clang
        if(CMAKE_C_COMPILER MATCHES "armclang")
            set(NEXUS_COMPILER_ARM_CLANG TRUE PARENT_SCOPE)
            set(NEXUS_COMPILER_NAME "ARM Clang" PARENT_SCOPE)
            set(NEXUS_COMPILER_FAMILY "clang" PARENT_SCOPE)
            set(NEXUS_TOOLCHAIN_NAME "armclang" PARENT_SCOPE)
        else()
            set(NEXUS_COMPILER_CLANG TRUE PARENT_SCOPE)
            set(NEXUS_COMPILER_NAME "Clang" PARENT_SCOPE)
            set(NEXUS_COMPILER_FAMILY "clang" PARENT_SCOPE)
        endif()
    elseif(CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
        set(NEXUS_COMPILER_APPLECLANG TRUE PARENT_SCOPE)
        set(NEXUS_COMPILER_NAME "AppleClang" PARENT_SCOPE)
        set(NEXUS_COMPILER_FAMILY "clang" PARENT_SCOPE)
    elseif(CMAKE_C_COMPILER_ID STREQUAL "IAR")
        set(NEXUS_COMPILER_IAR TRUE PARENT_SCOPE)
        set(NEXUS_COMPILER_NAME "IAR" PARENT_SCOPE)
        set(NEXUS_COMPILER_FAMILY "iar" PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_NAME "iar-arm" PARENT_SCOPE)
    else()
        set(NEXUS_COMPILER_NAME "Unknown" PARENT_SCOPE)
        set(NEXUS_COMPILER_FAMILY "unknown" PARENT_SCOPE)
        message(WARNING "Unknown compiler: ${CMAKE_C_COMPILER_ID}")
    endif()

    # Set compiler version
    set(NEXUS_COMPILER_VERSION "${CMAKE_C_COMPILER_VERSION}" PARENT_SCOPE)

    # Detect if cross-compiling
    if(CMAKE_CROSSCOMPILING)
        set(NEXUS_CROSS_COMPILING TRUE PARENT_SCOPE)
    else()
        set(NEXUS_CROSS_COMPILING FALSE PARENT_SCOPE)
    endif()

    message(VERBOSE "Compiler: ${NEXUS_COMPILER_NAME} ${CMAKE_C_COMPILER_VERSION}")
    message(VERBOSE "Cross-compiling: ${CMAKE_CROSSCOMPILING}")
endfunction()

# Detect toolchain from compiler and platform
# Determines the toolchain name for platform switching
#                  Validates: Requirements 4.3
function(nexus_detect_toolchain)
    # If toolchain is already set, use it
    if(DEFINED NEXUS_TOOLCHAIN_NAME)
        message(VERBOSE "Toolchain already set: ${NEXUS_TOOLCHAIN_NAME}")
        return()
    endif()

    # Detect toolchain based on compiler
    if(NEXUS_COMPILER_ARM_GCC)
        set(NEXUS_TOOLCHAIN_NAME "arm-none-eabi-gcc" PARENT_SCOPE)
    elseif(NEXUS_COMPILER_ARM_CLANG)
        set(NEXUS_TOOLCHAIN_NAME "armclang" PARENT_SCOPE)
    elseif(NEXUS_COMPILER_IAR)
        set(NEXUS_TOOLCHAIN_NAME "iar-arm" PARENT_SCOPE)
    elseif(NEXUS_COMPILER_MSVC)
        set(NEXUS_TOOLCHAIN_NAME "msvc" PARENT_SCOPE)
    elseif(NEXUS_COMPILER_GCC)
        set(NEXUS_TOOLCHAIN_NAME "gcc" PARENT_SCOPE)
    elseif(NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG)
        set(NEXUS_TOOLCHAIN_NAME "clang" PARENT_SCOPE)
    else()
        set(NEXUS_TOOLCHAIN_NAME "unknown" PARENT_SCOPE)
    endif()

    message(VERBOSE "Detected toolchain: ${NEXUS_TOOLCHAIN_NAME}")
endfunction()

# ---------------------------------------------------------------------------
# CMake Generator Detection
# ---------------------------------------------------------------------------

# Detect CMake generator
# Detects Visual Studio, Ninja, Make, or other generators
function(nexus_detect_generator)
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        set(NEXUS_GENERATOR_VS TRUE PARENT_SCOPE)
        set(NEXUS_GENERATOR_NAME "Visual Studio" PARENT_SCOPE)
    elseif(CMAKE_GENERATOR STREQUAL "Ninja")
        set(NEXUS_GENERATOR_NINJA TRUE PARENT_SCOPE)
        set(NEXUS_GENERATOR_NAME "Ninja" PARENT_SCOPE)
    elseif(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
        set(NEXUS_GENERATOR_MAKE TRUE PARENT_SCOPE)
        set(NEXUS_GENERATOR_NAME "Make" PARENT_SCOPE)
    elseif(CMAKE_GENERATOR STREQUAL "NMake Makefiles")
        set(NEXUS_GENERATOR_NMAKE TRUE PARENT_SCOPE)
        set(NEXUS_GENERATOR_NAME "NMake" PARENT_SCOPE)
    else()
        set(NEXUS_GENERATOR_NAME "${CMAKE_GENERATOR}" PARENT_SCOPE)
    endif()

    message(VERBOSE "Generator: ${CMAKE_GENERATOR}")
endfunction()

# ---------------------------------------------------------------------------
# Platform Configuration
# ---------------------------------------------------------------------------

# Configure platform-specific settings
# Main entry point for platform configuration
#                  Detects all platform, compiler, and toolchain information
#                  Validates: Requirements 4.1, 4.2
macro(nexus_configure_platform)
    # Detect all platform components
    nexus_detect_host_platform()
    nexus_detect_target_platform()
    nexus_detect_compiler()
    nexus_detect_toolchain()
    nexus_detect_generator()

    # Print configuration summary
    message(STATUS "")
    message(STATUS "=== Nexus Platform Configuration ===")
    message(STATUS "  Host Platform:   ${NEXUS_HOST_PLATFORM}")
    message(STATUS "  Target Platform: ${NEXUS_TARGET_PLATFORM}")
    message(STATUS "  Target Family:   ${NEXUS_TARGET_FAMILY}")
    message(STATUS "  Compiler:        ${NEXUS_COMPILER_NAME} ${NEXUS_COMPILER_VERSION}")
    message(STATUS "  Toolchain:       ${NEXUS_TOOLCHAIN_NAME}")
    message(STATUS "  Generator:       ${NEXUS_GENERATOR_NAME}")
    message(STATUS "  Build Type:      ${CMAKE_BUILD_TYPE}")
    message(STATUS "  Cross-Compile:   ${NEXUS_CROSS_COMPILING}")
    message(STATUS "=====================================")
    message(STATUS "")

    # Validate platform configuration
    nexus_validate_platform_config()

    # Set platform-specific defaults
    nexus_set_platform_defaults()
endmacro()

# Validate platform configuration
# Checks for incompatible platform/toolchain combinations
function(nexus_validate_platform_config)
    # Check if embedded platform has appropriate toolchain
    if(NEXUS_TARGET_EMBEDDED)
        if(NOT NEXUS_CROSS_COMPILING)
            message(WARNING
                "Embedded target platform '${NEXUS_TARGET_PLATFORM}' detected "
                "but not cross-compiling. This may cause build issues.")
        endif()

        # Check for ARM toolchain on ARM platforms
        if(NEXUS_TARGET_PLATFORM MATCHES "^(stm32|gd32|nrf52)$")
            if(NOT (NEXUS_COMPILER_ARM_GCC OR NEXUS_COMPILER_ARM_CLANG OR NEXUS_COMPILER_IAR))
                message(FATAL_ERROR
                    "ARM platform '${NEXUS_TARGET_PLATFORM}' requires ARM toolchain.\n"
                    "Detected compiler: ${NEXUS_COMPILER_NAME}\n"
                    "Please use one of:\n"
                    "  - arm-none-eabi-gcc (ARM GCC)\n"
                    "  - armclang (ARM Clang)\n"
                    "  - iccarm (IAR)\n"
                    "Set toolchain with: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/<toolchain>.cmake")
            endif()
        endif()
    endif()

    # Check native platform
    if(NEXUS_TARGET_NATIVE)
        if(NEXUS_CROSS_COMPILING)
            message(WARNING
                "Native platform selected but cross-compiling is enabled. "
                "This may cause unexpected behavior.")
        endif()
    endif()
endfunction()

# Set platform-specific defaults
# Configures platform-specific build settings
function(nexus_set_platform_defaults)
    if(NEXUS_HOST_WINDOWS)
        # Windows-specific settings
        if(NEXUS_COMPILER_MSVC)
            # Enable multi-processor compilation
            add_compile_options(/MP)

            # Set runtime library
            if(BUILD_SHARED_LIBS)
                set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" PARENT_SCOPE)
            else()
                set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" PARENT_SCOPE)
            endif()
        endif()
    elseif(NEXUS_HOST_LINUX)
        # Linux-specific settings
        # Enable position independent code for shared libraries
        set(CMAKE_POSITION_INDEPENDENT_CODE ON PARENT_SCOPE)
    elseif(NEXUS_HOST_MACOS)
        # macOS-specific settings
        set(CMAKE_MACOSX_RPATH ON PARENT_SCOPE)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Compiler Configuration
# ---------------------------------------------------------------------------

# Configure compiler flags for all supported compilers
# Sets warning levels, optimization flags, and preprocessor
#                  definitions for MSVC, GCC, Clang, and ARM toolchains
macro(nexus_configure_compiler_flags)
    if(NEXUS_COMPILER_MSVC)
        # MSVC compiler flags
        add_compile_options(
            /W4                 # Warning level 4
            /WX                 # Treat warnings as errors
            /wd4100             # Unreferenced formal parameter
            /wd4996             # Deprecated functions
            /permissive-        # Standards conformance
            /Zc:__cplusplus     # Enable __cplusplus macro
        )

        # Debug configuration
        add_compile_options($<$<CONFIG:Debug>:/Od>)
        add_compile_options($<$<CONFIG:Debug>:/Zi>)
        add_compile_options($<$<CONFIG:Debug>:/RTC1>)

        # Release configuration
        add_compile_options($<$<CONFIG:Release>:/O2>)
        add_compile_options($<$<CONFIG:Release>:/Ob2>)
        add_compile_options($<$<CONFIG:Release>:/Oi>)

        # Preprocessor definitions
        add_compile_definitions(
            $<$<CONFIG:Debug>:DEBUG>
            $<$<CONFIG:Debug>:_DEBUG>
            $<$<CONFIG:Release>:NDEBUG>
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_WARNINGS
        )

    elseif(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_GCC OR NEXUS_COMPILER_ARM_CLANG)
        # GCC/Clang compiler flags
        add_compile_options(
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wno-unused-parameter
            -ffunction-sections
            -fdata-sections
        )

        # Debug configuration
        add_compile_options($<$<CONFIG:Debug>:-Og>)
        add_compile_options($<$<CONFIG:Debug>:-g3>)

        # Release configuration
        add_compile_options($<$<CONFIG:Release>:-O2>)

        # Preprocessor definitions
        add_compile_definitions(
            $<$<CONFIG:Debug>:DEBUG>
            $<$<CONFIG:Release>:NDEBUG>
        )

        # Linker flags
        add_link_options(
            -Wl,--gc-sections
        )

        # Coverage flags
        if(NEXUS_ENABLE_COVERAGE)
            add_compile_options(--coverage -O0 -g)
            add_link_options(--coverage)

            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC)
                add_compile_options(-fprofile-arcs -ftest-coverage)
            endif()

            # Disable unused warnings for coverage build
            add_compile_options(
                -Wno-unused-function
                -Wno-unused-variable
            )
        endif()
    elseif(NEXUS_COMPILER_IAR)
        # IAR compiler flags
        add_compile_options(
            --strict
            --warnings_are_errors
        )

        # Debug configuration
        add_compile_options($<$<CONFIG:Debug>:-On>)
        add_compile_options($<$<CONFIG:Debug>:-r>)

        # Release configuration
        add_compile_options($<$<CONFIG:Release>:-Oh>)

        # Preprocessor definitions
        add_compile_definitions(
            $<$<CONFIG:Debug>:DEBUG>
            $<$<CONFIG:Release>:NDEBUG>
        )
    endif()
endmacro()

# ---------------------------------------------------------------------------
# Build Type Configuration
# ---------------------------------------------------------------------------

# Set default build type if not specified
# Defaults to Debug build if CMAKE_BUILD_TYPE is not set
macro(nexus_set_default_build_type)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
            "Choose the type of build (Debug, Release, MinSizeRel, RelWithDebInfo)"
            FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
    endif()
endmacro()

# ---------------------------------------------------------------------------
# Output Directory Configuration
# ---------------------------------------------------------------------------

# Configure output directories for build artifacts
# Sets up bin/ and lib/ directories for executables and
#                  libraries, handling both single and multi-config generators
macro(nexus_configure_output_directories)
    # Set output directories for single-configuration generators
    if(NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    else()
        # Multi-configuration generators (Visual Studio, Xcode)
        foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${CONFIG} CONFIG_UPPER)
            set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG})
            set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
            set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
        endforeach()
    endif()
endmacro()

# ---------------------------------------------------------------------------
# Platform Auto-Switching Logic
# ---------------------------------------------------------------------------

# Select appropriate toolchain for target platform
# PLATFORM: Target platform name
# Automatically selects the best toolchain for the platform
#                  Validates: Requirements 4.4
function(nexus_select_toolchain_for_platform PLATFORM)
    # Map platform to recommended toolchain
    if(PLATFORM MATCHES "^(stm32|stm32f4|stm32h7|stm32l4|gd32|nrf52)$")
        # ARM Cortex-M platforms
        if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
            # Check which ARM toolchains are available
            find_program(ARM_GCC_FOUND arm-none-eabi-gcc)
            find_program(ARM_CLANG_FOUND armclang)
            find_program(IAR_FOUND iccarm)

            if(ARM_GCC_FOUND)
                set(RECOMMENDED_TOOLCHAIN "arm-none-eabi-gcc" PARENT_SCOPE)
                message(STATUS "Recommended toolchain for ${PLATFORM}: ARM GCC")
            elseif(ARM_CLANG_FOUND)
                set(RECOMMENDED_TOOLCHAIN "armclang" PARENT_SCOPE)
                message(STATUS "Recommended toolchain for ${PLATFORM}: ARM Clang")
            elseif(IAR_FOUND)
                set(RECOMMENDED_TOOLCHAIN "iar-arm" PARENT_SCOPE)
                message(STATUS "Recommended toolchain for ${PLATFORM}: IAR")
            else()
                message(WARNING
                    "No ARM toolchain found for platform '${PLATFORM}'.\n"
                    "For embedded development, please install one of:\n"
                    "  - ARM GCC (arm-none-eabi-gcc)\n"
                    "  - ARM Clang (armclang)\n"
                    "  - IAR (iccarm)\n"
                    "Then specify toolchain with:\n"
                    "  cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/<toolchain>.cmake")
            endif()
        endif()
    elseif(PLATFORM STREQUAL "esp32")
        # ESP32 platform
        set(RECOMMENDED_TOOLCHAIN "xtensa-esp32" PARENT_SCOPE)
        message(STATUS "Recommended toolchain for ${PLATFORM}: Xtensa ESP32")
    elseif(PLATFORM STREQUAL "native")
        # Native platform - use system compiler
        set(RECOMMENDED_TOOLCHAIN "native" PARENT_SCOPE)
        message(STATUS "Using native system compiler for ${PLATFORM}")
    else()
        message(WARNING "Unknown platform '${PLATFORM}', cannot recommend toolchain")
    endif()
endfunction()

# Select linker script for target platform
# PLATFORM: Target platform name
# CHIP: Chip variant (optional)
# Automatically selects appropriate linker script
#                  Validates: Requirements 4.4
function(nexus_select_linker_script PLATFORM)
    # Parse arguments
    cmake_parse_arguments(ARG "" "CHIP;OUTPUT_VAR" "" ${ARGN})

    # Default output variable
    if(NOT ARG_OUTPUT_VAR)
        set(ARG_OUTPUT_VAR "NEXUS_LINKER_SCRIPT")
    endif()

    # Platform-specific linker script selection
    if(PLATFORM STREQUAL "stm32")
        if(ARG_CHIP)
            # Try to find chip-specific linker script
            set(LINKER_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/platforms/stm32/linker/${ARG_CHIP}.ld")
            if(EXISTS ${LINKER_SCRIPT_PATH})
                set(${ARG_OUTPUT_VAR} ${LINKER_SCRIPT_PATH} PARENT_SCOPE)
                message(STATUS "Selected linker script: ${LINKER_SCRIPT_PATH}")
            else()
                message(WARNING "Linker script not found for chip '${ARG_CHIP}'")
            endif()
        else()
            message(WARNING "No chip variant specified for STM32 platform")
        endif()
    elseif(PLATFORM STREQUAL "native")
        # Native platform doesn't need linker script
        set(${ARG_OUTPUT_VAR} "" PARENT_SCOPE)
    else()
        message(VERBOSE "No linker script selection for platform '${PLATFORM}'")
    endif()
endfunction()

# Set platform-specific compile flags
# PLATFORM: Target platform name
# Configures platform-specific compiler definitions and flags
#                  Validates: Requirements 4.4
function(nexus_set_platform_compile_flags PLATFORM)
    # Platform-specific definitions
    if(PLATFORM MATCHES "^(stm32|stm32f4|stm32h7|stm32l4)$")
        # STM32-specific definitions
        if(DEFINED CONFIG_STM32_CHIP)
            add_compile_definitions(${CONFIG_STM32_CHIP})
        endif()
        add_compile_definitions(USE_HAL_DRIVER)

        # Set ARM math definitions based on CPU
        if(NEXUS_CPU_ARCH MATCHES "cortex-m4")
            add_compile_definitions(ARM_MATH_CM4)
        elseif(NEXUS_CPU_ARCH MATCHES "cortex-m7")
            add_compile_definitions(ARM_MATH_CM7)
        elseif(NEXUS_CPU_ARCH MATCHES "cortex-m3")
            add_compile_definitions(ARM_MATH_CM3)
        endif()

    elseif(PLATFORM STREQUAL "gd32")
        # GD32-specific definitions
        if(DEFINED CONFIG_GD32_CHIP)
            add_compile_definitions(${CONFIG_GD32_CHIP})
        endif()

    elseif(PLATFORM STREQUAL "nrf52")
        # nRF52-specific definitions
        if(DEFINED CONFIG_NRF52_CHIP)
            add_compile_definitions(${CONFIG_NRF52_CHIP})
        endif()
        add_compile_definitions(NRF52)

    elseif(PLATFORM STREQUAL "esp32")
        # ESP32-specific definitions
        add_compile_definitions(ESP_PLATFORM)

    elseif(PLATFORM STREQUAL "native")
        # Native platform definitions
        add_compile_definitions(NEXUS_PLATFORM_NATIVE)
    endif()

    # Common embedded platform definitions
    if(PLATFORM MATCHES "^(stm32|stm32f4|stm32h7|stm32l4|gd32|nrf52|esp32)$")
        add_compile_definitions(NEXUS_PLATFORM_EMBEDDED)
    endif()
endfunction()

# Auto-configure platform based on Kconfig
# Main entry point for platform auto-switching
#                  Selects toolchain, linker script, and compile flags
#                  Validates: Requirements 4.4
macro(nexus_auto_configure_platform)
    # Get target platform from detection
    if(NOT DEFINED NEXUS_TARGET_PLATFORM)
        nexus_detect_target_platform()
    endif()

    # Select appropriate toolchain if not already set
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND NOT CMAKE_CROSSCOMPILING)
        nexus_select_toolchain_for_platform(${NEXUS_TARGET_PLATFORM})

        if(DEFINED RECOMMENDED_TOOLCHAIN)
            message(STATUS "")
            message(STATUS "Recommended toolchain: ${RECOMMENDED_TOOLCHAIN}")
            message(STATUS "To use this toolchain, configure with:")
            message(STATUS "  cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/${RECOMMENDED_TOOLCHAIN}.cmake ..")
            message(STATUS "")
        endif()
    endif()

    # Select linker script based on platform and chip
    if(DEFINED CONFIG_STM32_CHIP)
        nexus_select_linker_script(${NEXUS_TARGET_PLATFORM}
            CHIP ${CONFIG_STM32_CHIP}
            OUTPUT_VAR NEXUS_LINKER_SCRIPT)
    endif()

    # Set platform-specific compile flags
    nexus_set_platform_compile_flags(${NEXUS_TARGET_PLATFORM})

    # Print platform configuration
    message(STATUS "Platform auto-configuration:")
    message(STATUS "  Target Platform: ${NEXUS_TARGET_PLATFORM}")
    if(DEFINED NEXUS_LINKER_SCRIPT)
        message(STATUS "  Linker Script:   ${NEXUS_LINKER_SCRIPT}")
    endif()
endmacro()

# ---------------------------------------------------------------------------
# End of NexusPlatform.cmake
# ---------------------------------------------------------------------------
