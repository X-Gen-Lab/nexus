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
if(MSVC)
    # MSVC compiler flags
    add_compile_options(
        /W4
        /WX
        /wd4100  # unreferenced formal parameter
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(DEBUG)
    else()
        add_compile_definitions(NDEBUG)
    endif()
else()
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
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-Og -g3)
        add_compile_definitions(DEBUG)
    else()
        add_compile_options(-O2)
        add_compile_definitions(NDEBUG)
    endif()

    # Coverage flags (GCC/Clang only)
    if(NEXUS_ENABLE_COVERAGE)
        add_compile_options(--coverage)
        add_link_options(--coverage)
    endif()
endif()

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
