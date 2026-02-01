#-----------------------------------------------------------------------------
# VendorConfig.cmake - Unified Vendor Library Configuration
#-----------------------------------------------------------------------------
# VendorConfig.cmake
# Centralized configuration for all vendor libraries
# Author: Nexus Team
#
# This file defines all vendor library paths and metadata in one place.
# It supports multiple dependency sources with priority:
#   1. User-specified paths (NEXUS_VENDOR_<NAME>_DIR)
#   2. Environment variables (NEXUS_<NAME>_DIR)
#   3. System packages (find_package)
#   4. Git submodules (default)
#   5. Auto-download (optional, future)
#
#-----------------------------------------------------------------------------

include_guard(GLOBAL)

#-----------------------------------------------------------------------------
# Initialize Vendor Registry
#-----------------------------------------------------------------------------

# Initialize empty registry (will be populated by registration calls)
set_property(GLOBAL PROPERTY _NEXUS_VENDOR_REGISTRY "")

#-----------------------------------------------------------------------------
# Vendor Registration Macro (uses parent scope)
#-----------------------------------------------------------------------------

#
# Register a vendor library
# Arguments:
#   NAME: Unique identifier
#   DESCRIPTION: Human-readable description
#   SUBMODULE_PATH: Git submodule path
#   INCLUDE_DIRS: Include directories (list)
#   PLATFORMS: Supported platforms (list, empty = all)
#   REQUIRED: Whether required (TRUE/FALSE)
#
macro(nexus_register_vendor)
    cmake_parse_arguments(
        _ARG
        "REQUIRED"
        "NAME;DESCRIPTION;SUBMODULE_PATH"
        "INCLUDE_DIRS;PLATFORMS"
        ${ARGN}
    )

    if(NOT _ARG_NAME)
        message(FATAL_ERROR "nexus_register_vendor: NAME is required")
    endif()

    # Create vendor entry
    set(_VENDOR_ENTRY "")
    list(APPEND _VENDOR_ENTRY "name=${_ARG_NAME}")

    if(_ARG_DESCRIPTION)
        list(APPEND _VENDOR_ENTRY "description=${_ARG_DESCRIPTION}")
    endif()

    if(_ARG_SUBMODULE_PATH)
        list(APPEND _VENDOR_ENTRY "submodule_path=${_ARG_SUBMODULE_PATH}")
    endif()

    if(_ARG_INCLUDE_DIRS)
        string(REPLACE ";" "," _INCLUDE_DIRS_STR "${_ARG_INCLUDE_DIRS}")
        list(APPEND _VENDOR_ENTRY "include_dirs=${_INCLUDE_DIRS_STR}")
    endif()

    if(_ARG_PLATFORMS)
        string(REPLACE ";" "," _PLATFORMS_STR "${_ARG_PLATFORMS}")
        list(APPEND _VENDOR_ENTRY "platforms=${_PLATFORMS_STR}")
    else()
        list(APPEND _VENDOR_ENTRY "platforms=")
    endif()

    if(_ARG_REQUIRED)
        list(APPEND _VENDOR_ENTRY "required=TRUE")
    else()
        list(APPEND _VENDOR_ENTRY "required=FALSE")
    endif()

    # Add to global registry
    string(REPLACE ";" "|" _VENDOR_ENTRY_STR "${_VENDOR_ENTRY}")
    get_property(_CURRENT_REGISTRY GLOBAL PROPERTY _NEXUS_VENDOR_REGISTRY)
    list(APPEND _CURRENT_REGISTRY "${_VENDOR_ENTRY_STR}")
    set_property(GLOBAL PROPERTY _NEXUS_VENDOR_REGISTRY "${_CURRENT_REGISTRY}")

    message(VERBOSE "Registered vendor: ${_ARG_NAME}")
endmacro()

#-----------------------------------------------------------------------------
# ARM Vendors
#-----------------------------------------------------------------------------

# CMSIS Core (ARM Cortex-M)
nexus_register_vendor(
    NAME CMSIS_CORE
    DESCRIPTION "ARM CMSIS Core for Cortex-M"
    SUBMODULE_PATH "vendors/arm/CMSIS_5"
    INCLUDE_DIRS "CMSIS/Core/Include"
    PLATFORMS "stm32;gd32;nrf52"
    REQUIRED
)

#-----------------------------------------------------------------------------
# STMicroelectronics Vendors
#-----------------------------------------------------------------------------

# STM32 CMSIS Device Headers (per series)
set(_STM32_SERIES c0 f0 f1 f2 f3 f4 f7 g0 g4 h5 h7 l0 l1 l4 l5 u0 u3 u5)

foreach(_SERIES ${_STM32_SERIES})
    string(TOUPPER ${_SERIES} _SERIES_UPPER)

    # CMSIS Device
    nexus_register_vendor(
        NAME "CMSIS_DEVICE_${_SERIES_UPPER}"
        DESCRIPTION "STM32${_SERIES_UPPER} CMSIS Device Headers"
        SUBMODULE_PATH "vendors/st/cmsis_device_${_SERIES}"
        INCLUDE_DIRS "Include"
        PLATFORMS "stm32"
    )

    # HAL Driver
    nexus_register_vendor(
        NAME "HAL_DRIVER_${_SERIES_UPPER}"
        DESCRIPTION "STM32${_SERIES_UPPER} HAL Driver"
        SUBMODULE_PATH "vendors/st/stm32${_SERIES}xx_hal_driver"
        INCLUDE_DIRS "Inc;Inc/Legacy"
        PLATFORMS "stm32"
    )
endforeach()

#-----------------------------------------------------------------------------
# RTOS Vendors
#-----------------------------------------------------------------------------

# FreeRTOS Kernel
nexus_register_vendor(
    NAME FREERTOS
    DESCRIPTION "FreeRTOS Real-Time Kernel"
    SUBMODULE_PATH "ext/freertos"
    INCLUDE_DIRS "include"
)

#-----------------------------------------------------------------------------
# Test Framework Vendors
#-----------------------------------------------------------------------------

# GoogleTest
nexus_register_vendor(
    NAME GTEST
    DESCRIPTION "Google Test Framework"
    SUBMODULE_PATH "ext/googletest"
    INCLUDE_DIRS "googletest/include;googlemock/include"
    PLATFORMS "native"
)

#-----------------------------------------------------------------------------
# Vendor Query Functions
#-----------------------------------------------------------------------------

#
# Get vendor metadata by name
# Arguments:
#   NAME: Vendor name
#   OUTPUT_VAR: Output variable for metadata (list)
#
function(nexus_get_vendor_metadata NAME OUTPUT_VAR)
    get_property(REGISTRY GLOBAL PROPERTY _NEXUS_VENDOR_REGISTRY)

    foreach(ENTRY ${REGISTRY})
        string(REPLACE "|" ";" ENTRY_LIST "${ENTRY}")

        # Extract name
        foreach(FIELD ${ENTRY_LIST})
            if(FIELD MATCHES "^name=(.+)$")
                set(ENTRY_NAME ${CMAKE_MATCH_1})
                break()
            endif()
        endforeach()

        # Check if this is the vendor we're looking for
        if(ENTRY_NAME STREQUAL NAME)
            set(${OUTPUT_VAR} ${ENTRY_LIST} PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Not found
    set(${OUTPUT_VAR} "" PARENT_SCOPE)
endfunction()

#
# Get vendor field value
# Arguments:
#   METADATA: Vendor metadata (list)
#   FIELD: Field name
#   OUTPUT_VAR: Output variable
#
function(nexus_get_vendor_field METADATA FIELD OUTPUT_VAR)
    foreach(ENTRY ${METADATA})
        if(ENTRY MATCHES "^${FIELD}=(.*)$")
            set(${OUTPUT_VAR} ${CMAKE_MATCH_1} PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set(${OUTPUT_VAR} "" PARENT_SCOPE)
endfunction()

#
# Check if vendor supports current platform
# Arguments:
#   NAME: Vendor name
#   OUTPUT_VAR: Output variable (TRUE/FALSE)
#   PLATFORM: Platform to check (optional, default: NEXUS_PLATFORM)
#
function(nexus_vendor_supports_platform NAME OUTPUT_VAR)
    cmake_parse_arguments(ARG "" "PLATFORM" "" ${ARGN})

    if(NOT ARG_PLATFORM)
        set(ARG_PLATFORM ${NEXUS_PLATFORM})
    endif()

    # Get vendor metadata
    nexus_get_vendor_metadata(${NAME} METADATA)
    if(NOT METADATA)
        set(${OUTPUT_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Get platforms field
    nexus_get_vendor_field("${METADATA}" "platforms" PLATFORMS_STR)

    # Empty platforms = supports all
    if(NOT PLATFORMS_STR OR PLATFORMS_STR STREQUAL "")
        set(${OUTPUT_VAR} TRUE PARENT_SCOPE)
        return()
    endif()

    # Check if platform is in list
    string(REPLACE "," ";" PLATFORMS_LIST "${PLATFORMS_STR}")
    list(FIND PLATFORMS_LIST ${ARG_PLATFORM} PLATFORM_INDEX)

    if(PLATFORM_INDEX EQUAL -1)
        set(${OUTPUT_VAR} FALSE PARENT_SCOPE)
    else()
        set(${OUTPUT_VAR} TRUE PARENT_SCOPE)
    endif()
endfunction()

#
# List all vendors for a platform
# Arguments:
#   OUTPUT_VAR: Output variable (list of vendor names)
#   PLATFORM: Platform name (optional, default: NEXUS_PLATFORM)
#
function(nexus_list_platform_vendors OUTPUT_VAR)
    cmake_parse_arguments(ARG "" "PLATFORM" "" ${ARGN})

    if(NOT ARG_PLATFORM)
        set(ARG_PLATFORM ${NEXUS_PLATFORM})
    endif()

    set(VENDOR_LIST "")
    get_property(REGISTRY GLOBAL PROPERTY _NEXUS_VENDOR_REGISTRY)

    foreach(ENTRY ${REGISTRY})
        string(REPLACE "|" ";" ENTRY_LIST "${ENTRY}")

        # Extract name
        nexus_get_vendor_field("${ENTRY_LIST}" "name" VENDOR_NAME)

        # Check if supports platform
        nexus_vendor_supports_platform(${VENDOR_NAME} SUPPORTS PLATFORM ${ARG_PLATFORM})

        if(SUPPORTS)
            list(APPEND VENDOR_LIST ${VENDOR_NAME})
        endif()
    endforeach()

    set(${OUTPUT_VAR} ${VENDOR_LIST} PARENT_SCOPE)
endfunction()

#-----------------------------------------------------------------------------
# Enhanced Vendor Resolution
#-----------------------------------------------------------------------------

#
# Resolve vendor path with registry support
# Arguments:
#   NAME: Vendor name (from registry)
#   RESULT_VAR: Output variable for resolved path
#   REQUIRED: If TRUE, fail if path cannot be resolved (default: TRUE)
#   FIND_PACKAGE: Try find_package before submodule (default: FALSE)
#
function(nexus_resolve_vendor NAME)
    cmake_parse_arguments(
        ARG
        "REQUIRED;FIND_PACKAGE"
        "RESULT_VAR"
        ""
        ${ARGN}
    )

    if(NOT ARG_RESULT_VAR)
        message(FATAL_ERROR "nexus_resolve_vendor: RESULT_VAR is required")
    endif()

    if(NOT DEFINED ARG_REQUIRED)
        set(ARG_REQUIRED TRUE)
    endif()

    # Get vendor metadata
    nexus_get_vendor_metadata(${NAME} METADATA)
    if(NOT METADATA)
        if(ARG_REQUIRED)
            message(FATAL_ERROR "Unknown vendor: ${NAME}")
        else()
            set(${ARG_RESULT_VAR} "" PARENT_SCOPE)
            return()
        endif()
    endif()

    # Extract submodule path
    nexus_get_vendor_field("${METADATA}" "submodule_path" SUBMODULE_PATH)

    # Use NexusVendor module for resolution
    include(NexusVendor)
    nexus_resolve_vendor_path(${NAME}
        SUBMODULE_PATH ${SUBMODULE_PATH}
        RESULT_VAR RESOLVED_PATH
        REQUIRED ${ARG_REQUIRED}
        FIND_PACKAGE ${ARG_FIND_PACKAGE}
    )

    set(${ARG_RESULT_VAR} ${RESOLVED_PATH} PARENT_SCOPE)
endfunction()

#
# Initialize all vendors for current platform
#
function(nexus_init_platform_vendors)
    cmake_parse_arguments(ARG "" "PLATFORM" "" ${ARGN})

    if(NOT ARG_PLATFORM)
        set(ARG_PLATFORM ${NEXUS_PLATFORM})
    endif()

    message(STATUS "Initializing vendors for platform: ${ARG_PLATFORM}")

    # Get list of vendors for this platform
    nexus_list_platform_vendors(VENDOR_LIST PLATFORM ${ARG_PLATFORM})

    if(NOT VENDOR_LIST)
        message(STATUS "No vendors required for platform: ${ARG_PLATFORM}")
        return()
    endif()

    # Initialize each vendor
    foreach(VENDOR_NAME ${VENDOR_LIST})
        nexus_get_vendor_metadata(${VENDOR_NAME} METADATA)
        nexus_get_vendor_field("${METADATA}" "required" IS_REQUIRED)

        if(IS_REQUIRED STREQUAL "TRUE")
            nexus_resolve_vendor(${VENDOR_NAME}
                RESULT_VAR VENDOR_PATH
                REQUIRED TRUE
            )
        else()
            nexus_resolve_vendor(${VENDOR_NAME}
                RESULT_VAR VENDOR_PATH
                REQUIRED FALSE
            )
        endif()
    endforeach()

    message(STATUS "Platform vendors initialized")
endfunction()

#-----------------------------------------------------------------------------
# Vendor Information Display
#-----------------------------------------------------------------------------

#
# Print vendor registry information
#
function(nexus_print_vendor_registry)
    get_property(REGISTRY GLOBAL PROPERTY _NEXUS_VENDOR_REGISTRY)

    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "Vendor Library Registry")
    message(STATUS "========================================")

    foreach(ENTRY ${REGISTRY})
        string(REPLACE "|" ";" ENTRY_LIST "${ENTRY}")

        nexus_get_vendor_field("${ENTRY_LIST}" "name" NAME)
        nexus_get_vendor_field("${ENTRY_LIST}" "description" DESC)
        nexus_get_vendor_field("${ENTRY_LIST}" "platforms" PLATFORMS)
        nexus_get_vendor_field("${ENTRY_LIST}" "required" REQUIRED)

        message(STATUS "")
        message(STATUS "  ${NAME}")
        if(DESC)
            message(STATUS "    Description: ${DESC}")
        endif()

        if(PLATFORMS)
            message(STATUS "    Platforms:   ${PLATFORMS}")
        else()
            message(STATUS "    Platforms:   All")
        endif()

        if(REQUIRED STREQUAL "TRUE")
            message(STATUS "    Required:    Yes")
        else()
            message(STATUS "    Required:    No")
        endif()
    endforeach()

    message(STATUS "")
    message(STATUS "========================================")
endfunction()

message(STATUS "VendorConfig module loaded")

#-----------------------------------------------------------------------------
# End of VendorConfig.cmake
#-----------------------------------------------------------------------------

