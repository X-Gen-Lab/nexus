##############################################################################
# Nexus Plugin System
#
# This module provides a plugin system for extending the Nexus build system
# with custom build steps and tools.
##############################################################################

# Global plugin registry
if(NOT DEFINED NEXUS_PLUGIN_REGISTRY)
    set(NEXUS_PLUGIN_REGISTRY "" CACHE INTERNAL "List of registered plugins")
endif()

if(NOT DEFINED NEXUS_PLUGIN_HOOKS)
    set(NEXUS_PLUGIN_HOOKS "" CACHE INTERNAL "Plugin hook registry")
endif()

if(NOT DEFINED NEXUS_PLUGIN_CONFIGS)
    set(NEXUS_PLUGIN_CONFIGS "" CACHE INTERNAL "Plugin configuration registry")
endif()

##############################################################################
# Plugin Lifecycle Hooks
##############################################################################

# Available hook points in the build system
set(NEXUS_HOOK_PRE_CONFIGURE "pre_configure" CACHE INTERNAL "Before CMake configuration")
set(NEXUS_HOOK_POST_CONFIGURE "post_configure" CACHE INTERNAL "After CMake configuration")
set(NEXUS_HOOK_PRE_BUILD "pre_build" CACHE INTERNAL "Before build starts")
set(NEXUS_HOOK_POST_BUILD "post_build" CACHE INTERNAL "After build completes")
set(NEXUS_HOOK_PRE_TEST "pre_test" CACHE INTERNAL "Before tests run")
set(NEXUS_HOOK_POST_TEST "post_test" CACHE INTERNAL "After tests complete")
set(NEXUS_HOOK_PRE_INSTALL "pre_install" CACHE INTERNAL "Before installation")
set(NEXUS_HOOK_POST_INSTALL "post_install" CACHE INTERNAL "After installation")
set(NEXUS_HOOK_PRE_CLEAN "pre_clean" CACHE INTERNAL "Before clean")
set(NEXUS_HOOK_POST_CLEAN "post_clean" CACHE INTERNAL "After clean")

##############################################################################
# Plugin API Functions
##############################################################################

#
# Register a plugin with the build system
# NAME: Plugin name (unique identifier)
# VERSION: Plugin version (SemVer format)
# DESCRIPTION: Brief description of the plugin
# AUTHOR: Plugin author
# SCRIPT: Path to the plugin CMake script
# HOOKS: List of hooks this plugin implements
# DEPENDENCIES: List of required plugins
#
function(nexus_register_plugin)
    cmake_parse_arguments(
        PLUGIN
        ""
        "NAME;VERSION;DESCRIPTION;AUTHOR;SCRIPT"
        "HOOKS;DEPENDENCIES"
        ${ARGN}
    )

    # Validate required parameters
    if(NOT PLUGIN_NAME)
        message(FATAL_ERROR "Plugin NAME is required")
    endif()

    if(NOT PLUGIN_VERSION)
        message(FATAL_ERROR "Plugin VERSION is required for ${PLUGIN_NAME}")
    endif()

    if(NOT PLUGIN_SCRIPT)
        message(FATAL_ERROR "Plugin SCRIPT is required for ${PLUGIN_NAME}")
    endif()

    # Check if plugin already registered
    if("${PLUGIN_NAME}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(WARNING "Plugin ${PLUGIN_NAME} is already registered, skipping")
        return()
    endif()

    # Validate plugin script exists
    if(NOT EXISTS "${PLUGIN_SCRIPT}")
        message(FATAL_ERROR "Plugin script not found: ${PLUGIN_SCRIPT}")
    endif()

    # Add to registry
    list(APPEND NEXUS_PLUGIN_REGISTRY "${PLUGIN_NAME}")
    set(NEXUS_PLUGIN_REGISTRY "${NEXUS_PLUGIN_REGISTRY}" CACHE INTERNAL "List of registered plugins")

    # Store plugin metadata
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
# Unregister a plugin from the build system
# NAME: Plugin name to unregister
#
function(nexus_unregister_plugin NAME)
    if(NOT "${NAME}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(WARNING "Plugin ${NAME} is not registered")
        return()
    endif()

    # Remove from registry
    list(REMOVE_ITEM NEXUS_PLUGIN_REGISTRY "${NAME}")
    set(NEXUS_PLUGIN_REGISTRY "${NEXUS_PLUGIN_REGISTRY}" CACHE INTERNAL "List of registered plugins")

    # Clear plugin metadata
    unset(NEXUS_PLUGIN_${NAME}_VERSION CACHE)
    unset(NEXUS_PLUGIN_${NAME}_DESCRIPTION CACHE)
    unset(NEXUS_PLUGIN_${NAME}_AUTHOR CACHE)
    unset(NEXUS_PLUGIN_${NAME}_SCRIPT CACHE)
    unset(NEXUS_PLUGIN_${NAME}_HOOKS CACHE)
    unset(NEXUS_PLUGIN_${NAME}_DEPENDENCIES CACHE)
    unset(NEXUS_PLUGIN_${NAME}_LOADED CACHE)
    unset(NEXUS_PLUGIN_${NAME}_CONFIG CACHE)

    message(STATUS "Unregistered plugin: ${NAME}")
endfunction()

#
# Register a hook implementation for a plugin
# PLUGIN: Plugin name
# HOOK: Hook point name
# FUNCTION: CMake function to call
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

    # Validate plugin is registered
    if(NOT "${HOOK_PLUGIN}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(FATAL_ERROR "Plugin ${HOOK_PLUGIN} is not registered")
    endif()

    # Create hook entry
    set(HOOK_ENTRY "${HOOK_PLUGIN}:${HOOK_HOOK}:${HOOK_FUNCTION}")

    # Add to hook registry
    list(APPEND NEXUS_PLUGIN_HOOKS "${HOOK_ENTRY}")
    set(NEXUS_PLUGIN_HOOKS "${NEXUS_PLUGIN_HOOKS}" CACHE INTERNAL "Plugin hook registry")

    message(VERBOSE "Registered hook: ${HOOK_PLUGIN} -> ${HOOK_HOOK} -> ${HOOK_FUNCTION}")
endfunction()

#
# Set configuration for a plugin
# PLUGIN: Plugin name
# KEY: Configuration key
# VALUE: Configuration value
#
function(nexus_set_plugin_config)
    cmake_parse_arguments(
        CFG
        ""
        "PLUGIN;KEY;VALUE"
        ""
        ${ARGN}
    )

    if(NOT CFG_PLUGIN)
        message(FATAL_ERROR "PLUGIN is required for configuration")
    endif()

    if(NOT CFG_KEY)
        message(FATAL_ERROR "KEY is required for configuration")
    endif()

    # Validate plugin is registered
    if(NOT "${CFG_PLUGIN}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(FATAL_ERROR "Plugin ${CFG_PLUGIN} is not registered")
    endif()

    # Store configuration
    set(NEXUS_PLUGIN_${CFG_PLUGIN}_CONFIG_${CFG_KEY} "${CFG_VALUE}" CACHE INTERNAL "")

    message(VERBOSE "Set plugin config: ${CFG_PLUGIN}.${CFG_KEY} = ${CFG_VALUE}")
endfunction()

#
# Get configuration for a plugin
# PLUGIN: Plugin name
# KEY: Configuration key
# OUTPUT_VAR: Variable to store the value
#
function(nexus_get_plugin_config PLUGIN KEY OUTPUT_VAR)
    # Validate plugin is registered
    if(NOT "${PLUGIN}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(FATAL_ERROR "Plugin ${PLUGIN} is not registered")
    endif()

    # Retrieve configuration
    if(DEFINED NEXUS_PLUGIN_${PLUGIN}_CONFIG_${KEY})
        set(${OUTPUT_VAR} "${NEXUS_PLUGIN_${PLUGIN}_CONFIG_${KEY}}" PARENT_SCOPE)
    else()
        set(${OUTPUT_VAR} "" PARENT_SCOPE)
    endif()
endfunction()

#
# List all registered plugins
# OUTPUT_VAR: Variable to store the plugin list
#
function(nexus_list_plugins OUTPUT_VAR)
    set(${OUTPUT_VAR} "${NEXUS_PLUGIN_REGISTRY}" PARENT_SCOPE)
endfunction()

#
# Get plugin information
# PLUGIN: Plugin name
# PROPERTY: Property to retrieve (VERSION, DESCRIPTION, AUTHOR, etc.)
# OUTPUT_VAR: Variable to store the value
#
function(nexus_get_plugin_info PLUGIN PROPERTY OUTPUT_VAR)
    # Validate plugin is registered
    if(NOT "${PLUGIN}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(FATAL_ERROR "Plugin ${PLUGIN} is not registered")
    endif()

    # Retrieve property
    if(PROPERTY STREQUAL "VERSION")
        set(${OUTPUT_VAR} "${NEXUS_PLUGIN_${PLUGIN}_VERSION}" PARENT_SCOPE)
    elseif(PROPERTY STREQUAL "DESCRIPTION")
        set(${OUTPUT_VAR} "${NEXUS_PLUGIN_${PLUGIN}_DESCRIPTION}" PARENT_SCOPE)
    elseif(PROPERTY STREQUAL "AUTHOR")
        set(${OUTPUT_VAR} "${NEXUS_PLUGIN_${PLUGIN}_AUTHOR}" PARENT_SCOPE)
    elseif(PROPERTY STREQUAL "SCRIPT")
        set(${OUTPUT_VAR} "${NEXUS_PLUGIN_${PLUGIN}_SCRIPT}" PARENT_SCOPE)
    elseif(PROPERTY STREQUAL "HOOKS")
        set(${OUTPUT_VAR} "${NEXUS_PLUGIN_${PLUGIN}_HOOKS}" PARENT_SCOPE)
    elseif(PROPERTY STREQUAL "DEPENDENCIES")
        set(${OUTPUT_VAR} "${NEXUS_PLUGIN_${PLUGIN}_DEPENDENCIES}" PARENT_SCOPE)
    elseif(PROPERTY STREQUAL "LOADED")
        set(${OUTPUT_VAR} "${NEXUS_PLUGIN_${PLUGIN}_LOADED}" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unknown plugin property: ${PROPERTY}")
    endif()
endfunction()

##############################################################################
# Plugin Context Structure
##############################################################################

#
# Create a context object for plugin hooks
# OUTPUT_VAR: Variable to store the context
#
function(nexus_create_plugin_context OUTPUT_VAR)
    # Context contains build system state
    set(CONTEXT "")
    list(APPEND CONTEXT "CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}")
    list(APPEND CONTEXT "CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}")
    list(APPEND CONTEXT "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
    list(APPEND CONTEXT "CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")
    list(APPEND CONTEXT "CMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
    list(APPEND CONTEXT "CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")

    # Add Nexus-specific context
    if(DEFINED NEXUS_PLATFORM)
        list(APPEND CONTEXT "NEXUS_PLATFORM=${NEXUS_PLATFORM}")
    endif()

    if(DEFINED NEXUS_TOOLCHAIN)
        list(APPEND CONTEXT "NEXUS_TOOLCHAIN=${NEXUS_TOOLCHAIN}")
    endif()

    set(${OUTPUT_VAR} "${CONTEXT}" PARENT_SCOPE)
endfunction()

message(STATUS "Nexus Plugin System loaded")

##############################################################################
# Plugin Loading and Unloading
##############################################################################

#
# Load a plugin and execute its initialization
# NAME: Plugin name to load
#
function(nexus_load_plugin NAME)
    # Validate plugin is registered
    if(NOT "${NAME}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(FATAL_ERROR "Plugin ${NAME} is not registered")
    endif()

    # Check if already loaded
    if(NEXUS_PLUGIN_${NAME}_LOADED)
        message(VERBOSE "Plugin ${NAME} is already loaded")
        return()
    endif()

    # Load dependencies first
    nexus_get_plugin_info(${NAME} DEPENDENCIES DEPS)
    foreach(DEP ${DEPS})
        if(NOT "${DEP}" IN_LIST NEXUS_PLUGIN_REGISTRY)
            message(FATAL_ERROR "Plugin ${NAME} depends on ${DEP}, but it is not registered")
        endif()

        nexus_load_plugin(${DEP})
    endforeach()

    # Get plugin script
    nexus_get_plugin_info(${NAME} SCRIPT SCRIPT_PATH)
    nexus_get_plugin_info(${NAME} VERSION VERSION)

    message(STATUS "Loading plugin: ${NAME} v${VERSION}")

    # Verify plugin signature (basic check - file exists and is readable)
    if(NOT EXISTS "${SCRIPT_PATH}")
        message(FATAL_ERROR "Plugin script not found: ${SCRIPT_PATH}")
    endif()

    # Read plugin script to verify it's not empty
    file(READ "${SCRIPT_PATH}" SCRIPT_CONTENT)
    string(LENGTH "${SCRIPT_CONTENT}" SCRIPT_LENGTH)
    if(SCRIPT_LENGTH EQUAL 0)
        message(FATAL_ERROR "Plugin script is empty: ${SCRIPT_PATH}")
    endif()

    # Verify plugin signature (SHA-256 hash check if signature file exists)
    get_filename_component(SCRIPT_DIR "${SCRIPT_PATH}" DIRECTORY)
    get_filename_component(SCRIPT_NAME "${SCRIPT_PATH}" NAME)
    set(SIGNATURE_FILE "${SCRIPT_DIR}/${SCRIPT_NAME}.sha256")

    if(EXISTS "${SIGNATURE_FILE}")
        message(VERBOSE "Verifying plugin signature: ${SIGNATURE_FILE}")

        # Calculate actual hash
        file(SHA256 "${SCRIPT_PATH}" ACTUAL_HASH)

        # Read expected hash
        file(READ "${SIGNATURE_FILE}" EXPECTED_HASH)
        string(STRIP "${EXPECTED_HASH}" EXPECTED_HASH)

        # Compare hashes
        if(NOT "${ACTUAL_HASH}" STREQUAL "${EXPECTED_HASH}")
            message(FATAL_ERROR
                "Plugin signature verification failed for ${NAME}\n"
                "Expected: ${EXPECTED_HASH}\n"
                "Actual:   ${ACTUAL_HASH}\n"
                "The plugin may have been tampered with."
            )
        endif()

        message(STATUS "Plugin signature verified: ${NAME}")
    else()
        message(WARNING
            "No signature file found for plugin ${NAME}\n"
            "Plugin security cannot be verified.\n"
            "Consider creating ${SIGNATURE_FILE}"
        )
    endif()

    # Include the plugin script
    include("${SCRIPT_PATH}")

    # Call plugin initialization function if it exists
    if(COMMAND ${NAME}_plugin_init)
        message(VERBOSE "Calling ${NAME}_plugin_init()")
        cmake_language(CALL ${NAME}_plugin_init)
    endif()

    # Mark as loaded
    set(NEXUS_PLUGIN_${NAME}_LOADED TRUE CACHE INTERNAL "")

    message(STATUS "Plugin loaded successfully: ${NAME}")
endfunction()

#
# Unload a plugin
# NAME: Plugin name to unload
#
function(nexus_unload_plugin NAME)
    # Validate plugin is registered
    if(NOT "${NAME}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(WARNING "Plugin ${NAME} is not registered")
        return()
    endif()

    # Check if loaded
    if(NOT NEXUS_PLUGIN_${NAME}_LOADED)
        message(VERBOSE "Plugin ${NAME} is not loaded")
        return()
    endif()

    message(STATUS "Unloading plugin: ${NAME}")

    # Call plugin cleanup function if it exists
    if(COMMAND ${NAME}_plugin_cleanup)
        message(VERBOSE "Calling ${NAME}_plugin_cleanup()")
        cmake_language(CALL ${NAME}_plugin_cleanup)
    endif()

    # Mark as unloaded
    set(NEXUS_PLUGIN_${NAME}_LOADED FALSE CACHE INTERNAL "")

    message(STATUS "Plugin unloaded: ${NAME}")
endfunction()

#
# Load all registered plugins
#
function(nexus_load_all_plugins)
    message(STATUS "Loading all registered plugins...")

    foreach(PLUGIN ${NEXUS_PLUGIN_REGISTRY})
        nexus_load_plugin(${PLUGIN})
    endforeach()

    message(STATUS "All plugins loaded")
endfunction()

#
# Unload all loaded plugins
#
function(nexus_unload_all_plugins)
    message(STATUS "Unloading all plugins...")

    # Unload in reverse order
    list(REVERSE NEXUS_PLUGIN_REGISTRY)
    foreach(PLUGIN ${NEXUS_PLUGIN_REGISTRY})
        if(NEXUS_PLUGIN_${PLUGIN}_LOADED)
            nexus_unload_plugin(${PLUGIN})
        endif()
    endforeach()

    message(STATUS "All plugins unloaded")
endfunction()

#
# Generate plugin signature file
# SCRIPT: Path to plugin script
#
function(nexus_generate_plugin_signature SCRIPT)
    if(NOT EXISTS "${SCRIPT}")
        message(FATAL_ERROR "Plugin script not found: ${SCRIPT}")
    endif()

    # Calculate SHA-256 hash
    file(SHA256 "${SCRIPT}" HASH)

    # Write signature file
    get_filename_component(SCRIPT_DIR "${SCRIPT}" DIRECTORY)
    get_filename_component(SCRIPT_NAME "${SCRIPT}" NAME)
    set(SIGNATURE_FILE "${SCRIPT_DIR}/${SCRIPT_NAME}.sha256")

    file(WRITE "${SIGNATURE_FILE}" "${HASH}")

    message(STATUS "Generated signature for ${SCRIPT_NAME}: ${HASH}")
    message(STATUS "Signature file: ${SIGNATURE_FILE}")
endfunction()

##############################################################################
# Plugin Hook Invocation
##############################################################################

#
# Invoke all hooks registered for a specific hook point
# HOOK: Hook point name
# CONTEXT: Optional context data to pass to hooks
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

    # Create default context if not provided
    if(NOT INVOKE_CONTEXT)
        nexus_create_plugin_context(INVOKE_CONTEXT)
    endif()

    # Count hooks invoked
    set(HOOKS_INVOKED 0)

    # Iterate through all registered hooks
    foreach(HOOK_ENTRY ${NEXUS_PLUGIN_HOOKS})
        # Parse hook entry: PLUGIN:HOOK:FUNCTION
        string(REPLACE ":" ";" HOOK_PARTS "${HOOK_ENTRY}")
        list(GET HOOK_PARTS 0 PLUGIN_NAME)
        list(GET HOOK_PARTS 1 HOOK_NAME)
        list(GET HOOK_PARTS 2 HOOK_FUNCTION)

        # Check if this hook matches
        if("${HOOK_NAME}" STREQUAL "${HOOK}")
            # Check if plugin is loaded
            if(NOT NEXUS_PLUGIN_${PLUGIN_NAME}_LOADED)
                message(WARNING "Plugin ${PLUGIN_NAME} is not loaded, skipping hook ${HOOK_NAME}")
                continue()
            endif()

            message(VERBOSE "Calling hook: ${PLUGIN_NAME}.${HOOK_FUNCTION}")

            # Invoke the hook function with context
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

#
# Invoke pre-configure hooks
#
function(nexus_invoke_pre_configure_hooks)
    nexus_create_plugin_context(CONTEXT)
    nexus_invoke_hooks(${NEXUS_HOOK_PRE_CONFIGURE} CONTEXT ${CONTEXT})
endfunction()

#
# Invoke post-configure hooks
#
function(nexus_invoke_post_configure_hooks)
    nexus_create_plugin_context(CONTEXT)
    nexus_invoke_hooks(${NEXUS_HOOK_POST_CONFIGURE} CONTEXT ${CONTEXT})
endfunction()

#
# Invoke pre-build hooks
#
function(nexus_invoke_pre_build_hooks)
    nexus_create_plugin_context(CONTEXT)
    nexus_invoke_hooks(${NEXUS_HOOK_PRE_BUILD} CONTEXT ${CONTEXT})
endfunction()

#
# Invoke post-build hooks
#
function(nexus_invoke_post_build_hooks)
    nexus_create_plugin_context(CONTEXT)
    nexus_invoke_hooks(${NEXUS_HOOK_POST_BUILD} CONTEXT ${CONTEXT})
endfunction()

#
# Invoke pre-test hooks
#
function(nexus_invoke_pre_test_hooks)
    nexus_create_plugin_context(CONTEXT)
    nexus_invoke_hooks(${NEXUS_HOOK_PRE_TEST} CONTEXT ${CONTEXT})
endfunction()

#
# Invoke post-test hooks
#
function(nexus_invoke_post_test_hooks)
    nexus_create_plugin_context(CONTEXT)
    nexus_invoke_hooks(${NEXUS_HOOK_POST_TEST} CONTEXT ${CONTEXT})
endfunction()

#
# Invoke pre-install hooks
#
function(nexus_invoke_pre_install_hooks)
    nexus_create_plugin_context(CONTEXT)
    nexus_invoke_hooks(${NEXUS_HOOK_PRE_INSTALL} CONTEXT ${CONTEXT})
endfunction()

#
# Invoke post-install hooks
#
function(nexus_invoke_post_install_hooks)
    nexus_create_plugin_context(CONTEXT)
    nexus_invoke_hooks(${NEXUS_HOOK_POST_INSTALL} CONTEXT ${CONTEXT})
endfunction()

#
# Invoke pre-clean hooks
#
function(nexus_invoke_pre_clean_hooks)
    nexus_create_plugin_context(CONTEXT)
    nexus_invoke_hooks(${NEXUS_HOOK_PRE_CLEAN} CONTEXT ${CONTEXT})
endfunction()

#
# Invoke post-clean hooks
#
function(nexus_invoke_post_clean_hooks)
    nexus_create_plugin_context(CONTEXT)
    nexus_invoke_hooks(${NEXUS_HOOK_POST_CLEAN} CONTEXT ${CONTEXT})
endfunction()

##############################################################################
# Plugin Configuration Management
##############################################################################

#
# Load plugin configuration from a JSON file
# PLUGIN: Plugin name
# CONFIG_FILE: Path to JSON configuration file
#
function(nexus_load_plugin_config PLUGIN CONFIG_FILE)
    # Validate plugin is registered
    if(NOT "${PLUGIN}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(FATAL_ERROR "Plugin ${PLUGIN} is not registered")
    endif()

    # Check if config file exists
    if(NOT EXISTS "${CONFIG_FILE}")
        message(WARNING "Plugin config file not found: ${CONFIG_FILE}")
        return()
    endif()

    message(STATUS "Loading configuration for plugin ${PLUGIN} from ${CONFIG_FILE}")

    # Read config file
    file(READ "${CONFIG_FILE}" CONFIG_CONTENT)

    # Parse configuration file (supports both JSON and simple key=value format)
    # Use regex-based parsing for maximum compatibility
    string(REPLACE "\n" ";" CONFIG_LINES "${CONFIG_CONTENT}")
    foreach(LINE ${CONFIG_LINES})
        # Skip comments and empty lines
        string(STRIP "${LINE}" LINE)
        if(LINE MATCHES "^#" OR LINE MATCHES "^//" OR LINE STREQUAL "" OR LINE MATCHES "^[{}]")
            continue()
        endif()

        # Parse JSON format: "key": "value"
        if(LINE MATCHES "\"([^\"]+)\"[[:space:]]*:[[:space:]]*\"([^\"]*)\"")
            set(KEY "${CMAKE_MATCH_1}")
            set(VALUE "${CMAKE_MATCH_2}")

            nexus_set_plugin_config(
                PLUGIN ${PLUGIN}
                KEY ${KEY}
                VALUE "${VALUE}"
            )
        # Parse simple format: key=value
        elseif(LINE MATCHES "^([^=]+)=(.*)$")
            set(KEY "${CMAKE_MATCH_1}")
            set(VALUE "${CMAKE_MATCH_2}")
            string(STRIP "${KEY}" KEY)
            string(STRIP "${VALUE}" VALUE)

            nexus_set_plugin_config(
                PLUGIN ${PLUGIN}
                KEY ${KEY}
                VALUE "${VALUE}"
            )
        endif()
    endforeach()

    message(STATUS "Configuration loaded for plugin ${PLUGIN}")
endfunction()

#
# Save plugin configuration to a JSON file
# PLUGIN: Plugin name
# CONFIG_FILE: Path to output JSON configuration file
#
function(nexus_save_plugin_config PLUGIN CONFIG_FILE)
    # Validate plugin is registered
    if(NOT "${PLUGIN}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(FATAL_ERROR "Plugin ${PLUGIN} is not registered")
    endif()

    message(STATUS "Saving configuration for plugin ${PLUGIN} to ${CONFIG_FILE}")

    # Collect all config keys for this plugin
    get_cmake_property(ALL_VARS VARIABLES)
    set(CONFIG_KEYS "")

    foreach(VAR ${ALL_VARS})
        if(VAR MATCHES "^NEXUS_PLUGIN_${PLUGIN}_CONFIG_(.+)$")
            list(APPEND CONFIG_KEYS "${CMAKE_MATCH_1}")
        endif()
    endforeach()

    # Build JSON content
    set(JSON_CONTENT "{\n")
    set(FIRST TRUE)

    foreach(KEY ${CONFIG_KEYS})
        nexus_get_plugin_config(${PLUGIN} ${KEY} VALUE)

        if(NOT FIRST)
            string(APPEND JSON_CONTENT ",\n")
        endif()
        set(FIRST FALSE)

        string(APPEND JSON_CONTENT "  \"${KEY}\": \"${VALUE}\"")
    endforeach()

    string(APPEND JSON_CONTENT "\n}\n")

    # Write to file
    file(WRITE "${CONFIG_FILE}" "${JSON_CONTENT}")

    message(STATUS "Configuration saved for plugin ${PLUGIN}")
endfunction()

#
# Pass configuration to plugin initialization
# PLUGIN: Plugin name
#
function(nexus_pass_config_to_plugin PLUGIN)
    # Validate plugin is registered
    if(NOT "${PLUGIN}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(FATAL_ERROR "Plugin ${PLUGIN} is not registered")
    endif()

    # Collect all config keys for this plugin
    get_cmake_property(ALL_VARS VARIABLES)
    set(CONFIG_ARGS "")

    foreach(VAR ${ALL_VARS})
        if(VAR MATCHES "^NEXUS_PLUGIN_${PLUGIN}_CONFIG_(.+)$")
            set(KEY "${CMAKE_MATCH_1}")
            nexus_get_plugin_config(${PLUGIN} ${KEY} VALUE)
            list(APPEND CONFIG_ARGS "${KEY}=${VALUE}")
        endif()
    endforeach()

    # Call plugin config function if it exists
    if(COMMAND ${PLUGIN}_plugin_configure)
        message(VERBOSE "Calling ${PLUGIN}_plugin_configure() with config")
        cmake_language(CALL ${PLUGIN}_plugin_configure ${CONFIG_ARGS})
    endif()
endfunction()

#
# Set multiple configuration values at once
# PLUGIN: Plugin name
# ...: Key-value pairs (KEY1 VALUE1 KEY2 VALUE2 ...)
#
function(nexus_set_plugin_configs PLUGIN)
    # Validate plugin is registered
    if(NOT "${PLUGIN}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(FATAL_ERROR "Plugin ${PLUGIN} is not registered")
    endif()

    # Parse key-value pairs
    set(ARGS ${ARGN})
    list(LENGTH ARGS ARGS_LENGTH)

    if(ARGS_LENGTH EQUAL 0)
        return()
    endif()

    # Check if even number of arguments
    math(EXPR REMAINDER "${ARGS_LENGTH} % 2")
    if(NOT REMAINDER EQUAL 0)
        message(FATAL_ERROR "nexus_set_plugin_configs requires key-value pairs")
    endif()

    # Process pairs
    math(EXPR PAIRS "${ARGS_LENGTH} / 2")
    math(EXPR LAST_INDEX "${PAIRS} - 1")

    foreach(INDEX RANGE ${LAST_INDEX})
        math(EXPR KEY_INDEX "${INDEX} * 2")
        math(EXPR VALUE_INDEX "${KEY_INDEX} + 1")

        list(GET ARGS ${KEY_INDEX} KEY)
        list(GET ARGS ${VALUE_INDEX} VALUE)

        nexus_set_plugin_config(
            PLUGIN ${PLUGIN}
            KEY ${KEY}
            VALUE "${VALUE}"
        )
    endforeach()
endfunction()

#
# Get all configuration for a plugin
# PLUGIN: Plugin name
# OUTPUT_VAR: Variable to store the configuration map
#
function(nexus_get_all_plugin_config PLUGIN OUTPUT_VAR)
    # Validate plugin is registered
    if(NOT "${PLUGIN}" IN_LIST NEXUS_PLUGIN_REGISTRY)
        message(FATAL_ERROR "Plugin ${PLUGIN} is not registered")
    endif()

    # Collect all config keys for this plugin
    get_cmake_property(ALL_VARS VARIABLES)
    set(CONFIG_MAP "")

    foreach(VAR ${ALL_VARS})
        if(VAR MATCHES "^NEXUS_PLUGIN_${PLUGIN}_CONFIG_(.+)$")
            set(KEY "${CMAKE_MATCH_1}")
            nexus_get_plugin_config(${PLUGIN} ${KEY} VALUE)
            list(APPEND CONFIG_MAP "${KEY}=${VALUE}")
        endif()
    endforeach()

    set(${OUTPUT_VAR} "${CONFIG_MAP}" PARENT_SCOPE)
endfunction()
