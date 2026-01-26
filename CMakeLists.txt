##############################################################################
# Nexus Embedded Platform - Root CMake Configuration
##############################################################################
cmake_minimum_required(VERSION 3.16)

# Project definition
project(nexus
    VERSION 0.1.0
    DESCRIPTION "Nexus Embedded Software Development Platform"
    LANGUAGES C CXX ASM
)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please use a separate build directory.")
endif()

##############################################################################
# Options
##############################################################################
option(NEXUS_BUILD_TESTS "Build unit tests" ON)
option(NEXUS_BUILD_EXAMPLES "Build example applications" ON)
option(NEXUS_BUILD_DOCS "Build documentation" OFF)
option(NEXUS_ENABLE_COVERAGE "Enable code coverage" OFF)

# Platform selection
set(NEXUS_PLATFORM "native" CACHE STRING "Target platform")
set_property(CACHE NEXUS_PLATFORM PROPERTY STRINGS
    native stm32f4 stm32h7 esp32 nrf52
)

# OSAL backend selection
set(NEXUS_OSAL_BACKEND "baremetal" CACHE STRING "OSAL backend")
set_property(CACHE NEXUS_OSAL_BACKEND PROPERTY STRINGS
    baremetal freertos rtthread zephyr
)

##############################################################################
# Global Settings
##############################################################################

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

# Platform detection and configuration
include(NexusPlatform)
nexus_set_default_build_type()
nexus_configure_platform()
nexus_configure_output_directories()

# Find Python for configuration generation
find_package(Python3 COMPONENTS Interpreter REQUIRED)
message(STATUS "Found Python: ${Python3_EXECUTABLE}")

##############################################################################
# Toolchain Configuration
##############################################################################
if(NEXUS_PLATFORM STREQUAL "native")
    # Native build (for testing on host)
    add_compile_definitions(NEXUS_PLATFORM_NATIVE)
    message(STATUS "Building for native platform (host)")
elseif(NEXUS_PLATFORM STREQUAL "stm32f4")
    include(${CMAKE_SOURCE_DIR}/cmake/toolchains/arm-none-eabi.cmake)
    add_compile_definitions(NEXUS_PLATFORM_STM32F4)
    add_compile_definitions(STM32F407xx)
    message(STATUS "Building for STM32F4 platform")
elseif(NEXUS_PLATFORM STREQUAL "stm32h7")
    include(${CMAKE_SOURCE_DIR}/cmake/toolchains/arm-none-eabi.cmake)
    add_compile_definitions(NEXUS_PLATFORM_STM32H7)
    add_compile_definitions(STM32H743xx)
    message(STATUS "Building for STM32H7 platform")
else()
    message(FATAL_ERROR "Unsupported platform: ${NEXUS_PLATFORM}")
endif()

##############################################################################
# Compiler Flags
##############################################################################

# Configure compiler flags based on detected platform and compiler
# Only apply if not cross-compiling (native platform)
if(NEXUS_PLATFORM STREQUAL "native")
    nexus_configure_compiler_flags()
else()
    # Cross-compilation: flags are set by toolchain file
    message(STATUS "Using toolchain-provided compiler flags for ${NEXUS_PLATFORM}")
endif()

##############################################################################
# Kconfig Configuration System
##############################################################################

# Define configuration file paths
set(NEXUS_KCONFIG_FILE "${CMAKE_SOURCE_DIR}/Kconfig" CACHE FILEPATH "Root Kconfig file")
set(NEXUS_CONFIG_FILE "${CMAKE_SOURCE_DIR}/.config" CACHE FILEPATH "Configuration file")
set(NEXUS_CONFIG_HEADER "${CMAKE_SOURCE_DIR}/nexus_config.h" CACHE FILEPATH "Generated configuration header")

# Configuration generation function
function(nexus_generate_config)
    set(GENERATOR_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/kconfig/generate_config.py")

    # Check if generator script exists
    if(NOT EXISTS ${GENERATOR_SCRIPT})
        message(FATAL_ERROR "Configuration generator script not found: ${GENERATOR_SCRIPT}")
    endif()

    # Check if Kconfig file exists
    if(NOT EXISTS ${NEXUS_KCONFIG_FILE})
        message(WARNING "Kconfig file not found: ${NEXUS_KCONFIG_FILE}")
    endif()

    # Collect all Kconfig files for dependency tracking
    file(GLOB_RECURSE KCONFIG_FILES
        "${CMAKE_SOURCE_DIR}/Kconfig"
        "${CMAKE_SOURCE_DIR}/*/Kconfig"
        "${CMAKE_SOURCE_DIR}/*/*/Kconfig"
        "${CMAKE_SOURCE_DIR}/*/*/*/Kconfig"
    )

    # Add .config file as a dependency if it exists
    if(EXISTS ${NEXUS_CONFIG_FILE})
        list(APPEND KCONFIG_FILES ${NEXUS_CONFIG_FILE})
    endif()

    # Configure file dependencies - CMake will reconfigure if these change
    foreach(KCONFIG_FILE ${KCONFIG_FILES})
        if(EXISTS ${KCONFIG_FILE})
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${KCONFIG_FILE})
        endif()
    endforeach()

    # Generate configuration header
    message(STATUS "Generating HAL configuration header...")

    if(EXISTS ${NEXUS_CONFIG_FILE})
        # Use existing .config file
        execute_process(
            COMMAND ${Python3_EXECUTABLE} ${GENERATOR_SCRIPT}
                    --kconfig ${NEXUS_KCONFIG_FILE}
                    --config ${NEXUS_CONFIG_FILE}
                    --output ${NEXUS_CONFIG_HEADER}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE RESULT
            OUTPUT_VARIABLE OUTPUT
            ERROR_VARIABLE ERROR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
        )

        if(NOT RESULT EQUAL 0)
            message(WARNING "Failed to generate HAL config from .config file:")
            if(ERROR)
                message(WARNING "  ${ERROR}")
            endif()
            message(STATUS "Falling back to default configuration...")

            # Fall back to default configuration
            execute_process(
                COMMAND ${Python3_EXECUTABLE} ${GENERATOR_SCRIPT}
                        --kconfig ${NEXUS_KCONFIG_FILE}
                        --default
                        --output ${NEXUS_CONFIG_HEADER}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE RESULT
                ERROR_QUIET
            )

            if(NOT RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to generate default HAL config")
            endif()
        endif()
    else()
        # Generate default configuration
        message(STATUS "No .config file found, generating default configuration...")
        execute_process(
            COMMAND ${Python3_EXECUTABLE} ${GENERATOR_SCRIPT}
                    --kconfig ${NEXUS_KCONFIG_FILE}
                    --default
                    --output ${NEXUS_CONFIG_HEADER}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE RESULT
            OUTPUT_VARIABLE OUTPUT
            ERROR_QUIET
        )

        if(NOT RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to generate default HAL config")
        endif()
    endif()

    # Mark the generated header as a generated file
    set_source_files_properties(${NEXUS_CONFIG_HEADER} PROPERTIES GENERATED TRUE)

    # Count Kconfig files for status message
    list(LENGTH KCONFIG_FILES KCONFIG_COUNT)

    message(STATUS "Generated configuration header: ${NEXUS_CONFIG_HEADER}")
    message(STATUS "Tracking ${KCONFIG_COUNT} Kconfig/config files for changes")
endfunction()

# Generate configuration during CMake configuration phase
nexus_generate_config()

##############################################################################
# FreeRTOS Integration (if selected)
##############################################################################
include(FreeRTOS)

##############################################################################
# Subdirectories
##############################################################################

# Core libraries
add_subdirectory(hal)
add_subdirectory(osal)

# Framework layer
add_subdirectory(framework)

# Middleware (when available)
# add_subdirectory(middleware)

# Components (when available)
# add_subdirectory(components)

# Platform-specific code
add_subdirectory(platforms)

# Example applications
if(NEXUS_BUILD_EXAMPLES)
    add_subdirectory(applications)
endif()

# Tests
if(NEXUS_BUILD_TESTS AND NEXUS_PLATFORM STREQUAL "native")
    include(${CMAKE_SOURCE_DIR}/cmake/CTestConfig.cmake)
    enable_testing()
    add_subdirectory(tests)
endif()

# Documentation
if(NEXUS_BUILD_DOCS)
    add_subdirectory(docs)
endif()

##############################################################################
# Summary
##############################################################################
message(STATUS "")
message(STATUS "=== Nexus Build Configuration ===")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  Platform:     ${NEXUS_PLATFORM}")
message(STATUS "  OSAL Backend: ${NEXUS_OSAL_BACKEND}")
message(STATUS "  Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests:        ${NEXUS_BUILD_TESTS}")
message(STATUS "  Examples:     ${NEXUS_BUILD_EXAMPLES}")
message(STATUS "  Coverage:     ${NEXUS_ENABLE_COVERAGE}")
message(STATUS "=================================")
message(STATUS "")
