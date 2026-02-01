#-----------------------------------------------------------------------------
# NexusVendor.cmake - Vendor Dependency Management Module
#-----------------------------------------------------------------------------
# NexusVendor.cmake
# Flexible vendor library management with multiple source support
# Author: Nexus Team
#
# This module provides:
# - Automatic Git submodule initialization
# - Flexible vendor path resolution (user paths, system packages, submodules)
# - On-demand dependency loading (only load what's needed)
# - Clear error messages and recovery suggestions
#
#-----------------------------------------------------------------------------

include_guard(GLOBAL)

#-----------------------------------------------------------------------------
# Global Variables
#-----------------------------------------------------------------------------

set(NEXUS_VENDOR_ROOT "${CMAKE_SOURCE_DIR}/vendors" CACHE PATH "Vendor root directory")
set(NEXUS_EXT_ROOT "${CMAKE_SOURCE_DIR}/ext" CACHE PATH "External dependencies root")

# Track initialized submodules to avoid duplicate work
set(NEXUS_INITIALIZED_SUBMODULES "" CACHE INTERNAL "List of initialized submodules")

#-----------------------------------------------------------------------------
# Submodule Management
#-----------------------------------------------------------------------------

#
# Check if a Git submodule is initialized
# Arguments:
#   PATH: Submodule path relative to repository root
#   RESULT_VAR: Output variable (TRUE if initialized, FALSE otherwise)
#
function(nexus_is_submodule_initialized PATH RESULT_VAR)
    set(FULL_PATH "${CMAKE_SOURCE_DIR}/${PATH}")

    # Check if directory exists and is not empty
    if(EXISTS "${FULL_PATH}")
        file(GLOB CONTENTS "${FULL_PATH}/*")
        if(CONTENTS)
            set(${RESULT_VAR} TRUE PARENT_SCOPE)
            return()
        endif()
    endif()

    set(${RESULT_VAR} FALSE PARENT_SCOPE)
endfunction()

#
# Initialize a Git submodule automatically
# Arguments:
#   PATH: Submodule path relative to repository root
#   REQUIRED: If TRUE, fail on initialization error (default: TRUE)
#
function(nexus_init_submodule PATH)
    cmake_parse_arguments(ARG "" "REQUIRED" "" ${ARGN})

    if(NOT DEFINED ARG_REQUIRED)
        set(ARG_REQUIRED TRUE)
    endif()

    # Check if already initialized in this CMake run
    list(FIND NEXUS_INITIALIZED_SUBMODULES ${PATH} ALREADY_INIT)
    if(NOT ALREADY_INIT EQUAL -1)
        return()
    endif()

    # Check if submodule is already initialized
    nexus_is_submodule_initialized(${PATH} IS_INIT)
    if(IS_INIT)
        message(STATUS "Submodule already initialized: ${PATH}")
        list(APPEND NEXUS_INITIALIZED_SUBMODULES ${PATH})
        set(NEXUS_INITIALIZED_SUBMODULES ${NEXUS_INITIALIZED_SUBMODULES} CACHE INTERNAL "")
        return()
    endif()

    message(STATUS "Initializing submodule: ${PATH}")

    # Try to initialize submodule
    find_package(Git QUIET)
    if(NOT GIT_FOUND)
        if(ARG_REQUIRED)
            message(FATAL_ERROR
                "Git not found. Cannot initialize submodule: ${PATH}\n"
                "Please install Git or manually initialize submodules:\n"
                "  git submodule update --init --recursive"
            )
        else()
            message(WARNING "Git not found. Skipping submodule: ${PATH}")
            return()
        endif()
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --depth 1 -- ${PATH}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE GIT_RESULT
        OUTPUT_VARIABLE GIT_OUTPUT
        ERROR_VARIABLE GIT_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    if(NOT GIT_RESULT EQUAL 0)
        if(ARG_REQUIRED)
            message(FATAL_ERROR
                "Failed to initialize submodule: ${PATH}\n"
                "Git error: ${GIT_ERROR}\n"
                "Please try manually:\n"
                "  git submodule update --init -- ${PATH}"
            )
        else()
            message(WARNING
                "Failed to initialize submodule: ${PATH}\n"
                "Error: ${GIT_ERROR}"
            )
            return()
        endif()
    endif()

    # Verify initialization
    nexus_is_submodule_initialized(${PATH} IS_INIT_NOW)
    if(IS_INIT_NOW)
        message(STATUS "Successfully initialized submodule: ${PATH}")
        list(APPEND NEXUS_INITIALIZED_SUBMODULES ${PATH})
        set(NEXUS_INITIALIZED_SUBMODULES ${NEXUS_INITIALIZED_SUBMODULES} CACHE INTERNAL "")
    else()
        if(ARG_REQUIRED)
            message(FATAL_ERROR "Submodule initialization verification failed: ${PATH}")
        endif()
    endif()
endfunction()

#-----------------------------------------------------------------------------
# Vendor Path Resolution
#-----------------------------------------------------------------------------

#
# Resolve vendor library path with multiple source support
# Arguments:
#   NAME: Vendor library name (e.g., "CMSIS_CORE", "HAL_DRIVER")
#   SUBMODULE_PATH: Default submodule path (optional)
#   RESULT_VAR: Output variable for resolved path
#   REQUIRED: If TRUE, fail if path cannot be resolved (default: TRUE)
#   FIND_PACKAGE: Try find_package before submodule (default: FALSE)
#   DOWNLOAD_URL: URL for auto-download (optional, future)
#
# Resolution priority:
#   1. User-specified cache variable: NEXUS_VENDOR_<NAME>_DIR
#   2. Environment variable: NEXUS_<NAME>_DIR
#   3. System package via find_package (if FIND_PACKAGE=TRUE)
#   4. Git submodule at SUBMODULE_PATH (auto-initialize if needed)
#   5. Auto-download from DOWNLOAD_URL (if specified, future)
#
function(nexus_resolve_vendor_path NAME)
    cmake_parse_arguments(
        ARG
        "REQUIRED;FIND_PACKAGE"
        "SUBMODULE_PATH;RESULT_VAR;DOWNLOAD_URL"
        ""
        ${ARGN}
    )

    if(NOT ARG_RESULT_VAR)
        message(FATAL_ERROR "nexus_resolve_vendor_path: RESULT_VAR is required")
    endif()

    if(NOT DEFINED ARG_REQUIRED)
        set(ARG_REQUIRED TRUE)
    endif()

    set(CACHE_VAR "NEXUS_VENDOR_${NAME}_DIR")
    set(ENV_VAR "NEXUS_${NAME}_DIR")

    # Priority 1: User-specified cache variable
    if(DEFINED ${CACHE_VAR} AND EXISTS "${${CACHE_VAR}}")
        message(STATUS "Using user-specified path for ${NAME}: ${${CACHE_VAR}}")
        set(${ARG_RESULT_VAR} "${${CACHE_VAR}}" PARENT_SCOPE)
        return()
    endif()

    # Priority 2: Environment variable
    if(DEFINED ENV{${ENV_VAR}} AND EXISTS "$ENV{${ENV_VAR}}")
        message(STATUS "Using environment path for ${NAME}: $ENV{${ENV_VAR}}")
        set(${CACHE_VAR} "$ENV{${ENV_VAR}}" CACHE PATH "Path to ${NAME} (from environment)")
        set(${ARG_RESULT_VAR} "$ENV{${ENV_VAR}}" PARENT_SCOPE)
        return()
    endif()

    # Priority 3: System package (if enabled)
    if(ARG_FIND_PACKAGE)
        message(STATUS "Searching for ${NAME} via find_package...")
        find_package(${NAME} QUIET)

        if(${NAME}_FOUND)
            # Try to get package directory
            if(DEFINED ${NAME}_DIR AND EXISTS "${${NAME}_DIR}")
                message(STATUS "Found ${NAME} via system package: ${${NAME}_DIR}")
                set(${CACHE_VAR} "${${NAME}_DIR}" CACHE PATH "Path to ${NAME} (from system)")
                set(${ARG_RESULT_VAR} "${${NAME}_DIR}" PARENT_SCOPE)
                return()
            elseif(DEFINED ${NAME}_INCLUDE_DIR AND EXISTS "${${NAME}_INCLUDE_DIR}")
                message(STATUS "Found ${NAME} via system package: ${${NAME}_INCLUDE_DIR}")
                set(${CACHE_VAR} "${${NAME}_INCLUDE_DIR}" CACHE PATH "Path to ${NAME} (from system)")
                set(${ARG_RESULT_VAR} "${${NAME}_INCLUDE_DIR}" PARENT_SCOPE)
                return()
            endif()
        endif()
    endif()

    # Priority 4: Git submodule
    if(ARG_SUBMODULE_PATH)
        set(SUBMODULE_FULL_PATH "${CMAKE_SOURCE_DIR}/${ARG_SUBMODULE_PATH}")

        # Check if submodule is initialized
        nexus_is_submodule_initialized(${ARG_SUBMODULE_PATH} IS_INIT)

        if(NOT IS_INIT)
            # Try to auto-initialize
            message(STATUS "Auto-initializing submodule for ${NAME}: ${ARG_SUBMODULE_PATH}")
            nexus_init_submodule(${ARG_SUBMODULE_PATH} REQUIRED ${ARG_REQUIRED})

            # Re-check after initialization
            nexus_is_submodule_initialized(${ARG_SUBMODULE_PATH} IS_INIT)
        endif()

        if(IS_INIT AND EXISTS "${SUBMODULE_FULL_PATH}")
            message(STATUS "Using submodule path for ${NAME}: ${SUBMODULE_FULL_PATH}")
            set(${CACHE_VAR} "${SUBMODULE_FULL_PATH}" CACHE PATH "Path to ${NAME} (from submodule)")
            set(${ARG_RESULT_VAR} "${SUBMODULE_FULL_PATH}" PARENT_SCOPE)
            return()
        endif()
    endif()

    # Priority 5: Auto-download (future feature)
    if(ARG_DOWNLOAD_URL AND NEXUS_ENABLE_AUTO_DOWNLOAD)
        message(STATUS "Auto-download is not yet implemented for ${NAME}")
        # TODO: Implement auto-download functionality
    endif()

    # Failed to resolve
    if(ARG_REQUIRED)
        set(ERROR_MSG "Cannot resolve vendor path for: ${NAME}\n\nTried:\n")
        string(APPEND ERROR_MSG "  1. Cache variable: ${CACHE_VAR}\n")
        string(APPEND ERROR_MSG "  2. Environment variable: ${ENV_VAR}\n")

        if(ARG_FIND_PACKAGE)
            string(APPEND ERROR_MSG "  3. System package: find_package(${NAME})\n")
            string(APPEND ERROR_MSG "  4. Submodule: ${ARG_SUBMODULE_PATH}\n")
        else()
            string(APPEND ERROR_MSG "  3. Submodule: ${ARG_SUBMODULE_PATH}\n")
        endif()

        string(APPEND ERROR_MSG "\nSolutions:\n")
        string(APPEND ERROR_MSG "  - Set ${CACHE_VAR} to the library path\n")
        string(APPEND ERROR_MSG "  - Set environment variable ${ENV_VAR}\n")

        if(ARG_SUBMODULE_PATH)
            string(APPEND ERROR_MSG "  - Initialize submodule: git submodule update --init ${ARG_SUBMODULE_PATH}\n")
        endif()

        string(APPEND ERROR_MSG "  - Run setup script: ./scripts/setup_deps.sh\n")

        message(FATAL_ERROR "${ERROR_MSG}")
    else()
        message(WARNING "Cannot resolve vendor path for: ${NAME}")
        set(${ARG_RESULT_VAR} "" PARENT_SCOPE)
    endif()
endfunction()

#-----------------------------------------------------------------------------
# Platform-Specific Vendor Configuration
#-----------------------------------------------------------------------------#-----------------------------------------------------------------------------
# Platform-Specific Vendor Configuration
#-----------------------------------------------------------------------------

#
# Configure vendor paths for STM32 platform
# Arguments:
#   SERIES: STM32 series (f4, h7, l4, etc.)
#
function(nexus_configure_stm32_vendors SERIES)
    string(TOUPPER ${SERIES} SERIES_UPPER)
    string(TOLOWER ${SERIES} SERIES_LOWER)

    message(STATUS "Configuring STM32${SERIES_UPPER} vendor libraries...")

    # CMSIS Core (ARM)
    nexus_resolve_vendor_path(CMSIS_CORE
        SUBMODULE_PATH "vendors/arm/CMSIS_5"
        RESULT_VAR CMSIS_CORE_DIR
        REQUIRED TRUE
    )

    # CMSIS Device (ST)
    nexus_resolve_vendor_path(CMSIS_DEVICE_${SERIES_UPPER}
        SUBMODULE_PATH "vendors/st/cmsis_device_${SERIES_LOWER}"
        RESULT_VAR CMSIS_DEVICE_DIR
        REQUIRED TRUE
    )

    # HAL Driver (ST)
    nexus_resolve_vendor_path(HAL_DRIVER_${SERIES_UPPER}
        SUBMODULE_PATH "vendors/st/stm32${SERIES_LOWER}xx_hal_driver"
        RESULT_VAR HAL_DRIVER_DIR
        REQUIRED TRUE
    )

    # Export to parent scope
    set(CMSIS_CORE_DIR ${CMSIS_CORE_DIR} PARENT_SCOPE)
    set(CMSIS_DEVICE_DIR ${CMSIS_DEVICE_DIR} PARENT_SCOPE)
    set(HAL_DRIVER_DIR ${HAL_DRIVER_DIR} PARENT_SCOPE)

    message(STATUS "STM32${SERIES_UPPER} vendor paths configured:")
    message(STATUS "  CMSIS Core:   ${CMSIS_CORE_DIR}")
    message(STATUS "  CMSIS Device: ${CMSIS_DEVICE_DIR}")
    message(STATUS "  HAL Driver:   ${HAL_DRIVER_DIR}")
endfunction()

#
# Configure vendor paths for FreeRTOS
#
function(nexus_configure_freertos_vendor)
    message(STATUS "Configuring FreeRTOS vendor library...")

    nexus_resolve_vendor_path(FREERTOS
        SUBMODULE_PATH "ext/freertos"
        RESULT_VAR FREERTOS_DIR
        REQUIRED TRUE
    )

    # Export to parent scope
    set(FREERTOS_DIR ${FREERTOS_DIR} PARENT_SCOPE)

    message(STATUS "FreeRTOS vendor path configured: ${FREERTOS_DIR}")
endfunction()

#
# Configure vendor paths for GoogleTest
#
function(nexus_configure_gtest_vendor)
    message(STATUS "Configuring GoogleTest vendor library...")

    nexus_resolve_vendor_path(GTEST
        SUBMODULE_PATH "ext/googletest"
        RESULT_VAR GTEST_DIR
        REQUIRED FALSE  # Not required for embedded builds
    )

    if(GTEST_DIR)
        # Export to parent scope
        set(GTEST_DIR ${GTEST_DIR} PARENT_SCOPE)
        message(STATUS "GoogleTest vendor path configured: ${GTEST_DIR}")
    else()
        message(STATUS "GoogleTest not available (not required for embedded builds)")
    endif()
endfunction()

#-----------------------------------------------------------------------------
# Batch Operations
#-----------------------------------------------------------------------------

#
# Initialize all submodules for a specific platform
# Arguments:
#   PLATFORM: Platform name (stm32, native, etc.)
#   SERIES: Platform series (optional, e.g., f4 for STM32)
#
function(nexus_init_platform_submodules PLATFORM)
    cmake_parse_arguments(ARG "" "SERIES" "" ${ARGN})

    message(STATUS "Initializing submodules for platform: ${PLATFORM}")

    if(PLATFORM STREQUAL "stm32")
        if(NOT ARG_SERIES)
            message(FATAL_ERROR "SERIES is required for STM32 platform")
        endif()

        string(TOLOWER ${ARG_SERIES} SERIES_LOWER)

        # Initialize STM32-specific submodules
        nexus_init_submodule("vendors/arm/CMSIS_5" REQUIRED TRUE)
        nexus_init_submodule("vendors/st/cmsis_device_${SERIES_LOWER}" REQUIRED TRUE)
        nexus_init_submodule("vendors/st/stm32${SERIES_LOWER}xx_hal_driver" REQUIRED TRUE)

    elseif(PLATFORM STREQUAL "native")
        # Initialize test framework for native builds
        nexus_init_submodule("ext/googletest" REQUIRED FALSE)

    else()
        message(WARNING "Unknown platform for submodule initialization: ${PLATFORM}")
    endif()

    message(STATUS "Platform submodules initialized")
endfunction()

#
# Initialize common submodules (FreeRTOS, etc.)
#
function(nexus_init_common_submodules)
    message(STATUS "Initializing common submodules...")

    # FreeRTOS (if OSAL backend is freertos)
    if(NEXUS_OSAL_BACKEND STREQUAL "freertos")
        nexus_init_submodule("ext/freertos" REQUIRED TRUE)
    endif()

    message(STATUS "Common submodules initialized")
endfunction()

message(STATUS "NexusVendor module loaded")

#-----------------------------------------------------------------------------
# End of NexusVendor.cmake
#-----------------------------------------------------------------------------
