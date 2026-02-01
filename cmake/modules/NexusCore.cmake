##############################################################################
# NexusCore.cmake - Core Platform and Toolchain Module
##############################################################################
#
# NexusCore.cmake
# Core platform detection, toolchain abstraction, and compiler configuration
# Author: Nexus Team
#
# This module provides:
# - Platform detection (host and target)
# - Compiler detection and configuration
# - Toolchain abstraction layer
# - Build type configuration
# - Output directory configuration
#
# Consolidated from:
# - NexusPlatform.cmake
# - NexusToolchain.cmake
# - Parts of NexusHelpers.cmake
#
##############################################################################

include_guard(GLOBAL)

# Load helper functions for error handling
include(NexusHelpers)

##############################################################################
# Platform Detection
##############################################################################

#
# Detect host platform (Windows/Linux/macOS)
#
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

#
# Detect target platform from Kconfig or CMake variables
#
function(nexus_detect_target_platform)
    # Priority 1: Check Kconfig configuration
    if(DEFINED CONFIG_PLATFORM_NAME)
        set(PLATFORM_NAME ${CONFIG_PLATFORM_NAME})
        message(VERBOSE "Target platform from Kconfig: ${PLATFORM_NAME}")
    # Priority 2: Check CMake variable
    elseif(DEFINED NEXUS_TARGET_PLATFORM)
        set(PLATFORM_NAME ${NEXUS_TARGET_PLATFORM})
        message(VERBOSE "Target platform set via CMake: ${PLATFORM_NAME}")
    # Priority 3: Check NEXUS_PLATFORM variable (legacy)
    elseif(DEFINED NEXUS_PLATFORM)
        set(PLATFORM_NAME ${NEXUS_PLATFORM})
        message(VERBOSE "Target platform from NEXUS_PLATFORM: ${PLATFORM_NAME}")
    # Priority 4: Check individual platform flags
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
    # Priority 5: Check CMAKE_SYSTEM_NAME
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Generic")
        if(CMAKE_C_COMPILER MATCHES "arm-none-eabi")
            set(PLATFORM_NAME "arm-embedded")
            message(VERBOSE "Detected ARM embedded platform from toolchain")
        else()
            set(PLATFORM_NAME "generic")
            message(WARNING "Generic platform detected, specific platform unknown")
        endif()
    else()
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

##############################################################################
# Compiler Detection
##############################################################################

#
# Detect compiler type and version
#
function(nexus_detect_compiler)
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        set(NEXUS_COMPILER_MSVC TRUE PARENT_SCOPE)
        set(NEXUS_COMPILER_NAME "MSVC" PARENT_SCOPE)
        set(NEXUS_COMPILER_FAMILY "msvc" PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_NAME "msvc" PARENT_SCOPE)
    elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
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

    set(NEXUS_COMPILER_VERSION "${CMAKE_C_COMPILER_VERSION}" PARENT_SCOPE)

    if(CMAKE_CROSSCOMPILING)
        set(NEXUS_CROSS_COMPILING TRUE PARENT_SCOPE)
    else()
        set(NEXUS_CROSS_COMPILING FALSE PARENT_SCOPE)
    endif()

    message(VERBOSE "Compiler: ${NEXUS_COMPILER_NAME} ${CMAKE_C_COMPILER_VERSION}")
endfunction()

#
# Detect toolchain from compiler and platform
#
function(nexus_detect_toolchain)
    if(DEFINED CONFIG_TOOLCHAIN_NAME)
        set(NEXUS_TOOLCHAIN_NAME "${CONFIG_TOOLCHAIN_NAME}" CACHE INTERNAL "" FORCE)
        message(VERBOSE "Toolchain from Kconfig: ${CONFIG_TOOLCHAIN_NAME}")
        return()
    endif()

    if(DEFINED NEXUS_TOOLCHAIN_NAME AND NEXUS_TOOLCHAIN_NAME)
        message(VERBOSE "Toolchain already cached: ${NEXUS_TOOLCHAIN_NAME}")
        return()
    endif()

    if(NEXUS_COMPILER_ARM_GCC)
        set(NEXUS_TOOLCHAIN_NAME "arm-none-eabi-gcc" CACHE INTERNAL "" FORCE)
    elseif(NEXUS_COMPILER_ARM_CLANG)
        set(NEXUS_TOOLCHAIN_NAME "armclang" CACHE INTERNAL "" FORCE)
    elseif(NEXUS_COMPILER_IAR)
        set(NEXUS_TOOLCHAIN_NAME "iar-arm" CACHE INTERNAL "" FORCE)
    elseif(NEXUS_COMPILER_MSVC)
        set(NEXUS_TOOLCHAIN_NAME "msvc" CACHE INTERNAL "" FORCE)
    elseif(NEXUS_COMPILER_GCC)
        set(NEXUS_TOOLCHAIN_NAME "gcc" CACHE INTERNAL "" FORCE)
    elseif(NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG)
        set(NEXUS_TOOLCHAIN_NAME "clang" CACHE INTERNAL "" FORCE)
    else()
        set(NEXUS_TOOLCHAIN_NAME "unknown" CACHE INTERNAL "" FORCE)
    endif()

    message(VERBOSE "Detected toolchain: ${NEXUS_TOOLCHAIN_NAME}")
endfunction()

#
# Detect CMake generator
#
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

##############################################################################
# Platform Configuration
##############################################################################

#
# Configure platform-specific settings
#
macro(nexus_configure_platform)
    nexus_detect_host_platform()
    nexus_detect_target_platform()
    nexus_detect_compiler()
    nexus_detect_toolchain()
    nexus_detect_generator()

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

    nexus_validate_platform_config()
    nexus_set_platform_defaults()
endmacro()

#
# Validate platform configuration
#
function(nexus_validate_platform_config)
    if(NEXUS_TARGET_EMBEDDED)
        if(NOT NEXUS_CROSS_COMPILING)
            message(WARNING
                "Embedded target platform '${NEXUS_TARGET_PLATFORM}' detected "
                "but not cross-compiling. This may cause build issues.")
        endif()

        if(NEXUS_TARGET_PLATFORM MATCHES "^(stm32|gd32|nrf52)$")
            if(NOT (NEXUS_COMPILER_ARM_GCC OR NEXUS_COMPILER_ARM_CLANG OR NEXUS_COMPILER_IAR))
                message(FATAL_ERROR
                    "ARM platform '${NEXUS_TARGET_PLATFORM}' requires ARM toolchain.\n"
                    "Detected compiler: ${NEXUS_COMPILER_NAME}\n"
                    "Please use: arm-none-eabi-gcc, armclang, or iccarm")
            endif()
        endif()
    endif()

    if(NEXUS_TARGET_NATIVE)
        if(NEXUS_CROSS_COMPILING)
            message(WARNING
                "Native platform selected but cross-compiling is enabled.")
        endif()
    endif()
endfunction()

#
# Set platform-specific defaults
#
function(nexus_set_platform_defaults)
    if(NEXUS_HOST_WINDOWS)
        if(NEXUS_COMPILER_MSVC)
            add_compile_options(/MP)
            if(BUILD_SHARED_LIBS)
                set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" PARENT_SCOPE)
            else()
                set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" PARENT_SCOPE)
            endif()
        endif()
    elseif(NEXUS_HOST_LINUX)
        set(CMAKE_POSITION_INDEPENDENT_CODE ON PARENT_SCOPE)
    elseif(NEXUS_HOST_MACOS)
        set(CMAKE_MACOSX_RPATH ON PARENT_SCOPE)
    endif()
endfunction()

##############################################################################
# Compiler Configuration
##############################################################################

#
# Configure compiler flags
#
macro(nexus_configure_compiler_flags)
    if(NEXUS_COMPILER_MSVC)
        add_compile_options(
            /W4 /WX /wd4100 /wd4996
            /permissive- /Zc:__cplusplus
        )
        # Debug configuration
        add_compile_options($<$<CONFIG:Debug>:/Od>)
        add_compile_options($<$<CONFIG:Debug>:/Zi>)
        add_compile_options($<$<CONFIG:Debug>:/RTC1>)
        # Release configuration
        add_compile_options($<$<CONFIG:Release>:/O2>)
        add_compile_options($<$<CONFIG:Release>:/Ob2>)
        add_compile_options($<$<CONFIG:Release>:/Oi>)
        add_compile_definitions(
            $<$<CONFIG:Debug>:DEBUG>
            $<$<CONFIG:Debug>:_DEBUG>
            $<$<CONFIG:Release>:NDEBUG>
            _CRT_SECURE_NO_WARNINGS
        )
    elseif(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_GCC)
        add_compile_options(
            -Wall -Wextra -Wpedantic -Werror
            -Wno-unused-parameter
            -ffunction-sections -fdata-sections
        )
        add_compile_options($<$<CONFIG:Debug>:-Og>)
        add_compile_options($<$<CONFIG:Debug>:-g3>)
        add_compile_options($<$<CONFIG:Release>:-O2>)
        add_compile_definitions(
            $<$<CONFIG:Debug>:DEBUG>
            $<$<CONFIG:Release>:NDEBUG>
        )
        add_link_options(-Wl,--gc-sections)

        if(NEXUS_ENABLE_COVERAGE)
            add_compile_options(--coverage -O0 -g)
            add_link_options(--coverage)
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC)
                add_compile_options(-fprofile-arcs -ftest-coverage)
            endif()
        endif()
    elseif(NEXUS_COMPILER_ARM_CLANG)
        # ARM Clang specific flags
        add_compile_options(
            -Wall -Wextra -Wpedantic -Werror
            -Wno-unused-parameter
            -ffunction-sections -fdata-sections
        )
        # Set optimization and debug flags based on build type
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_compile_options(-O1 -g)
            add_compile_definitions(DEBUG)
        elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
            add_compile_options(-O2)
            add_compile_definitions(NDEBUG)
        endif()
    elseif(NEXUS_COMPILER_IAR)
        add_compile_options(--strict --warnings_are_errors)
        add_compile_options($<$<CONFIG:Debug>:-On -r>)
        add_compile_options($<$<CONFIG:Release>:-Oh>)
        add_compile_definitions(
            $<$<CONFIG:Debug>:DEBUG>
            $<$<CONFIG:Release>:NDEBUG>
        )
    endif()
endmacro()

##############################################################################
# Build Type Configuration
##############################################################################

#
# Set default build type
#
macro(nexus_set_default_build_type)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
            "Choose the type of build (Debug, Release, MinSizeRel, RelWithDebInfo)"
            FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
    endif()
endmacro()

##############################################################################
# Output Directory Configuration
##############################################################################

#
# Configure output directories
#
macro(nexus_configure_output_directories)
    if(NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    else()
        foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${CONFIG} CONFIG_UPPER)
            set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG})
            set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
            set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
        endforeach()
    endif()
endmacro()

##############################################################################
# Toolchain Abstraction Layer
##############################################################################

#
# Add startup file with toolchain-specific handling
# Arguments:
#   TARGET: Target to add startup file to
#   STARTUP_FILE: Path to startup file
#
function(nexus_add_startup_file TARGET STARTUP_FILE)
    if(NOT EXISTS ${STARTUP_FILE})
        nexus_fatal_error("Startup file not found: ${STARTUP_FILE}")
    endif()

    if(NEXUS_COMPILER_ARM_GCC)
        # ARM GCC: Direct assembly compilation
        target_sources(${TARGET} PRIVATE ${STARTUP_FILE})
        nexus_log(VERBOSE "Added startup file (ARM GCC): ${STARTUP_FILE}")

    elseif(NEXUS_COMPILER_ARM_CLANG)
        # ARM Clang: Use armasm for assembly files
        # armasm doesn't accept C compiler flags, so we use custom command
        get_filename_component(STARTUP_NAME ${STARTUP_FILE} NAME_WE)
        set(STARTUP_OBJ "${CMAKE_CURRENT_BINARY_DIR}/${STARTUP_NAME}.o")

        # Get CPU flags from Kconfig or default
        if(DEFINED NEXUS_CPU_ARCH)
            set(CPU_FLAG "--cpu=${NEXUS_CPU_ARCH}")
        else()
            set(CPU_FLAG "--cpu=Cortex-M4.fp")
        endif()

        add_custom_command(
            OUTPUT ${STARTUP_OBJ}
            COMMAND ${CMAKE_ASM_COMPILER} ${CPU_FLAG} -g
                    -o ${STARTUP_OBJ}
                    ${STARTUP_FILE}
            DEPENDS ${STARTUP_FILE}
            COMMENT "Assembling ${STARTUP_NAME} with armasm"
            VERBATIM
        )

        target_sources(${TARGET} PRIVATE ${STARTUP_OBJ})
        nexus_log(VERBOSE "Added startup file (ARM Clang/armasm): ${STARTUP_FILE}")

    elseif(NEXUS_COMPILER_IAR)
        # IAR: Direct assembly compilation
        target_sources(${TARGET} PRIVATE ${STARTUP_FILE})
        nexus_log(VERBOSE "Added startup file (IAR): ${STARTUP_FILE}")

    else()
        nexus_warning("Unknown compiler for startup file: ${NEXUS_COMPILER_NAME}")
        target_sources(${TARGET} PRIVATE ${STARTUP_FILE})
    endif()

    get_filename_component(STARTUP_NAME ${STARTUP_FILE} NAME)
    message(STATUS "Startup file: ${STARTUP_NAME}")
endfunction()

#
# Configure linker script with automatic toolchain detection
# Arguments:
#   TARGET: Target to configure linker script for
#   SCRIPT_BASE_PATH: Base path to linker scripts (without toolchain subdirectory)
#   SCRIPT_NAME: Script name (without extension, optional, defaults to TARGET name)
#
function(nexus_configure_linker_script TARGET SCRIPT_BASE_PATH)
    cmake_parse_arguments(ARG "" "SCRIPT_NAME" "" ${ARGN})

    if(NOT ARG_SCRIPT_NAME)
        set(ARG_SCRIPT_NAME ${TARGET})
    endif()

    # Determine toolchain-specific subdirectory and extension
    if(NEXUS_COMPILER_ARM_GCC)
        set(SCRIPT_EXT ".ld")
        set(SCRIPT_DIR "gcc")
    elseif(NEXUS_COMPILER_ARM_CLANG)
        set(SCRIPT_EXT ".sct")
        set(SCRIPT_DIR "arm")
    elseif(NEXUS_COMPILER_IAR)
        set(SCRIPT_EXT ".icf")
        set(SCRIPT_DIR "iar")
    else()
        nexus_fatal_error("Unsupported compiler for linker script: ${NEXUS_COMPILER_NAME}")
    endif()

    # Construct full linker script path
    set(LINKER_SCRIPT "${SCRIPT_BASE_PATH}/${SCRIPT_DIR}/${ARG_SCRIPT_NAME}${SCRIPT_EXT}")

    # Validate linker script exists
    nexus_require_file(${LINKER_SCRIPT}
        "Linker script not found for ${TARGET}\n"
        "Expected: ${LINKER_SCRIPT}\n"
        "Please ensure linker scripts are present for all supported toolchains"
    )

    # Apply linker script
    nexus_set_linker_script(${TARGET} ${LINKER_SCRIPT})

    get_filename_component(SCRIPT_NAME ${LINKER_SCRIPT} NAME)
    message(STATUS "Linker script: ${SCRIPT_NAME}")
endfunction()

#
# Generate binary file from ELF
#
# Set linker script
#
function(nexus_set_linker_script TARGET SCRIPT)
    if(NOT EXISTS ${SCRIPT})
        message(WARNING "Linker script not found: ${SCRIPT}")
        return()
    endif()

    if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC)
        target_link_options(${TARGET} PRIVATE -T${SCRIPT})
    elseif(NEXUS_COMPILER_ARM_CLANG)
        target_link_options(${TARGET} PRIVATE --scatter=${SCRIPT})
    elseif(NEXUS_COMPILER_IAR)
        target_link_options(${TARGET} PRIVATE --config ${SCRIPT})
    endif()

    message(STATUS "Linker script for ${TARGET}: ${SCRIPT}")
endfunction()

message(STATUS "NexusCore module loaded")

##############################################################################
# End of NexusCore.cmake
##############################################################################
