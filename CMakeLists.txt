#-----------------------------------------------------------------------------
# Nexus Embedded Platform - Root CMake Configuration
#-----------------------------------------------------------------------------
# Nexus Embedded Platform
# Root CMake configuration file
# Author: Nexus Team
# Version: 0.1.0
#
# This is the main CMake configuration file for the Nexus embedded platform.
# It orchestrates the build process through streamlined phases:
#   1. Configuration Management (Kconfig)
#   2. Core Platform Setup (Platform/Toolchain/Compiler)
#   3. Build Optimization (Cache/Performance)
#   4. Quality Assurance (Tests/Diagnostics)
#   5. Build Targets (Subdirectories)
#
#-----------------------------------------------------------------------------

# CMake 3.21+ required for ARM Clang toolchain support (Policy CMP0123)
cmake_minimum_required(VERSION 3.21)

#-----------------------------------------------------------------------------
# Project Definition
#-----------------------------------------------------------------------------

project(nexus
    VERSION 0.1.0
    DESCRIPTION "Nexus Embedded Software Development Platform"
    LANGUAGES C CXX ASM
)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not allowed.\n"
        "Please create a separate build directory:\n"
        "  mkdir build && cd build && cmake .."
    )
endif()

#-----------------------------------------------------------------------------
# Build Options
#-----------------------------------------------------------------------------

option(NEXUS_BUILD_TESTS "Build unit tests" ON)
option(NEXUS_BUILD_EXAMPLES "Build example applications" ON)
option(NEXUS_BUILD_DOCS "Build documentation" OFF)
option(NEXUS_ENABLE_COVERAGE "Enable code coverage" OFF)
option(NEXUS_ENABLE_CACHE "Enable build cache" ON)

# Platform selection (can be overridden by Kconfig)
# These are fallback values if Kconfig is not configured
set(NEXUS_PLATFORM "native" CACHE STRING "Target platform (fallback if Kconfig not set)")
set_property(CACHE NEXUS_PLATFORM PROPERTY STRINGS
    native stm32 stm32f4 stm32h7 stm32l4 esp32 nrf52 gd32
)

# OSAL backend selection (can be overridden by Kconfig)
# These are fallback values if Kconfig is not configured
set(NEXUS_OSAL_BACKEND "baremetal" CACHE STRING "OSAL backend (fallback if Kconfig not set)")
set_property(CACHE NEXUS_OSAL_BACKEND PROPERTY STRINGS
    baremetal freertos rtthread zephyr
)

#-----------------------------------------------------------------------------
# Global Settings
#-----------------------------------------------------------------------------

# C/C++ Standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include CMake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/application)

#-----------------------------------------------------------------------------
# Find Required Tools
#-----------------------------------------------------------------------------

# Find Python for configuration generation (needed early)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
message(STATUS "Python: ${Python3_EXECUTABLE}")

#-----------------------------------------------------------------------------
# Phase 1: Configuration Management
#-----------------------------------------------------------------------------
# Load and process Kconfig configuration, generate configuration header,
# and apply configuration to CMake variables.
#-----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "=== Configuration Management ===")

# Define configuration file paths
set(NEXUS_KCONFIG_FILE "${CMAKE_SOURCE_DIR}/Kconfig"
    CACHE FILEPATH "Root Kconfig file")
set(NEXUS_CONFIG_FILE "${CMAKE_SOURCE_DIR}/.config"
    CACHE FILEPATH "Configuration file")
set(NEXUS_CONFIG_HEADER "${CMAKE_SOURCE_DIR}/nexus_config.h"
    CACHE FILEPATH "Generated configuration header")

# Load NexusConfig module (Kconfig + Dependency management)
include(NexusConfig)

# Generate configuration header from Kconfig
nexus_load_kconfig(
    KCONFIG_FILE ${NEXUS_KCONFIG_FILE}
    CONFIG_FILE ${NEXUS_CONFIG_FILE}
    OUTPUT_HEADER ${NEXUS_CONFIG_HEADER}
)

# Validate Kconfig configuration
nexus_validate_kconfig(CONFIG_FILE ${NEXUS_CONFIG_FILE})

# Load Kconfig configuration into CMake variables
load_kconfig(${NEXUS_CONFIG_FILE})

# Apply Kconfig configuration to CMake variables
apply_kconfig_to_cmake()

# Override OSAL backend if configured in Kconfig
if(DEFINED CONFIG_OSAL_BACKEND_NAME)
    set(NEXUS_OSAL_BACKEND "${CONFIG_OSAL_BACKEND_NAME}" CACHE STRING "OSAL backend from Kconfig" FORCE)
endif()

#-----------------------------------------------------------------------------
# Phase 2: Core Platform Setup
#-----------------------------------------------------------------------------
# Detect and configure platform, toolchain, and compiler settings.
#-----------------------------------------------------------------------------

message(STATUS "=== Core Platform Setup ===")

# Load NexusCore module (Platform + Toolchain + Compiler)
include(NexusCore)

# Configure platform (detects host, target, compiler, toolchain, generator)
nexus_configure_platform()

# Set default build type
nexus_set_default_build_type()

# Configure output directories
nexus_configure_output_directories()

# Apply Kconfig compiler and linker configurations
# This must be done AFTER platform detection to ensure toolchain variables are set
if(EXISTS ${CMAKE_SOURCE_DIR}/cmake/modules/NexusCompilerConfig.cmake)
    include(NexusCompilerConfig)
    nexus_apply_compiler_kconfig()
    nexus_apply_linker_kconfig()
endif()

# Configure compiler flags (applies default/baseline flags)
# Kconfig flags are already applied above, this adds any additional required flags
nexus_configure_compiler_flags()

#-----------------------------------------------------------------------------
# Phase 3: Dependency Management
#-----------------------------------------------------------------------------

message(STATUS "=== Dependency Management ===")

# Load dependency lock file if exists
if(EXISTS ${CMAKE_SOURCE_DIR}/nexus.lock)
    nexus_load_lockfile()
    message(STATUS "Dependency lock file: Loaded")
endif()

#-----------------------------------------------------------------------------
# Phase 4: Build Optimization
#-----------------------------------------------------------------------------

message(STATUS "=== Build Optimization ===")

# Load NexusOptimization module (Cache + Performance + Resource)
include(NexusOptimization)

# Enable build cache if configured
if(NEXUS_ENABLE_CACHE OR DEFINED ENV{NEXUS_ENABLE_CACHE})
    nexus_enable_cache(
        LOCAL_DIR "${CMAKE_BINARY_DIR}/.cache"
        MAX_SIZE 4096  # Increased to 4GB for better cache utilization
    )
    message(STATUS "Build cache: Enabled")
else()
    message(STATUS "Build cache: Disabled")
endif()

# Configure parallel build based on available resources
nexus_configure_parallel_build()

#-----------------------------------------------------------------------------
# Phase 5: Quality Assurance Setup
#-----------------------------------------------------------------------------

message(STATUS "=== Quality Assurance ===")

# Load NexusQuality module (Tests + Diagnostics + Reproducible builds)
include(NexusQuality)

# Initialize test framework if tests are enabled
if(NEXUS_BUILD_TESTS AND NEXUS_PLATFORM STREQUAL "native")
    nexus_test_init_gtest()
endif()

# Enable reproducible builds if configured
if(DEFINED CONFIG_REPRODUCIBLE_BUILD AND CONFIG_REPRODUCIBLE_BUILD)
    nexus_init_reproducible_build()
endif()

#-----------------------------------------------------------------------------
# Phase 6: Extension System (Optional)
#-----------------------------------------------------------------------------

# Load NexusExtension module (Plugin system + Build isolation)
include(NexusExtension)

# Load plugins if any are configured
if(EXISTS ${CMAKE_SOURCE_DIR}/cmake/plugins)
    file(GLOB PLUGIN_FILES ${CMAKE_SOURCE_DIR}/cmake/plugins/*.cmake)
    foreach(PLUGIN_FILE ${PLUGIN_FILES})
        include(${PLUGIN_FILE})
    endforeach()
endif()

#-----------------------------------------------------------------------------
# Phase 7: FreeRTOS Integration (if selected)
#-----------------------------------------------------------------------------

if(NEXUS_OSAL_BACKEND STREQUAL "freertos")
    message(STATUS "")
    message(STATUS "=== Phase 7: FreeRTOS Integration ===")
    include(FreeRTOS)
endif()

#-----------------------------------------------------------------------------
# Phase 8: Build Targets (Subdirectories)
#-----------------------------------------------------------------------------

message(STATUS "=== Build Targets ===")

# Load NexusBuild module for target creation helpers
include(NexusBuild)

# Core libraries
add_subdirectory(hal)
add_subdirectory(osal)

# Framework layer
add_subdirectory(framework)

# Middleware (when available)
if(EXISTS ${CMAKE_SOURCE_DIR}/middleware)
    add_subdirectory(middleware)
endif()

# Components (when available)
if(EXISTS ${CMAKE_SOURCE_DIR}/components)
    add_subdirectory(components)
endif()

# Platform-specific code
add_subdirectory(platforms)

# Example applications
if(NEXUS_BUILD_EXAMPLES)
    add_subdirectory(applications)
endif()

# Tests (only for native platform)
if(NEXUS_BUILD_TESTS AND NEXUS_PLATFORM STREQUAL "native")
    enable_testing()
    add_subdirectory(tests)

    # Configure parallel test execution
    if(NEXUS_TEST_ENABLE_PARALLEL)
        nexus_test_configure_parallel()
    endif()
endif()

# Documentation
if(NEXUS_BUILD_DOCS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/docs)
        add_subdirectory(docs)
    endif()
endif()

#-----------------------------------------------------------------------------
# Build Summary
#-----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "========================================")
message(STATUS "=== Nexus Build Configuration ===")
message(STATUS "========================================")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Platform:       ${NEXUS_PLATFORM}")
message(STATUS "  OSAL Backend:   ${NEXUS_OSAL_BACKEND}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler:       ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Generator:      ${CMAKE_GENERATOR}")
message(STATUS "----------------------------------------")
message(STATUS "  Tests:          ${NEXUS_BUILD_TESTS}")
message(STATUS "  Examples:       ${NEXUS_BUILD_EXAMPLES}")
message(STATUS "  Documentation:  ${NEXUS_BUILD_DOCS}")
message(STATUS "  Coverage:       ${NEXUS_ENABLE_COVERAGE}")
message(STATUS "  Build Cache:    ${NEXUS_ENABLE_CACHE}")
message(STATUS "----------------------------------------")
message(STATUS "  Parallel Jobs:  ${PARALLEL_JOBS}")
message(STATUS "  Binary Dir:     ${CMAKE_BINARY_DIR}")

# Print Kconfig-specific configuration if available
if(DEFINED CONFIG_PLATFORM_NAME)
    message(STATUS "----------------------------------------")
    message(STATUS "  Kconfig Configuration:")

    # Toolchain info
    if(DEFINED NEXUS_TOOLCHAIN_NAME)
        message(STATUS "    Toolchain:    ${NEXUS_TOOLCHAIN_NAME}")
    endif()

    # ARM CPU configuration
    if(DEFINED NEXUS_CPU_ARCH)
        message(STATUS "    CPU Arch:     ${NEXUS_CPU_ARCH}")
    endif()

    if(DEFINED NEXUS_FPU_TYPE AND NEXUS_FPU_TYPE)
        message(STATUS "    FPU Type:     ${NEXUS_FPU_TYPE}")
    endif()

    if(DEFINED NEXUS_FLOAT_ABI)
        message(STATUS "    Float ABI:    ${NEXUS_FLOAT_ABI}")
    endif()

    # Linker script
    if(DEFINED NEXUS_LINKER_SCRIPT AND NEXUS_LINKER_SCRIPT)
        get_filename_component(LINKER_NAME ${NEXUS_LINKER_SCRIPT} NAME)
        message(STATUS "    Linker:       ${LINKER_NAME}")
    endif()

    # Memory configuration
    if(DEFINED NEXUS_STACK_SIZE)
        message(STATUS "    Stack Size:   ${NEXUS_STACK_SIZE}")
    endif()

    if(DEFINED NEXUS_HEAP_SIZE)
        message(STATUS "    Heap Size:    ${NEXUS_HEAP_SIZE}")
    endif()
endif()

message(STATUS "----------------------------------------")
message(STATUS "  Compiler/Linker Flags:")

# C Compiler flags - compact format
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_COMBINED_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(_COMBINED_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELEASE}")
else()
    set(_COMBINED_C_FLAGS "${CMAKE_C_FLAGS}")
endif()
string(STRIP "${_COMBINED_C_FLAGS}" _COMBINED_C_FLAGS)
message(STATUS "    C (${CMAKE_BUILD_TYPE}):  ${_COMBINED_C_FLAGS}")

# Assembler flags - compact format
if(NEXUS_CPU_ARCH)
    # Map to assembler format
    if(NEXUS_CPU_ARCH STREQUAL "cortex-m4")
        set(_asm_display "--cpu=Cortex-M4")
    elseif(NEXUS_CPU_ARCH STREQUAL "cortex-m7")
        set(_asm_display "--cpu=Cortex-M7")
    elseif(NEXUS_CPU_ARCH STREQUAL "cortex-m3")
        set(_asm_display "--cpu=Cortex-M3")
    elseif(NEXUS_CPU_ARCH STREQUAL "cortex-m0")
        set(_asm_display "--cpu=Cortex-M0")
    elseif(NEXUS_CPU_ARCH STREQUAL "cortex-m0plus")
        set(_asm_display "--cpu=Cortex-M0plus")
    endif()

    if(NEXUS_FPU_TYPE)
        if(NEXUS_FPU_TYPE STREQUAL "fpv4-sp-d16")
            string(APPEND _asm_display " --fpu=FPv4-SP")
        elseif(NEXUS_FPU_TYPE STREQUAL "fpv5-d16")
            string(APPEND _asm_display " --fpu=FPv5-D16")
        elseif(NEXUS_FPU_TYPE STREQUAL "fpv5-sp-d16")
            string(APPEND _asm_display " --fpu=FPv5-SP")
        endif()
    endif()

    # Add MicroLIB info if applicable
    if(NEXUS_COMPILER_ARM_CLANG)
        if(DEFINED CONFIG_COMPILER_USE_MICROLIB AND CONFIG_COMPILER_USE_MICROLIB)
            string(APPEND _asm_display " --pd \"__MICROLIB SETA 1\"")
        endif()
    endif()

    message(STATUS "    ASM:      ${_asm_display}")
endif()

# Linker flags - compact format
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_COMBINED_LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(_COMBINED_LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
else()
    set(_COMBINED_LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endif()
string(STRIP "${_COMBINED_LINK_FLAGS}" _COMBINED_LINK_FLAGS)
message(STATUS "    Link (${CMAKE_BUILD_TYPE}): ${_COMBINED_LINK_FLAGS}")

# Additional Kconfig flags (applied via add_compile_options)
set(_KCONFIG_FLAGS "")

# C/C++ Standard (from CMAKE_C_STANDARD/CMAKE_CXX_STANDARD)
if(CMAKE_C_STANDARD)
    list(APPEND _KCONFIG_FLAGS "-std=c${CMAKE_C_STANDARD}")
endif()

if(DEFINED CONFIG_COMPILER_WARNING_LEVEL)
    if(CONFIG_COMPILER_WARNING_LEVEL STREQUAL "STANDARD")
        list(APPEND _KCONFIG_FLAGS "-Wall" "-Wextra")
    elseif(CONFIG_COMPILER_WARNING_LEVEL STREQUAL "PEDANTIC")
        list(APPEND _KCONFIG_FLAGS "-Wall" "-Wextra" "-Wpedantic")
    endif()
endif()
if(DEFINED CONFIG_COMPILER_DEBUG_FORMAT AND CONFIG_COMPILER_DEBUG_FORMAT STREQUAL "DWARF4")
    list(APPEND _KCONFIG_FLAGS "-gdwarf-4" "-g3")
endif()
if(DEFINED CONFIG_COMPILER_UNSIGNED_CHAR AND CONFIG_COMPILER_UNSIGNED_CHAR)
    list(APPEND _KCONFIG_FLAGS "-funsigned-char")
endif()
if(DEFINED CONFIG_COMPILER_SHORT_ENUMS AND CONFIG_COMPILER_SHORT_ENUMS)
    list(APPEND _KCONFIG_FLAGS "-fshort-enums")
endif()
if(DEFINED CONFIG_COMPILER_FUNCTION_SECTIONS AND CONFIG_COMPILER_FUNCTION_SECTIONS)
    list(APPEND _KCONFIG_FLAGS "-ffunction-sections" "-fdata-sections")
endif()

if(_KCONFIG_FLAGS)
    string(JOIN " " _KCONFIG_FLAGS_STR ${_KCONFIG_FLAGS})
    message(STATUS "    Kconfig:  ${_KCONFIG_FLAGS_STR}")
endif()

message(STATUS "========================================")
message(STATUS "")
message(STATUS "Note: Run 'cmake --build --preset <preset> --verbose' to see")
message(STATUS "      complete compilation commands with all flags.")
message(STATUS "")

#-----------------------------------------------------------------------------
# End of Root CMake Configuration
#-----------------------------------------------------------------------------

