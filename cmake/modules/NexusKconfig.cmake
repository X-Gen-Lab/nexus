##############################################################################
# NexusKconfig.cmake - Kconfig Integration Module
##############################################################################
#
# NexusKconfig.cmake
# Kconfig integration module for Nexus build system
# Author: Nexus Team
#
# This module provides Kconfig integration functionality, including
# configuration loading, validation, and menu generation.
#
##############################################################################

include_guard(GLOBAL)

##############################################################################
# Kconfig Integration Functions
##############################################################################

# Load Kconfig configuration and generate header
# Arguments:
#   KCONFIG_FILE: Path to root Kconfig file
#   CONFIG_FILE: Path to .config file
#   OUTPUT_HEADER: Path to generated header file
function(nexus_load_kconfig)
    cmake_parse_arguments(
        ARG
        ""
        "KCONFIG_FILE;CONFIG_FILE;OUTPUT_HEADER"
        ""
        ${ARGN}
    )

    # Validate required arguments
    if(NOT ARG_KCONFIG_FILE)
        message(FATAL_ERROR "nexus_load_kconfig: KCONFIG_FILE argument is required")
    endif()

    if(NOT ARG_CONFIG_FILE)
        message(FATAL_ERROR "nexus_load_kconfig: CONFIG_FILE argument is required")
    endif()

    if(NOT ARG_OUTPUT_HEADER)
        message(FATAL_ERROR "nexus_load_kconfig: OUTPUT_HEADER argument is required")
    endif()

    # Find Python interpreter
    if(NOT Python3_EXECUTABLE)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
    endif()

    # Locate generator script
    set(GENERATOR_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/kconfig/generate_config.py")

    if(NOT EXISTS ${GENERATOR_SCRIPT})
        message(FATAL_ERROR "Configuration generator script not found: ${GENERATOR_SCRIPT}")
    endif()

    # Check if Kconfig file exists
    if(NOT EXISTS ${ARG_KCONFIG_FILE})
        message(WARNING "Kconfig file not found: ${ARG_KCONFIG_FILE}")
    endif()

    # Collect all Kconfig files for dependency tracking
    file(GLOB_RECURSE KCONFIG_FILES
        "${CMAKE_SOURCE_DIR}/Kconfig"
        "${CMAKE_SOURCE_DIR}/*/Kconfig"
        "${CMAKE_SOURCE_DIR}/*/*/Kconfig"
        "${CMAKE_SOURCE_DIR}/*/*/*/Kconfig"
    )

    # Add .config file as a dependency if it exists
    if(EXISTS ${ARG_CONFIG_FILE})
        list(APPEND KCONFIG_FILES ${ARG_CONFIG_FILE})
    endif()

    # Configure file dependencies - CMake will reconfigure if these change
    foreach(KCONFIG_FILE ${KCONFIG_FILES})
        if(EXISTS ${KCONFIG_FILE})
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${KCONFIG_FILE})
        endif()
    endforeach()

    # Generate configuration header
    message(STATUS "Generating configuration header from Kconfig...")

    if(EXISTS ${ARG_CONFIG_FILE})
        # Use existing .config file
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
            message(WARNING "Failed to generate config from .config file:")
            if(ERROR)
                message(WARNING "  ${ERROR}")
            endif()
            message(STATUS "Falling back to default configuration...")

            # Fall back to default configuration
            execute_process(
                COMMAND ${Python3_EXECUTABLE} ${GENERATOR_SCRIPT}
                        --kconfig ${ARG_KCONFIG_FILE}
                        --default
                        --output ${ARG_OUTPUT_HEADER}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE RESULT
                ERROR_QUIET
            )

            if(NOT RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to generate default config")
            endif()
        endif()
    else()
        # Generate default configuration
        message(STATUS "No .config file found, generating default configuration...")
        execute_process(
            COMMAND ${Python3_EXECUTABLE} ${GENERATOR_SCRIPT}
                    --kconfig ${ARG_KCONFIG_FILE}
                    --default
                    --output ${ARG_OUTPUT_HEADER}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE RESULT
            OUTPUT_VARIABLE OUTPUT
            ERROR_QUIET
        )

        if(NOT RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to generate default config")
        endif()
    endif()

    # Mark the generated header as a generated file
    set_source_files_properties(${ARG_OUTPUT_HEADER} PROPERTIES GENERATED TRUE)

    # Count Kconfig files for status message
    list(LENGTH KCONFIG_FILES KCONFIG_COUNT)

    message(STATUS "Generated configuration header: ${ARG_OUTPUT_HEADER}")
    message(STATUS "Tracking ${KCONFIG_COUNT} Kconfig/config files for changes")
endfunction()

# Add Kconfig dependency to target
# Arguments:
#   TARGET: Target name
#   CONFIG_OPTIONS: List of config options this target depends on
function(nexus_add_kconfig_dependency)
    cmake_parse_arguments(
        ARG
        ""
        "TARGET"
        "CONFIG_OPTIONS"
        ${ARGN}
    )

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "nexus_add_kconfig_dependency: TARGET argument is required")
    endif()

    if(NOT TARGET ${ARG_TARGET})
        message(FATAL_ERROR "nexus_add_kconfig_dependency: Target '${ARG_TARGET}' does not exist")
    endif()

    # Add nexus_config.h as a dependency
    if(EXISTS ${NEXUS_CONFIG_HEADER})
        get_target_property(TARGET_SOURCES ${ARG_TARGET} SOURCES)

        foreach(SOURCE ${TARGET_SOURCES})
            set_property(SOURCE ${SOURCE} APPEND PROPERTY
                OBJECT_DEPENDS ${NEXUS_CONFIG_HEADER}
            )
        endforeach()
    endif()

    message(VERBOSE "Added Kconfig dependency to target: ${ARG_TARGET}")
endfunction()

# Validate Kconfig configuration
# Arguments:
#   CONFIG_FILE: Path to .config file
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

    # Check for required platform configuration
    if(NOT DEFINED CONFIG_PLATFORM_NAME AND NOT DEFINED NEXUS_PLATFORM)
        message(WARNING
            "Platform not configured in Kconfig.\n"
            "Please run 'make menuconfig' or set NEXUS_PLATFORM variable."
        )
    endif()

    # Check toolchain consistency for embedded platforms
    if(DEFINED CONFIG_TOOLCHAIN_ARM_GCC OR DEFINED CONFIG_TOOLCHAIN_ARM_CLANG OR DEFINED CONFIG_TOOLCHAIN_IAR)
        if(DEFINED CONFIG_PLATFORM_NATIVE AND CONFIG_PLATFORM_NATIVE)
            message(FATAL_ERROR
                "Configuration conflict: ARM toolchain selected but native platform configured.\n"
                "Please select a compatible toolchain for native platform (GCC/Clang/MSVC)."
            )
        endif()
    endif()

    # Check FPU configuration consistency
    if(DEFINED CONFIG_FPU_TYPE AND CONFIG_FPU_TYPE)
        if(DEFINED CONFIG_CPU_CORTEX_M0 OR DEFINED CONFIG_CPU_CORTEX_M0PLUS OR DEFINED CONFIG_CPU_CORTEX_M3)
            message(FATAL_ERROR
                "Configuration conflict: FPU configured but CPU does not support FPU.\n"
                "Cortex-M0/M0+/M3 do not have FPU. Please select FPU_NONE."
            )
        endif()
    endif()

    # Check linker script for embedded platforms
    if(NOT DEFINED CONFIG_PLATFORM_NATIVE OR NOT CONFIG_PLATFORM_NATIVE)
        if(NOT DEFINED CONFIG_LINKER_SCRIPT OR NOT CONFIG_LINKER_SCRIPT)
            message(WARNING
                "No linker script configured for embedded platform.\n"
                "Please configure CONFIG_LINKER_SCRIPT in Kconfig or provide via platform defaults."
            )
        endif()
    endif()

    # Check build tests on embedded platforms
    if(DEFINED CONFIG_BUILD_TESTS AND CONFIG_BUILD_TESTS)
        if(NOT DEFINED CONFIG_PLATFORM_NATIVE OR NOT CONFIG_PLATFORM_NATIVE)
            message(WARNING
                "Tests are enabled but platform is not native.\n"
                "Tests are only supported on native platform. Disabling tests."
            )
            set(NEXUS_BUILD_TESTS OFF CACHE BOOL "Tests disabled for embedded platform" FORCE)
        endif()
    endif()

    # Check sanitizers on embedded platforms
    if(DEFINED CONFIG_ENABLE_SANITIZERS AND CONFIG_ENABLE_SANITIZERS)
        if(NOT DEFINED CONFIG_PLATFORM_NATIVE OR NOT CONFIG_PLATFORM_NATIVE)
            message(WARNING
                "Sanitizers are enabled but platform is not native.\n"
                "Sanitizers are only supported on native platform."
            )
        endif()
    endif()

    message(STATUS "Kconfig configuration validation passed")
endfunction()

# Update conditional dependencies based on Kconfig changes
function(nexus_update_kconfig_dependencies)
    message(STATUS "Updating conditional dependencies based on Kconfig...")

    # Re-evaluate all conditional dependencies
    # This function is called when Kconfig changes are detected

    # Example: Enable/disable modules based on Kconfig
    if(DEFINED CONFIG_ENABLE_MODULE_X AND CONFIG_ENABLE_MODULE_X)
        message(STATUS "  Module X enabled by Kconfig")
        set(NEXUS_MODULE_X_ENABLED TRUE PARENT_SCOPE)
    else()
        message(STATUS "  Module X disabled by Kconfig")
        set(NEXUS_MODULE_X_ENABLED FALSE PARENT_SCOPE)
    endif()

    message(STATUS "Conditional dependencies updated")
endfunction()

# Generate Kconfig menu configuration
# Arguments:
#   INTERFACE: Interface type (menuconfig, guiconfig, nconfig)
function(nexus_kconfig_menu)
    cmake_parse_arguments(
        ARG
        ""
        "INTERFACE"
        ""
        ${ARGN}
    )

    if(NOT ARG_INTERFACE)
        set(ARG_INTERFACE "menuconfig")
    endif()

    # Find Python interpreter
    if(NOT Python3_EXECUTABLE)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
    endif()

    # Locate Kconfig menu script
    set(MENU_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/kconfig/${ARG_INTERFACE}.py")

    if(NOT EXISTS ${MENU_SCRIPT})
        message(FATAL_ERROR "Kconfig menu script not found: ${MENU_SCRIPT}")
    endif()

    # Launch Kconfig menu
    message(STATUS "Launching Kconfig ${ARG_INTERFACE}...")

    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${MENU_SCRIPT}
                --kconfig ${NEXUS_KCONFIG_FILE}
                --config ${NEXUS_CONFIG_FILE}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE RESULT
    )

    if(NOT RESULT EQUAL 0)
        message(FATAL_ERROR "Kconfig menu failed")
    endif()

    message(STATUS "Kconfig configuration updated. Please reconfigure CMake.")
endfunction()

##############################################################################
# End of NexusKconfig.cmake
##############################################################################
