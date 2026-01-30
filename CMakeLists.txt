##############################################################################
# Nexus Embedded Platform - Root CMake Configuration
##############################################################################
#
# Nexus Embedded Platform
# Root CMake configuration file
# Author: Nexus Team
# Version: 0.1.0
#
# This is the main CMake configuration file for the Nexus embedded platform.
# It orchestrates the build process through multiple phases:
#   1. Kconfig configuration system
#   2. Platform detection and configuration
#   3. Dependency management
#   4. Build scheduling and cache
#   5. FreeRTOS integration (if selected)
#   6. Build targets (subdirectories)
#
##############################################################################

cmake_minimum_required(VERSION 3.16)

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
option(NEXUS_ENABLE_CACHE "Enable build cache" OFF)

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

#-----------------------------------------------------------------------------
# Find Required Tools
#-----------------------------------------------------------------------------

# Find Python for configuration generation (needed early)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
message(STATUS "Python: ${Python3_EXECUTABLE}")

#-----------------------------------------------------------------------------
# Phase 1: Kconfig Configuration System
#-----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "=== Phase 1: Kconfig Configuration ===")

# Define configuration file paths
set(NEXUS_KCONFIG_FILE "${CMAKE_SOURCE_DIR}/Kconfig"
    CACHE FILEPATH "Root Kconfig file")
set(NEXUS_CONFIG_FILE "${CMAKE_SOURCE_DIR}/.config"
    CACHE FILEPATH "Configuration file")
set(NEXUS_CONFIG_HEADER "${CMAKE_SOURCE_DIR}/nexus_config.h"
    CACHE FILEPATH "Generated configuration header")

# Load Kconfig module
include(NexusKconfig)

# Generate configuration header from Kconfig
nexus_load_kconfig(
    KCONFIG_FILE ${NEXUS_KCONFIG_FILE}
    CONFIG_FILE ${NEXUS_CONFIG_FILE}
    OUTPUT_HEADER ${NEXUS_CONFIG_HEADER}
)

# Validate Kconfig configuration
nexus_validate_kconfig(CONFIG_FILE ${NEXUS_CONFIG_FILE})

# Load Kconfig configuration into CMake variables
include(LoadKconfig)
load_kconfig(${NEXUS_CONFIG_FILE})

# Override CMake variables with Kconfig values if available
if(DEFINED CONFIG_PLATFORM_NAME)
    set(NEXUS_PLATFORM "${CONFIG_PLATFORM_NAME}" CACHE STRING "Target platform from Kconfig" FORCE)
    message(STATUS "Platform from Kconfig: ${CONFIG_PLATFORM_NAME}")
endif()

if(DEFINED CONFIG_OSAL_BACKEND_NAME)
    set(NEXUS_OSAL_BACKEND "${CONFIG_OSAL_BACKEND_NAME}" CACHE STRING "OSAL backend from Kconfig" FORCE)
    message(STATUS "OSAL backend from Kconfig: ${CONFIG_OSAL_BACKEND_NAME}")
endif()

message(STATUS "Kconfig: Configured")

#-----------------------------------------------------------------------------
# Phase 2: Platform Detection and Configuration
#-----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "=== Phase 2: Platform Configuration ===")

# Load platform module
include(NexusPlatform)

# Set default build type
nexus_set_default_build_type()

# Configure output directories
nexus_configure_output_directories()

# Detect and configure platform (reads Kconfig variables)
nexus_configure_platform()

# Auto-select toolchain and linker script based on platform
nexus_auto_configure_platform()

# Configure compiler flags
if(NEXUS_TARGET_NATIVE)
    nexus_configure_compiler_flags()
else()
    message(STATUS "Using toolchain-provided compiler flags")
endif()

#-----------------------------------------------------------------------------
# Phase 3: Dependency Management
#-----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "=== Phase 3: Dependency Management ===")

# Load dependency management module
include(NexusDependency)

# Add module dependencies to dependency graph
nexus_add_module_dependencies(hal DEPS "")
nexus_add_module_dependencies(osal DEPS hal)
nexus_add_module_dependencies(framework DEPS hal osal)

# Check for circular dependencies
nexus_check_circular_dependencies(HAS_CYCLE)
if(HAS_CYCLE)
    message(FATAL_ERROR "Circular dependency detected in module graph")
endif()

# Get dependency order for build scheduling
nexus_get_dependency_order(BUILD_ORDER)
message(STATUS "Build order: ${BUILD_ORDER}")

# Load dependency lock file if exists
if(EXISTS ${CMAKE_SOURCE_DIR}/nexus.lock)
    nexus_load_lockfile()
    message(STATUS "Dependency lock file: Loaded")
endif()

#-----------------------------------------------------------------------------
# Phase 4: Build Scheduling and Cache
#-----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "=== Phase 4: Build Optimization ===")

# Load cache management module
include(NexusCache)

# Enable build cache if configured
if(NEXUS_ENABLE_CACHE OR DEFINED ENV{NEXUS_ENABLE_CACHE})
    nexus_enable_cache(
        LOCAL_DIR "${CMAKE_BINARY_DIR}/.cache"
        MAX_SIZE 1024
    )
    nexus_cache_init_stats()
    message(STATUS "Build cache: Enabled")
else()
    message(STATUS "Build cache: Disabled (set NEXUS_ENABLE_CACHE=ON to enable)")
endif()

# Configure parallel build based on available resources
include(ProcessorCount)
ProcessorCount(N_CORES)
if(N_CORES EQUAL 0)
    set(N_CORES 1)
endif()

# Reserve one core for system
math(EXPR PARALLEL_JOBS "${N_CORES} - 1")
if(PARALLEL_JOBS LESS 1)
    set(PARALLEL_JOBS 1)
endif()

set(CMAKE_BUILD_PARALLEL_LEVEL ${PARALLEL_JOBS}
    CACHE STRING "Number of parallel build jobs")
message(STATUS "Parallel jobs: ${PARALLEL_JOBS} (${N_CORES} cores available)")

#-----------------------------------------------------------------------------
# Phase 5: FreeRTOS Integration (if selected)
#-----------------------------------------------------------------------------

if(NEXUS_OSAL_BACKEND STREQUAL "freertos")
    message(STATUS "")
    message(STATUS "=== Phase 5: FreeRTOS Integration ===")
    include(FreeRTOS)
endif()

#-----------------------------------------------------------------------------
# Phase 6: Build Targets (Subdirectories)
#-----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "=== Phase 6: Build Targets ===")

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
    # Include CTest configuration if it exists
    if(EXISTS ${CMAKE_SOURCE_DIR}/cmake/CTestConfig.cmake)
        include(${CMAKE_SOURCE_DIR}/cmake/CTestConfig.cmake)
    endif()
    enable_testing()
    add_subdirectory(tests)
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

# Platform information with source indicator
if(DEFINED CONFIG_PLATFORM_NAME)
    message(STATUS "  Platform:       ${NEXUS_PLATFORM} (from Kconfig)")
else()
    message(STATUS "  Platform:       ${NEXUS_PLATFORM}")
endif()

# OSAL backend information with source indicator
if(DEFINED CONFIG_OSAL_BACKEND_NAME)
    message(STATUS "  OSAL Backend:   ${NEXUS_OSAL_BACKEND} (from Kconfig)")
else()
    message(STATUS "  OSAL Backend:   ${NEXUS_OSAL_BACKEND}")
endif()

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

# Additional Kconfig information
if(DEFINED CONFIG_PLATFORM_NAME)
    message(STATUS "----------------------------------------")
    message(STATUS "  Kconfig Details:")
    message(STATUS "    Platform:   ${CONFIG_PLATFORM_NAME}")
    if(CONFIG_PLATFORM_NATIVE)
        message(STATUS "    Logging:    ${CONFIG_NATIVE_ENABLE_LOGGING}")
        message(STATUS "    Log Level:  ${CONFIG_NATIVE_LOG_LEVEL}")
    endif()
endif()

message(STATUS "========================================")
message(STATUS "")

#-----------------------------------------------------------------------------
# End of Root CMake Configuration
#-----------------------------------------------------------------------------
