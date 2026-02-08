#-----------------------------------------------------------------------------
# NexusHelpers.cmake - Helper Functions Module
#-----------------------------------------------------------------------------
# Helper functions for error handling, logging, and validation
# Author: Nexus Team
#
# This module provides:
# - Unified error handling
# - Logging utilities
# - Input validation helpers
#
#-----------------------------------------------------------------------------

include_guard(GLOBAL)

# Module loaded (silent - only show in verbose mode)
# message(STATUS "NexusHelpers module loaded")

#-----------------------------------------------------------------------------
# Error Handling
#-----------------------------------------------------------------------------

#
# Fatal error with formatted message
# Arguments:
#   MESSAGE: Error message
#
function(nexus_fatal_error MESSAGE)
    message(FATAL_ERROR "[NEXUS ERROR] ${MESSAGE}")
endfunction()

#
# Warning with formatted message
# Arguments:
#   MESSAGE: Warning message
#
function(nexus_warning MESSAGE)
    message(WARNING "[NEXUS WARNING] ${MESSAGE}")
endfunction()

#
# Require file to exist
# Arguments:
#   FILE_PATH: Path to file
#   ERROR_MESSAGE: Error message if file not found
#
function(nexus_require_file FILE_PATH ERROR_MESSAGE)
    if(NOT EXISTS ${FILE_PATH})
        nexus_fatal_error("${ERROR_MESSAGE}: ${FILE_PATH}")
    endif()
endfunction()

#
# Require directory to exist
# Arguments:
#   DIR_PATH: Path to directory
#   ERROR_MESSAGE: Error message if directory not found
#
function(nexus_require_directory DIR_PATH ERROR_MESSAGE)
    if(NOT IS_DIRECTORY ${DIR_PATH})
        nexus_fatal_error("${ERROR_MESSAGE}: ${DIR_PATH}")
    endif()
endfunction()

#-----------------------------------------------------------------------------
# Logging Utilities
#-----------------------------------------------------------------------------

# Log levels
set(NEXUS_LOG_LEVEL_VERBOSE 0 CACHE INTERNAL "")
set(NEXUS_LOG_LEVEL_STATUS 1 CACHE INTERNAL "")
set(NEXUS_LOG_LEVEL_WARNING 2 CACHE INTERNAL "")
set(NEXUS_LOG_LEVEL_ERROR 3 CACHE INTERNAL "")

# Current log level (default: STATUS)
set(NEXUS_LOG_LEVEL "STATUS" CACHE STRING "Log level (VERBOSE/STATUS/WARNING/ERROR)")
set_property(CACHE NEXUS_LOG_LEVEL PROPERTY STRINGS VERBOSE STATUS WARNING ERROR)

#
# Log message with level control
# Arguments:
#   LEVEL: Log level (VERBOSE/STATUS/WARNING/ERROR)
#   MESSAGE: Log message
#
function(nexus_log LEVEL MESSAGE)
    set(CURRENT_LEVEL ${NEXUS_LOG_LEVEL_${NEXUS_LOG_LEVEL}})
    set(MSG_LEVEL ${NEXUS_LOG_LEVEL_${LEVEL}})

    if(MSG_LEVEL GREATER_EQUAL CURRENT_LEVEL)
        if(LEVEL STREQUAL "VERBOSE")
            message(VERBOSE "[NEXUS] ${MESSAGE}")
        elseif(LEVEL STREQUAL "STATUS")
            message(STATUS "[NEXUS] ${MESSAGE}")
        elseif(LEVEL STREQUAL "WARNING")
            message(WARNING "[NEXUS] ${MESSAGE}")
        elseif(LEVEL STREQUAL "ERROR")
            message(SEND_ERROR "[NEXUS] ${MESSAGE}")
        endif()
    endif()
endfunction()

#-----------------------------------------------------------------------------
# Input Validation
#-----------------------------------------------------------------------------

#
# Validate that a variable is defined
# Arguments:
#   VAR_NAME: Variable name to check
#   ERROR_MESSAGE: Error message if not defined
#
function(nexus_require_variable VAR_NAME ERROR_MESSAGE)
    if(NOT DEFINED ${VAR_NAME})
        nexus_fatal_error("${ERROR_MESSAGE}: ${VAR_NAME} is not defined")
    endif()
endfunction()

#
# Validate enum value
# Arguments:
#   VALUE: Value to check
#   VAR_NAME: Variable name for error message
#   ALLOWED_VALUES: List of allowed values
#
function(nexus_validate_enum VALUE VAR_NAME)
    cmake_parse_arguments(ARG "" "" "ALLOWED_VALUES" ${ARGN})

    if(NOT VALUE IN_LIST ARG_ALLOWED_VALUES)
        string(JOIN ", " ALLOWED_STR ${ARG_ALLOWED_VALUES})
        nexus_fatal_error(
            "Invalid value for ${VAR_NAME}: '${VALUE}'\n"
            "Allowed values: ${ALLOWED_STR}"
        )
    endif()
endfunction()

#
# Validate numeric range
# Arguments:
#   VALUE: Value to check
#   MIN: Minimum value
#   MAX: Maximum value
#   VAR_NAME: Variable name for error message
#
function(nexus_validate_range VALUE MIN MAX VAR_NAME)
    if(VALUE LESS MIN OR VALUE GREATER MAX)
        nexus_fatal_error(
            "Value out of range for ${VAR_NAME}: ${VALUE}\n"
            "Valid range: [${MIN}, ${MAX}]"
        )
    endif()
endfunction()

#-----------------------------------------------------------------------------
# Path Validation
#-----------------------------------------------------------------------------

#
# Require vendor path to exist with helpful error message
# Arguments:
#   VAR_NAME: Variable name containing the path
#   PATH_HINT: Hint message for resolving the issue
#
function(nexus_require_vendor_path VAR_NAME PATH_HINT)
    if(NOT EXISTS ${${VAR_NAME}})
        message(FATAL_ERROR
            "================================================================\n"
            "ERROR: Vendor Dependency Missing\n"
            "================================================================\n"
            "\n"
            "Path: ${${VAR_NAME}}\n"
            "\n"
            "Solution:\n"
            "  ${PATH_HINT}\n"
            "\n"
            "  Or run:\n"
            "  git submodule update --init --recursive\n"
            "\n"
            "================================================================\n"
        )
    endif()
endfunction()

#-----------------------------------------------------------------------------
# Source File Collection
#-----------------------------------------------------------------------------

#
# Collect source files using glob patterns
# Arguments:
#   OUTPUT_VAR: Variable to store collected sources
#   BASE_DIR: Base directory for pattern matching
#   PATTERNS: List of glob patterns
#   EXCLUDE: List of exclude patterns (optional)
#
function(nexus_collect_sources OUTPUT_VAR)
    cmake_parse_arguments(ARG "" "BASE_DIR" "PATTERNS;EXCLUDE" ${ARGN})

    if(NOT ARG_BASE_DIR)
        nexus_fatal_error("nexus_collect_sources: BASE_DIR is required")
    endif()

    if(NOT ARG_PATTERNS)
        nexus_fatal_error("nexus_collect_sources: PATTERNS is required")
    endif()

    set(SOURCES "")
    foreach(PATTERN ${ARG_PATTERNS})
        file(GLOB_RECURSE PATTERN_SOURCES
            "${ARG_BASE_DIR}/${PATTERN}"
        )
        list(APPEND SOURCES ${PATTERN_SOURCES})
    endforeach()

    # Apply exclusion patterns
    if(ARG_EXCLUDE)
        foreach(EXCLUDE_PATTERN ${ARG_EXCLUDE})
            list(FILTER SOURCES EXCLUDE REGEX "${EXCLUDE_PATTERN}")
        endforeach()
    endif()

    set(${OUTPUT_VAR} ${SOURCES} PARENT_SCOPE)

    list(LENGTH SOURCES SOURCE_COUNT)
    nexus_log(VERBOSE "Collected ${SOURCE_COUNT} source files from ${ARG_BASE_DIR}")
endfunction()

#-----------------------------------------------------------------------------
# Platform Module Configuration
#-----------------------------------------------------------------------------

#
# Configure platform modules based on Kconfig
# Arguments:
#   TARGET: Target to add sources to
#   MODULES: List of module names
#   BASE_DIR: Base directory containing module subdirectories
#   PREFIX: Kconfig prefix (default: HAL)
#
function(nexus_configure_platform_modules TARGET)
    cmake_parse_arguments(ARG "" "BASE_DIR;PREFIX" "MODULES" ${ARGN})

    if(NOT ARG_BASE_DIR)
        nexus_fatal_error("nexus_configure_platform_modules: BASE_DIR is required")
    endif()

    if(NOT ARG_MODULES)
        nexus_fatal_error("nexus_configure_platform_modules: MODULES is required")
    endif()

    if(NOT ARG_PREFIX)
        set(ARG_PREFIX "HAL")
    endif()

    set(ENABLED_MODULES "")

    foreach(MODULE ${ARG_MODULES})
        string(TOUPPER ${MODULE} MODULE_UPPER)
        set(CONFIG_VAR "CONFIG_${ARG_PREFIX}_${MODULE_UPPER}")

        if(${CONFIG_VAR})
            file(GLOB MODULE_SOURCES "${ARG_BASE_DIR}/${MODULE}/*.c")
            if(MODULE_SOURCES)
                target_sources(${TARGET} PRIVATE ${MODULE_SOURCES})
                list(APPEND ENABLED_MODULES ${MODULE})
                nexus_log(VERBOSE "  [+] ${MODULE} module (${CONFIG_VAR}=ON)")
            else()
                nexus_warning("Module ${MODULE} enabled but no sources found in ${ARG_BASE_DIR}/${MODULE}")
            endif()
        else()
            nexus_log(VERBOSE "  [-] ${MODULE} module (${CONFIG_VAR}=OFF)")
        endif()
    endforeach()

    list(LENGTH ENABLED_MODULES ENABLED_COUNT)
    message(STATUS "Configured ${ENABLED_COUNT} platform modules for ${TARGET}")
endfunction()

#-----------------------------------------------------------------------------
# End of NexusHelpers.cmake
#-----------------------------------------------------------------------------
