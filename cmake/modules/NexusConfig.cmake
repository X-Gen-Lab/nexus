##############################################################################
# NexusConfig.cmake - Configuration Management Module
##############################################################################
#
# NexusConfig.cmake
# Kconfig integration and dependency management
# Author: Nexus Team
#
# This module provides:
# - Kconfig configuration loading and parsing
# - Configuration header generation
# - Configuration validation
# - Dependency declaration and resolution
# - SemVer version management
# - Dependency lock files
# - Circular dependency detection
#
# Consolidated from:
# - LoadKconfig.cmake
# - NexusKconfig.cmake
# - NexusDependency.cmake
#
##############################################################################

include_guard(GLOBAL)

##############################################################################
# Kconfig Loading
##############################################################################

#
# Load Kconfig configuration from .config file
# Arguments:
#   CONFIG_FILE: Path to .config file
#
function(load_kconfig CONFIG_FILE)
    if(NOT EXISTS ${CONFIG_FILE})
        message(WARNING "Config file not found: ${CONFIG_FILE}")
        return()
    endif()

    message(STATUS "Loading Kconfig from: ${CONFIG_FILE}")

    file(STRINGS ${CONFIG_FILE} CONFIG_LINES)

    foreach(LINE ${CONFIG_LINES})
        if(LINE MATCHES "^#" OR LINE MATCHES "^$")
            continue()
        endif()

        if(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=y$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(CONFIG_${VAR_NAME} TRUE PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} TRUE CACHE BOOL "Kconfig option" FORCE)
        elseif(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=n$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(CONFIG_${VAR_NAME} FALSE PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} FALSE CACHE BOOL "Kconfig option" FORCE)
        elseif(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=\"([^\"]*)\"$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(VAR_VALUE ${CMAKE_MATCH_2})
            set(CONFIG_${VAR_NAME} "${VAR_VALUE}" PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} "${VAR_VALUE}" CACHE STRING "Kconfig option" FORCE)
        elseif(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=(0x[0-9A-Fa-f]+)$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(VAR_VALUE ${CMAKE_MATCH_2})
            set(CONFIG_${VAR_NAME} ${VAR_VALUE} PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} ${VAR_VALUE} CACHE STRING "Kconfig option" FORCE)
        elseif(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=([0-9]+)$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(VAR_VALUE ${CMAKE_MATCH_2})
            set(CONFIG_${VAR_NAME} ${VAR_VALUE} PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} ${VAR_VALUE} CACHE STRING "Kconfig option" FORCE)
        elseif(LINE MATCHES "^# CONFIG_([A-Za-z0-9_]+) is not set$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(CONFIG_${VAR_NAME} FALSE PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} FALSE CACHE BOOL "Kconfig option" FORCE)
        endif()
    endforeach()

    message(STATUS "Loaded Kconfig configuration")
endfunction()

#
# Apply Kconfig configuration to CMake variables
#
function(apply_kconfig_to_cmake)
    message(STATUS "Applying Kconfig configuration to CMake...")

    if(DEFINED CONFIG_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "${CONFIG_BUILD_TYPE}" CACHE STRING "Build type from Kconfig" FORCE)
        message(STATUS "  Build Type: ${CONFIG_BUILD_TYPE}")
    endif()

    if(DEFINED CONFIG_BUILD_TESTS)
        set(NEXUS_BUILD_TESTS ${CONFIG_BUILD_TESTS} CACHE BOOL "Build tests from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_BUILD_EXAMPLES)
        set(NEXUS_BUILD_EXAMPLES ${CONFIG_BUILD_EXAMPLES} CACHE BOOL "Build examples from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_ENABLE_COVERAGE)
        set(NEXUS_ENABLE_COVERAGE ${CONFIG_ENABLE_COVERAGE} CACHE BOOL "Enable coverage from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_PLATFORM_NAME)
        set(NEXUS_PLATFORM "${CONFIG_PLATFORM_NAME}" CACHE STRING "Platform from Kconfig" FORCE)
        message(STATUS "  Platform: ${CONFIG_PLATFORM_NAME}")
    endif()

    if(DEFINED CONFIG_TOOLCHAIN_NAME)
        set(NEXUS_TOOLCHAIN_NAME "${CONFIG_TOOLCHAIN_NAME}" CACHE STRING "Toolchain from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_CPU_ARCH AND CONFIG_CPU_ARCH)
        set(NEXUS_CPU_ARCH "${CONFIG_CPU_ARCH}" CACHE STRING "CPU architecture from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_FPU_TYPE)
        set(NEXUS_FPU_TYPE "${CONFIG_FPU_TYPE}" CACHE STRING "FPU type from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_LINKER_SCRIPT AND CONFIG_LINKER_SCRIPT)
        set(NEXUS_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/${CONFIG_LINKER_SCRIPT}" CACHE FILEPATH "Linker script from Kconfig" FORCE)
    endif()

    message(STATUS "Kconfig configuration applied to CMake")

    # Note: Compiler and linker flags are applied separately via NexusCompilerConfig module
    # This is done after platform detection to ensure correct toolchain variables are set
endfunction()

##############################################################################
# Kconfig Integration
##############################################################################

#
# Load Kconfig configuration and generate header
# Arguments:
#   KCONFIG_FILE: Path to root Kconfig file
#   CONFIG_FILE: Path to .config file
#   OUTPUT_HEADER: Path to generated header file
#
function(nexus_load_kconfig)
    cmake_parse_arguments(
        ARG
        ""
        "KCONFIG_FILE;CONFIG_FILE;OUTPUT_HEADER"
        ""
        ${ARGN}
    )

    if(NOT ARG_KCONFIG_FILE)
        message(FATAL_ERROR "nexus_load_kconfig: KCONFIG_FILE argument is required")
    endif()

    if(NOT ARG_CONFIG_FILE)
        message(FATAL_ERROR "nexus_load_kconfig: CONFIG_FILE argument is required")
    endif()

    if(NOT ARG_OUTPUT_HEADER)
        message(FATAL_ERROR "nexus_load_kconfig: OUTPUT_HEADER argument is required")
    endif()

    if(NOT Python3_EXECUTABLE)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
    endif()

    set(GENERATOR_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/kconfig/generate_config.py")

    if(NOT EXISTS ${GENERATOR_SCRIPT})
        message(FATAL_ERROR "Configuration generator script not found: ${GENERATOR_SCRIPT}")
    endif()

    file(GLOB_RECURSE KCONFIG_FILES
        "${CMAKE_SOURCE_DIR}/Kconfig"
        "${CMAKE_SOURCE_DIR}/*/Kconfig"
        "${CMAKE_SOURCE_DIR}/*/*/Kconfig"
    )

    if(EXISTS ${ARG_CONFIG_FILE})
        list(APPEND KCONFIG_FILES ${ARG_CONFIG_FILE})
    endif()

    foreach(KCONFIG_FILE ${KCONFIG_FILES})
        if(EXISTS ${KCONFIG_FILE})
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${KCONFIG_FILE})
        endif()
    endforeach()

    message(STATUS "Generating configuration header from Kconfig...")

    if(EXISTS ${ARG_CONFIG_FILE})
        execute_process(
            COMMAND ${Python3_EXECUTABLE} ${GENERATOR_SCRIPT}
                    --kconfig ${ARG_KCONFIG_FILE}
                    --config ${ARG_CONFIG_FILE}
                    --output ${ARG_OUTPUT_HEADER}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE RESULT
            OUTPUT_VARIABLE OUTPUT
            ERROR_VARIABLE ERROR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
        )

        if(NOT RESULT EQUAL 0)
            message(WARNING "Failed to generate config from .config file, using defaults")
            execute_process(
                COMMAND ${Python3_EXECUTABLE} ${GENERATOR_SCRIPT}
                        --kconfig ${ARG_KCONFIG_FILE}
                        --default
                        --output ${ARG_OUTPUT_HEADER}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE RESULT
                ERROR_QUIET
            )
        endif()
    else()
        message(STATUS "No .config file found, generating default configuration...")
        execute_process(
            COMMAND ${Python3_EXECUTABLE} ${GENERATOR_SCRIPT}
                    --kconfig ${ARG_KCONFIG_FILE}
                    --default
                    --output ${ARG_OUTPUT_HEADER}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE RESULT
            ERROR_QUIET
        )
    endif()

    set_source_files_properties(${ARG_OUTPUT_HEADER} PROPERTIES GENERATED TRUE)

    list(LENGTH KCONFIG_FILES KCONFIG_COUNT)
    message(STATUS "Generated configuration header: ${ARG_OUTPUT_HEADER}")
    message(STATUS "Tracking ${KCONFIG_COUNT} Kconfig/config files for changes")
endfunction()

#
# Validate Kconfig configuration
# Arguments:
#   CONFIG_FILE: Path to .config file
#
function(nexus_validate_kconfig)
    cmake_parse_arguments(
        ARG
        ""
        "CONFIG_FILE"
        ""
        ${ARGN}
    )

    if(NOT ARG_CONFIG_FILE)
        set(ARG_CONFIG_FILE ${NEXUS_CONFIG_FILE})
    endif()

    if(NOT EXISTS ${ARG_CONFIG_FILE})
        message(STATUS "No .config file to validate, using defaults")
        return()
    endif()

    message(STATUS "Validating Kconfig configuration...")

    if(NOT DEFINED CONFIG_PLATFORM_NAME AND NOT DEFINED NEXUS_PLATFORM)
        message(WARNING "Platform not configured in Kconfig")
    endif()

    # Validate FPU configuration
    if(DEFINED CONFIG_FPU_TYPE AND CONFIG_FPU_TYPE AND NOT CONFIG_FPU_TYPE STREQUAL "")
        # Check if CPU does not support FPU (only M0/M0+/M3 don't support FPU)
        if(CONFIG_CPU_CORTEX_M0 OR CONFIG_CPU_CORTEX_M0PLUS OR CONFIG_CPU_CORTEX_M3)
            message(FATAL_ERROR "Configuration conflict: FPU configured but CPU does not support FPU")
        endif()
    endif()

    message(STATUS "Kconfig configuration validation passed")
endfunction()

##############################################################################
# Dependency Management
##############################################################################

set(NEXUS_DEPENDENCY_GRAPH "" CACHE INTERNAL "Dependency graph")
set(NEXUS_DEPENDENCY_VERSIONS "" CACHE INTERNAL "Dependency versions")
set(NEXUS_DEPENDENCY_INFO "" CACHE INTERNAL "Dependency information")
set(NEXUS_LOCK_FILE "${CMAKE_SOURCE_DIR}/nexus.lock" CACHE FILEPATH "Dependency lock file")

#
# Declare a dependency
# Arguments:
#   NAME: Dependency name
#   VERSION: Version specification (SemVer)
#   REQUIRED: Dependency is required (optional)
#   COMPONENTS: Required components (optional)
#
function(nexus_declare_dependency)
    cmake_parse_arguments(
        ARG
        "REQUIRED"
        "NAME;VERSION"
        "COMPONENTS"
        ${ARGN}
    )

    if(NOT ARG_NAME)
        message(FATAL_ERROR "nexus_declare_dependency: NAME argument is required")
    endif()

    if(NOT ARG_VERSION)
        message(FATAL_ERROR "nexus_declare_dependency: VERSION argument is required")
    endif()

    set(DEP_INFO "name=${ARG_NAME};version=${ARG_VERSION}")

    if(ARG_REQUIRED)
        string(APPEND DEP_INFO ";required=TRUE")
    else()
        string(APPEND DEP_INFO ";required=FALSE")
    endif()

    if(ARG_COMPONENTS)
        string(JOIN "," COMPONENTS_STR ${ARG_COMPONENTS})
        string(APPEND DEP_INFO ";components=${COMPONENTS_STR}")
    endif()

    list(APPEND NEXUS_DEPENDENCY_INFO ${DEP_INFO})
    set(NEXUS_DEPENDENCY_INFO ${NEXUS_DEPENDENCY_INFO} CACHE INTERNAL "Dependency information")

    message(STATUS "Declared dependency: ${ARG_NAME} (${ARG_VERSION})")

    nexus_resolve_dependency(${ARG_NAME} ${ARG_VERSION})
endfunction()

#
# Resolve a dependency
# Arguments:
#   NAME: Dependency name
#   VERSION: Version specification
#
function(nexus_resolve_dependency NAME VERSION)
    if(TARGET ${NAME})
        message(STATUS "Dependency already resolved: ${NAME}")
        return()
    endif()

    if(EXISTS ${NEXUS_LOCK_FILE})
        nexus_load_lockfile()
        nexus_get_locked_version(${NAME} LOCKED_VERSION)
        if(LOCKED_VERSION)
            nexus_version_satisfies(${LOCKED_VERSION} ${VERSION} SATISFIES)
            if(SATISFIES)
                message(STATUS "Using locked version: ${NAME} ${LOCKED_VERSION}")
                set(VERSION ${LOCKED_VERSION})
            endif()
        endif()
    endif()

    find_package(${NAME} ${VERSION} QUIET)

    if(${NAME}_FOUND)
        message(STATUS "Found dependency: ${NAME} ${${NAME}_VERSION}")
        list(APPEND NEXUS_DEPENDENCY_VERSIONS "${NAME}=${${NAME}_VERSION}")
        set(NEXUS_DEPENDENCY_VERSIONS ${NEXUS_DEPENDENCY_VERSIONS} CACHE INTERNAL "Dependency versions")
    else()
        message(WARNING "Dependency not found: ${NAME} (${VERSION})")
    endif()
endfunction()

##############################################################################
# SemVer Version Management
##############################################################################

#
# Parse SemVer version string
#
function(nexus_parse_semver VERSION MAJOR_VAR MINOR_VAR PATCH_VAR OPERATOR_VAR)
    set(OPERATOR "")
    set(VERSION_NUM ${VERSION})

    if(VERSION MATCHES "^([><=^~]+)(.+)$")
        set(OPERATOR ${CMAKE_MATCH_1})
        set(VERSION_NUM ${CMAKE_MATCH_2})
    endif()

    if(VERSION_NUM MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
        set(MAJOR ${CMAKE_MATCH_1})
        set(MINOR ${CMAKE_MATCH_2})
        set(PATCH ${CMAKE_MATCH_3})
    elseif(VERSION_NUM MATCHES "^([0-9]+)\\.([0-9]+)")
        set(MAJOR ${CMAKE_MATCH_1})
        set(MINOR ${CMAKE_MATCH_2})
        set(PATCH 0)
    elseif(VERSION_NUM MATCHES "^([0-9]+)")
        set(MAJOR ${CMAKE_MATCH_1})
        set(MINOR 0)
        set(PATCH 0)
    else()
        message(FATAL_ERROR "Invalid version format: ${VERSION}")
    endif()

    set(${MAJOR_VAR} ${MAJOR} PARENT_SCOPE)
    set(${MINOR_VAR} ${MINOR} PARENT_SCOPE)
    set(${PATCH_VAR} ${PATCH} PARENT_SCOPE)
    set(${OPERATOR_VAR} ${OPERATOR} PARENT_SCOPE)
endfunction()

#
# Check if version satisfies constraint
#
function(nexus_version_satisfies VERSION CONSTRAINT SATISFIES_VAR)
    nexus_parse_semver(${CONSTRAINT} REQ_MAJOR REQ_MINOR REQ_PATCH OPERATOR)
    nexus_parse_semver(${VERSION} ACT_MAJOR ACT_MINOR ACT_PATCH ACT_OP)

    set(SATISFIES FALSE)

    if(NOT OPERATOR OR OPERATOR STREQUAL "=")
        if(ACT_MAJOR EQUAL REQ_MAJOR AND ACT_MINOR EQUAL REQ_MINOR AND ACT_PATCH EQUAL REQ_PATCH)
            set(SATISFIES TRUE)
        endif()
    elseif(OPERATOR STREQUAL ">=")
        if(ACT_MAJOR GREATER REQ_MAJOR OR
           (ACT_MAJOR EQUAL REQ_MAJOR AND ACT_MINOR GREATER REQ_MINOR) OR
           (ACT_MAJOR EQUAL REQ_MAJOR AND ACT_MINOR EQUAL REQ_MINOR AND ACT_PATCH GREATER_EQUAL REQ_PATCH))
            set(SATISFIES TRUE)
        endif()
    elseif(OPERATOR STREQUAL "^")
        if(ACT_MAJOR EQUAL REQ_MAJOR)
            if(ACT_MINOR GREATER REQ_MINOR OR
               (ACT_MINOR EQUAL REQ_MINOR AND ACT_PATCH GREATER_EQUAL REQ_PATCH))
                set(SATISFIES TRUE)
            endif()
        endif()
    endif()

    set(${SATISFIES_VAR} ${SATISFIES} PARENT_SCOPE)
endfunction()

##############################################################################
# Lock File Management
##############################################################################

set(NEXUS_LOCKFILE_DATA "" CACHE INTERNAL "Parsed lock file data")

#
# Load dependency lock file
#
function(nexus_load_lockfile)
    if(NOT EXISTS ${NEXUS_LOCK_FILE})
        message(STATUS "Lock file not found: ${NEXUS_LOCK_FILE}")
        return()
    endif()

    message(STATUS "Loading lock file: ${NEXUS_LOCK_FILE}")

    file(READ ${NEXUS_LOCK_FILE} LOCK_CONTENT)

    if(LOCK_CONTENT MATCHES "\"dependencies\"[[:space:]]*:[[:space:]]*\\{([^}]+)\\}")
        set(DEPS_SECTION ${CMAKE_MATCH_1})
        string(REGEX MATCHALL "\"([^\"]+)\"[[:space:]]*:[[:space:]]*\\{([^}]+)\\}" DEP_MATCHES ${DEPS_SECTION})

        set(LOCKFILE_DATA "")
        foreach(DEP_MATCH ${DEP_MATCHES})
            if(DEP_MATCH MATCHES "\"([^\"]+)\"[[:space:]]*:[[:space:]]*\\{([^}]+)\\}")
                set(DEP_NAME ${CMAKE_MATCH_1})
                set(DEP_INFO ${CMAKE_MATCH_2})

                if(DEP_INFO MATCHES "\"version\"[[:space:]]*:[[:space:]]*\"([^\"]+)\"")
                    set(DEP_VERSION ${CMAKE_MATCH_1})
                endif()

                if(DEP_INFO MATCHES "\"hash\"[[:space:]]*:[[:space:]]*\"([^\"]+)\"")
                    set(DEP_HASH ${CMAKE_MATCH_1})
                endif()

                list(APPEND LOCKFILE_DATA "${DEP_NAME}:${DEP_VERSION}:${DEP_HASH}")
            endif()
        endforeach()

        set(NEXUS_LOCKFILE_DATA ${LOCKFILE_DATA} CACHE INTERNAL "Parsed lock file data")
    endif()

    message(STATUS "Lock file loaded")
endfunction()

#
# Get locked version for a dependency
#
function(nexus_get_locked_version NAME VERSION_VAR)
    set(LOCKED_VERSION "")

    foreach(ENTRY ${NEXUS_LOCKFILE_DATA})
        string(REPLACE ":" ";" PARTS ${ENTRY})
        list(GET PARTS 0 ENTRY_NAME)

        if(ENTRY_NAME STREQUAL NAME)
            list(GET PARTS 1 LOCKED_VERSION)
            break()
        endif()
    endforeach()

    set(${VERSION_VAR} ${LOCKED_VERSION} PARENT_SCOPE)
endfunction()

##############################################################################
# Circular Dependency Detection
##############################################################################

#
# Check for circular dependencies
#
function(nexus_check_circular_dependencies OUTPUT_VAR)
    set(HAS_CYCLE FALSE)

    set(ALL_MODULES "")
    foreach(ENTRY ${NEXUS_DEPENDENCY_GRAPH})
        string(REPLACE ":" ";" PARTS ${ENTRY})
        list(GET PARTS 0 MODULE)
        list(APPEND ALL_MODULES ${MODULE})
    endforeach()

    foreach(MODULE ${ALL_MODULES})
        set(VISITED "")
        set(REC_STACK "")
        nexus_has_cycle_dfs(${MODULE} VISITED REC_STACK CYCLE_FOUND)

        if(CYCLE_FOUND)
            set(HAS_CYCLE TRUE)
            message(WARNING "Circular dependency detected starting from: ${MODULE}")
            break()
        endif()
    endforeach()

    set(${OUTPUT_VAR} ${HAS_CYCLE} PARENT_SCOPE)
endfunction()

#
# DFS helper for cycle detection
#
function(nexus_has_cycle_dfs NODE VISITED_VAR REC_STACK_VAR CYCLE_FOUND_VAR)
    set(VISITED ${${VISITED_VAR}})
    set(REC_STACK ${${REC_STACK_VAR}})

    list(FIND REC_STACK ${NODE} IN_REC_STACK)
    if(NOT IN_REC_STACK EQUAL -1)
        set(${CYCLE_FOUND_VAR} TRUE PARENT_SCOPE)
        return()
    endif()

    list(FIND VISITED ${NODE} IN_VISITED)
    if(NOT IN_VISITED EQUAL -1)
        set(${CYCLE_FOUND_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    list(APPEND VISITED ${NODE})
    list(APPEND REC_STACK ${NODE})

    set(NODE_DEPS "")
    foreach(ENTRY ${NEXUS_DEPENDENCY_GRAPH})
        string(REPLACE ":" ";" PARTS ${ENTRY})
        list(GET PARTS 0 MODULE)

        if(MODULE STREQUAL NODE)
            list(LENGTH PARTS PARTS_LEN)
            if(PARTS_LEN GREATER 1)
                list(GET PARTS 1 DEPS_STR)
                if(DEPS_STR)
                    string(REPLACE ";" "," DEPS_STR_TEMP ${DEPS_STR})
                    string(REPLACE "," ";" NODE_DEPS ${DEPS_STR_TEMP})
                endif()
            endif()
            break()
        endif()
    endforeach()

    foreach(DEP ${NODE_DEPS})
        nexus_has_cycle_dfs(${DEP} VISITED REC_STACK CYCLE_FOUND)
        if(CYCLE_FOUND)
            set(${CYCLE_FOUND_VAR} TRUE PARENT_SCOPE)
            return()
        endif()
    endforeach()

    list(REMOVE_ITEM REC_STACK ${NODE})

    set(${VISITED_VAR} ${VISITED} PARENT_SCOPE)
    set(${REC_STACK_VAR} ${REC_STACK} PARENT_SCOPE)
    set(${CYCLE_FOUND_VAR} FALSE PARENT_SCOPE)
endfunction()

message(STATUS "NexusConfig module loaded")

##############################################################################
# End of NexusConfig.cmake
##############################################################################


##############################################################################
# Vendor Path Management (Deprecated - Use NexusVendor.cmake)
##############################################################################

# Legacy vendor path management functions are deprecated.
# New code should use NexusVendor.cmake module instead.
# These functions are kept for backward compatibility.

#
# Configure and validate vendor paths for a platform (DEPRECATED)
# Use nexus_configure_stm32_vendors() from NexusVendor.cmake instead
#
function(nexus_configure_vendor_paths PLATFORM)
    message(DEPRECATION
        "nexus_configure_vendor_paths() is deprecated. "
        "Use nexus_configure_stm32_vendors() from NexusVendor.cmake instead."
    )

    cmake_parse_arguments(ARG "" "SERIES" "" ${ARGN})

    # Forward to new implementation if NexusVendor is loaded
    if(COMMAND nexus_configure_stm32_vendors AND PLATFORM STREQUAL "stm32")
        nexus_configure_stm32_vendors(${ARG_SERIES})

        # Export to parent scope for compatibility
        set(CMSIS_CORE_DIR ${CMSIS_CORE_DIR} PARENT_SCOPE)
        set(CMSIS_DEVICE_DIR ${CMSIS_DEVICE_DIR} PARENT_SCOPE)
        set(HAL_DRIVER_DIR ${HAL_DRIVER_DIR} PARENT_SCOPE)
    endif()
endfunction()

#
# Detect and configure STM32 vendor libraries (DEPRECATED)
# Use nexus_configure_stm32_vendors() from NexusVendor.cmake instead
#
function(nexus_configure_stm32_vendor_libs SERIES)
    message(DEPRECATION
        "nexus_configure_stm32_vendor_libs() is deprecated. "
        "Use nexus_configure_stm32_vendors() from NexusVendor.cmake instead."
    )

    # Forward to new implementation if NexusVendor is loaded
    if(COMMAND nexus_configure_stm32_vendors)
        nexus_configure_stm32_vendors(${SERIES})
    endif()
endfunction()

##############################################################################
# End of NexusConfig.cmake
##############################################################################
