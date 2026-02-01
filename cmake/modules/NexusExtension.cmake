#-----------------------------------------------------------------------------
# NexusExtension.cmake - Extension and Isolation Module
#-----------------------------------------------------------------------------
# Plugin system and build isolation
# Author: Nexus Team
#
# This module provides:
# - Plugin registration and loading
# - Hook system for build lifecycle
# - Plugin configuration management
# - Environment variable isolation
# - File system access control
# - Resource limits and audit logging
#
# Consolidated from:
# - NexusPlugin.cmake
# - NexusIsolation.cmake
#
#-----------------------------------------------------------------------------

include_guard(GLOBAL)

#-----------------------------------------------------------------------------
# Plugin System
#-----------------------------------------------------------------------------

if(NOT DEFINED NEXUS_PLUGIN_REGISTRY)
    set(NEXUS_PLUGIN_REGISTRY "" CACHE INTERNAL "List of registered plugins")
endif()

if(NOT DEFINED NEXUS_PLUGIN_HOOKS)
    set(NEXUS_PLUGIN_HOOKS "" CACHE INTERNAL "Plugin hook registry")
endif()

# Available hook points
set(NEXUS_HOOK_PRE_CONFIGURE "pre_configure" CACHE INTERNAL "Before CMake configuration")
set(NEXUS_HOOK_POST_CONFIGURE "post_configure" CACHE INTERNAL "After CMake configuration")
set(NEXUS_HOOK_PRE_BUILD "pre_build" CACHE INTERNAL "Before build starts")
set(NEXUS_HOOK_POST_BUILD "post_build" CACHE INTERNAL "After build completes")
set(NEXUS_HOOK_PRE_TEST "pre_test" CACHE INTERNAL "Before tests run")
set(NEXUS_HOOK_POST_TEST "post_test" CACHE INTERNAL "After tests complete")

#
# Register a plugin with the build system
# Arguments:
#   NAME: Plugin name (unique identifier)
#   VERSION: Plugin version (SemVer format)
#   DESCRIPTION: Brief description
#   AUTHOR: Plugin author
#   SCRIPT: Path to plugin CMake script
#   HOOKS: List of hooks this plugin implements
#   DEPENDENCIES: List of required plugins
#
function(nexus_register_plugin)
    cmake_parse_arguments(
        PLUGIN
        ""
        "NAME;VERSION;DESCRIPTION;AUTHOR;SCRIPT"
        "HOOKS;DEPENDENCIES"
        ${ARGN}
    )

    if(NOT PLUGIN_NAME)
        message(FATAL_ERROR "Plugin NAME is required")
    endif()

    if(NOT PLUGIN_VERSION)
        message(FATAL_ERROR "Plugin VERSION is required for ${PLUGIN_NAME}")
    endif()

    if(NOT PLUGIN_SCRIPT)
        message(FATAL_ERROR "Plugin SCRIPT is required for ${PLUGIN_NAME}")
    endif()

    if("${PLUGIN_NAME}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(WARNING "Plugin ${PLUGIN_NAME} is already registered, skipping")
        return()
    endif()

    if(NOT EXISTS "${PLUGIN_SCRIPT}")
        message(FATAL_ERROR "Plugin script not found: ${PLUGIN_SCRIPT}")
    endif()

    list(APPEND NEXUS_PLUGIN_REGISTRY "${PLUGIN_NAME}")
    set(NEXUS_PLUGIN_REGISTRY "${NEXUS_PLUGIN_REGISTRY}" CACHE INTERNAL "List of registered plugins")

    set(NEXUS_PLUGIN_${PLUGIN_NAME}_VERSION "${PLUGIN_VERSION}" CACHE INTERNAL "")
    set(NEXUS_PLUGIN_${PLUGIN_NAME}_DESCRIPTION "${PLUGIN_DESCRIPTION}" CACHE INTERNAL "")
    set(NEXUS_PLUGIN_${PLUGIN_NAME}_AUTHOR "${PLUGIN_AUTHOR}" CACHE INTERNAL "")
    set(NEXUS_PLUGIN_${PLUGIN_NAME}_SCRIPT "${PLUGIN_SCRIPT}" CACHE INTERNAL "")
    set(NEXUS_PLUGIN_${PLUGIN_NAME}_HOOKS "${PLUGIN_HOOKS}" CACHE INTERNAL "")
    set(NEXUS_PLUGIN_${PLUGIN_NAME}_DEPENDENCIES "${PLUGIN_DEPENDENCIES}" CACHE INTERNAL "")
    set(NEXUS_PLUGIN_${PLUGIN_NAME}_LOADED FALSE CACHE INTERNAL "")

    message(STATUS "Registered plugin: ${PLUGIN_NAME} v${PLUGIN_VERSION}")
endfunction()

#
# Register a hook implementation for a plugin
# Arguments:
#   PLUGIN: Plugin name
#   HOOK: Hook point name
#   FUNCTION: CMake function to call
#
function(nexus_register_hook)
    cmake_parse_arguments(
        HOOK
        ""
        "PLUGIN;HOOK;FUNCTION"
        ""
        ${ARGN}
    )

    if(NOT HOOK_PLUGIN)
        message(FATAL_ERROR "PLUGIN is required for hook registration")
    endif()

    if(NOT HOOK_HOOK)
        message(FATAL_ERROR "HOOK is required for hook registration")
    endif()

    if(NOT HOOK_FUNCTION)
        message(FATAL_ERROR "FUNCTION is required for hook registration")
    endif()

    if(NOT "${HOOK_PLUGIN}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(FATAL_ERROR "Plugin ${HOOK_PLUGIN} is not registered")
    endif()

    set(HOOK_ENTRY "${HOOK_PLUGIN}:${HOOK_HOOK}:${HOOK_FUNCTION}")

    list(APPEND NEXUS_PLUGIN_HOOKS "${HOOK_ENTRY}")
    set(NEXUS_PLUGIN_HOOKS "${NEXUS_PLUGIN_HOOKS}" CACHE INTERNAL "Plugin hook registry")

    message(VERBOSE "Registered hook: ${HOOK_PLUGIN} -> ${HOOK_HOOK} -> ${HOOK_FUNCTION}")
endfunction()

#
# Load a plugin and execute its initialization
# Arguments:
#   NAME: Plugin name to load
#
function(nexus_load_plugin NAME)
    if(NOT "${NAME}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(FATAL_ERROR "Plugin ${NAME} is not registered")
    endif()

    if(NEXUS_PLUGIN_${NAME}_LOADED)
        message(VERBOSE "Plugin ${NAME} is already loaded")
        return()
    endif()

    # Load dependencies first
    if(DEFINED NEXUS_PLUGIN_${NAME}_DEPENDENCIES)
        foreach(DEP ${NEXUS_PLUGIN_${NAME}_DEPENDENCIES})
            if(NOT "${DEP}" IN_LIST NEXUS_PLUGIN_REGISTRY)
                message(FATAL_ERROR "Plugin ${NAME} depends on ${DEP}, but it is not registered")
            endif()
            nexus_load_plugin(${DEP})
        endforeach()
    endif()

    set(SCRIPT_PATH ${NEXUS_PLUGIN_${NAME}_SCRIPT})
    set(VERSION ${NEXUS_PLUGIN_${NAME}_VERSION})

    message(STATUS "Loading plugin: ${NAME} v${VERSION}")

    if(NOT EXISTS "${SCRIPT_PATH}")
        message(FATAL_ERROR "Plugin script not found: ${SCRIPT_PATH}")
    endif()

    # Verify plugin signature if available
    get_filename_component(SCRIPT_DIR "${SCRIPT_PATH}" DIRECTORY)
    get_filename_component(SCRIPT_NAME "${SCRIPT_PATH}" NAME)
    set(SIGNATURE_FILE "${SCRIPT_DIR}/${SCRIPT_NAME}.sha256")

    if(EXISTS "${SIGNATURE_FILE}")
        message(VERBOSE "Verifying plugin signature: ${SIGNATURE_FILE}")

        file(SHA256 "${SCRIPT_PATH}" ACTUAL_HASH)
        file(READ "${SIGNATURE_FILE}" EXPECTED_HASH)
        string(STRIP "${EXPECTED_HASH}" EXPECTED_HASH)

        if(NOT "${ACTUAL_HASH}" STREQUAL "${EXPECTED_HASH}")
            message(FATAL_ERROR
                "Plugin signature verification failed for ${NAME}\n"
                "Expected: ${EXPECTED_HASH}\n"
                "Actual:   ${ACTUAL_HASH}")
        endif()

        message(STATUS "Plugin signature verified: ${NAME}")
    else()
        message(WARNING "No signature file found for plugin ${NAME}")
    endif()

    include("${SCRIPT_PATH}")

    if(COMMAND ${NAME}_plugin_init)
        message(VERBOSE "Calling ${NAME}_plugin_init()")
        cmake_language(CALL ${NAME}_plugin_init)
    endif()

    set(NEXUS_PLUGIN_${NAME}_LOADED TRUE CACHE INTERNAL "")

    message(STATUS "Plugin loaded successfully: ${NAME}")
endfunction()

#
# Invoke all hooks registered for a specific hook point
# Arguments:
#   HOOK: Hook point name
#   CONTEXT: Optional context data
#
function(nexus_invoke_hooks HOOK)
    cmake_parse_arguments(
        INVOKE
        ""
        ""
        "CONTEXT"
        ${ARGN}
    )

    message(VERBOSE "Invoking hooks for: ${HOOK}")

    set(HOOKS_INVOKED 0)

    foreach(HOOK_ENTRY ${NEXUS_PLUGIN_HOOKS})
        string(REPLACE ":" ";" HOOK_PARTS "${HOOK_ENTRY}")
        list(GET HOOK_PARTS 0 PLUGIN_NAME)
        list(GET HOOK_PARTS 1 HOOK_NAME)
        list(GET HOOK_PARTS 2 HOOK_FUNCTION)

        if("${HOOK_NAME}" STREQUAL "${HOOK}")
            if(NOT NEXUS_PLUGIN_${PLUGIN_NAME}_LOADED)
                message(WARNING "Plugin ${PLUGIN_NAME} is not loaded, skipping hook ${HOOK_NAME}")
                continue()
            endif()

            message(VERBOSE "Calling hook: ${PLUGIN_NAME}.${HOOK_FUNCTION}")

            if(COMMAND ${HOOK_FUNCTION})
                cmake_language(CALL ${HOOK_FUNCTION} ${INVOKE_CONTEXT})
                math(EXPR HOOKS_INVOKED "${HOOKS_INVOKED} + 1")
            else()
                message(WARNING "Hook function ${HOOK_FUNCTION} not found for plugin ${PLUGIN_NAME}")
            endif()
        endif()
    endforeach()

    message(VERBOSE "Invoked ${HOOKS_INVOKED} hooks for ${HOOK}")
endfunction()

#-----------------------------------------------------------------------------
# Build Isolation
#-----------------------------------------------------------------------------

set(NEXUS_ISOLATION_ENABLED FALSE CACHE BOOL "Enable build isolation")
set(NEXUS_ISOLATION_STRICT FALSE CACHE BOOL "Enable strict isolation mode")
set(NEXUS_ISOLATION_ENV_WHITELIST "" CACHE STRING "Whitelisted environment variables")
set(NEXUS_ISOLATION_AUDIT_ENABLED FALSE CACHE BOOL "Enable audit logging")
set(NEXUS_ISOLATION_AUDIT_FILE "${CMAKE_BINARY_DIR}/nexus_audit.log")

#
# Initialize environment isolation
#
function(nexus_isolation_init_environment)
    if(NOT NEXUS_ISOLATION_ENABLED)
        return()
    endif()

    message(STATUS "[Nexus Isolation] Initializing environment isolation...")

    set(DEFAULT_WHITELIST
        "PATH" "HOME" "USER" "USERNAME" "USERPROFILE"
        "TEMP" "TMP" "TMPDIR" "SystemRoot" "COMSPEC" "SHELL"
        "LANG" "LC_ALL" "CMAKE_PREFIX_PATH" "CMAKE_MODULE_PATH"
    )

    set(WHITELIST ${DEFAULT_WHITELIST})
    if(NEXUS_ISOLATION_ENV_WHITELIST)
        list(APPEND WHITELIST ${NEXUS_ISOLATION_ENV_WHITELIST})
    endif()

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E environment
        OUTPUT_VARIABLE ENV_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    string(REPLACE "\n" ";" ENV_LINES "${ENV_OUTPUT}")

    set(CLEANED_COUNT 0)
    set(KEPT_COUNT 0)

    foreach(ENV_LINE ${ENV_LINES})
        string(REGEX MATCH "^([^=]+)=" VAR_MATCH "${ENV_LINE}")
        if(VAR_MATCH)
            set(VAR_NAME "${CMAKE_MATCH_1}")

            list(FIND WHITELIST "${VAR_NAME}" WHITELIST_INDEX)

            if(WHITELIST_INDEX EQUAL -1)
                if(NEXUS_ISOLATION_STRICT)
                    unset(ENV{${VAR_NAME}})
                    math(EXPR CLEANED_COUNT "${CLEANED_COUNT} + 1")

                    if(NEXUS_ISOLATION_AUDIT_ENABLED)
                        nexus_isolation_audit_log("ENV_CLEANED" "Cleaned environment variable: ${VAR_NAME}")
                    endif()
                endif()
            else()
                math(EXPR KEPT_COUNT "${KEPT_COUNT} + 1")
            endif()
        endif()
    endforeach()

    message(STATUS "[Nexus Isolation] Environment isolation complete")
    message(STATUS "  - Kept variables: ${KEPT_COUNT}")
    if(NEXUS_ISOLATION_STRICT)
        message(STATUS "  - Cleaned variables: ${CLEANED_COUNT}")
    endif()
endfunction()

#
# Initialize file system isolation
#
function(nexus_isolation_init_filesystem)
    if(NOT NEXUS_ISOLATION_ENABLED)
        return()
    endif()

    message(STATUS "[Nexus Isolation] Initializing file system isolation...")

    set(DEFAULT_FS_WHITELIST
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
    )

    if(CMAKE_C_COMPILER)
        get_filename_component(COMPILER_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
        list(APPEND DEFAULT_FS_WHITELIST "${COMPILER_DIR}")
    endif()

    if(WIN32)
        list(APPEND DEFAULT_FS_WHITELIST "C:/Windows/System32")
    else()
        list(APPEND DEFAULT_FS_WHITELIST "/usr" "/lib" "/lib64")
    endif()

    message(STATUS "[Nexus Isolation] File system isolation initialized")
endfunction()

#
# Log audit event
# Arguments:
#   EVENT_TYPE: Type of event
#   EVENT_MESSAGE: Event message
#
function(nexus_isolation_audit_log EVENT_TYPE EVENT_MESSAGE)
    if(NOT NEXUS_ISOLATION_AUDIT_ENABLED)
        return()
    endif()

    string(TIMESTAMP TIMESTAMP "%Y-%m-%d %H:%M:%S")

    file(APPEND "${NEXUS_ISOLATION_AUDIT_FILE}"
         "[${TIMESTAMP}] [${EVENT_TYPE}] ${EVENT_MESSAGE}\n")
endfunction()

#
# Initialize build isolation
#
function(nexus_isolation_init)
    if(NOT NEXUS_ISOLATION_ENABLED)
        message(STATUS "[Nexus Isolation] Build isolation disabled")
        return()
    endif()

    message(STATUS "==============================================================")
    message(STATUS "  Nexus Build Isolation and Security")
    message(STATUS "==============================================================")

    if(NEXUS_ISOLATION_AUDIT_ENABLED)
        file(WRITE "${NEXUS_ISOLATION_AUDIT_FILE}"
             "# Nexus Build System - Audit Log\n")
        file(APPEND "${NEXUS_ISOLATION_AUDIT_FILE}"
             "# Build: ${CMAKE_PROJECT_NAME}\n")
        string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")
        file(APPEND "${NEXUS_ISOLATION_AUDIT_FILE}"
             "# Timestamp: ${BUILD_TIMESTAMP}\n\n")

        nexus_isolation_audit_log("INIT" "Build isolation system initialized")
    endif()

    nexus_isolation_init_environment()
    nexus_isolation_init_filesystem()

    message(STATUS "==============================================================")
endfunction()

#
# Finalize build isolation
#
function(nexus_isolation_finalize)
    if(NOT NEXUS_ISOLATION_ENABLED)
        return()
    endif()

    message(STATUS "[Nexus Isolation] Finalizing build isolation...")

    if(NEXUS_ISOLATION_AUDIT_ENABLED)
        nexus_isolation_audit_log("FINALIZE" "Build isolation system finalized")
    endif()

    message(STATUS "[Nexus Isolation] Build isolation finalized")
endfunction()

message(STATUS "NexusExtension module loaded")

#-----------------------------------------------------------------------------
# End of NexusExtension.cmake
#-----------------------------------------------------------------------------
