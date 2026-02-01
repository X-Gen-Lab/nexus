##############################################################################
# NexusBuild.cmake - Build Functions Module
##############################################################################
#
# NexusBuild.cmake
# Core build functions for creating libraries, executables, and tests
# Author: Nexus Team
#
# This module provides:
# - Library creation with standard settings
# - Executable creation with linker script support
# - Test creation with Google Test integration
# - Binary file generation (.bin, .hex)
# - Memory usage reporting
# - Precompiled header support
#
# Consolidated from:
# - NexusBuild.cmake
# - NexusHelpers.cmake
#
##############################################################################

include_guard(GLOBAL)

##############################################################################
# Library Creation
##############################################################################

#
# Add a Nexus library with standard settings
# Arguments:
#   TARGET: Target name
#   SOURCES: Source files
#   INCLUDES: Public include directories
#   DEPS: Dependencies
#   PRIVATE_INCLUDES: Private include directories (optional)
#   COMPILE_OPTIONS: Additional compile options (optional)
#   LINK_OPTIONS: Additional link options (optional)
#
function(nexus_add_library TARGET)
    cmake_parse_arguments(
        ARG
        ""
        ""
        "SOURCES;INCLUDES;DEPS;PRIVATE_INCLUDES;COMPILE_OPTIONS;LINK_OPTIONS"
        ${ARGN}
    )

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "nexus_add_library: SOURCES argument is required")
    endif()

    add_library(${TARGET} STATIC ${ARG_SOURCES})

    if(ARG_INCLUDES)
        target_include_directories(${TARGET}
            PUBLIC ${ARG_INCLUDES}
        )
    endif()

    if(ARG_PRIVATE_INCLUDES)
        target_include_directories(${TARGET}
            PRIVATE ${ARG_PRIVATE_INCLUDES}
        )
    endif()

    if(ARG_DEPS)
        target_link_libraries(${TARGET} PUBLIC ${ARG_DEPS})
    endif()

    target_compile_options(${TARGET} PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
        $<$<C_COMPILER_ID:MSVC>:/W4>
    )

    if(ARG_COMPILE_OPTIONS)
        target_compile_options(${TARGET} PRIVATE ${ARG_COMPILE_OPTIONS})
    endif()

    if(ARG_LINK_OPTIONS)
        target_link_options(${TARGET} PRIVATE ${ARG_LINK_OPTIONS})
    endif()

    set_target_properties(${TARGET} PROPERTIES
        EXPORT_NAME ${TARGET}
        VERSION ${PROJECT_VERSION}
    )

    message(STATUS "Added Nexus library: ${TARGET}")
endfunction()

##############################################################################
# Executable Creation
##############################################################################

#
# Add a Nexus executable with standard settings
# Arguments:
#   TARGET: Target name
#   SOURCES: Source files
#   DEPS: Dependencies
#   LINKER_SCRIPT: Linker script path (optional)
#   COMPILE_OPTIONS: Additional compile options (optional)
#   LINK_OPTIONS: Additional link options (optional)
#
function(nexus_add_executable TARGET)
    cmake_parse_arguments(
        ARG
        ""
        "LINKER_SCRIPT"
        "SOURCES;DEPS;COMPILE_OPTIONS;LINK_OPTIONS"
        ${ARGN}
    )

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "nexus_add_executable: SOURCES argument is required")
    endif()

    add_executable(${TARGET} ${ARG_SOURCES})

    if(ARG_DEPS)
        target_link_libraries(${TARGET} PRIVATE ${ARG_DEPS})
    endif()

    if(ARG_LINKER_SCRIPT)
        if(NOT EXISTS ${ARG_LINKER_SCRIPT})
            message(FATAL_ERROR "Linker script not found: ${ARG_LINKER_SCRIPT}")
        endif()

        nexus_set_linker_script(${TARGET} ${ARG_LINKER_SCRIPT})
        nexus_generate_map(${TARGET})

        set_target_properties(${TARGET} PROPERTIES
            LINK_DEPENDS ${ARG_LINKER_SCRIPT}
        )
    endif()

    if(ARG_COMPILE_OPTIONS)
        target_compile_options(${TARGET} PRIVATE ${ARG_COMPILE_OPTIONS})
    endif()

    if(ARG_LINK_OPTIONS)
        target_link_options(${TARGET} PRIVATE ${ARG_LINK_OPTIONS})
    endif()

    if(NOT NEXUS_PLATFORM STREQUAL "native")
        nexus_generate_binary(${TARGET})
    endif()

    nexus_generate_memory_report(${TARGET})

    message(STATUS "Added Nexus executable: ${TARGET}")
endfunction()

##############################################################################
# Application Creation
##############################################################################

#
# Add a Nexus application with complete configuration
# This is a high-level function that combines executable creation,
# library linking, and post-build actions.
#
# Arguments:
#   TARGET: Application target name (required)
#   SOURCES: Application source files (required)
#   VERSION: Application version string (optional, default: "1.0.0")
#   PLATFORM_DEPS: Platform dependencies (optional, auto-detected if not provided)
#   EXTRA_DEPS: Additional dependencies (optional)
#   INCLUDES: Include directories (optional)
#   LINKER_SCRIPT_DIR: Linker script directory (optional, auto-detected)
#   GENERATE_BIN: Generate .bin file (optional, default: from Kconfig)
#   GENERATE_HEX: Generate .hex file (optional, default: from Kconfig)
#   PRINT_SIZE: Print size information (optional, default: from Kconfig)
#
# Example:
#   nexus_add_application(
#       TARGET my_app
#       SOURCES main.c app.c
#       VERSION "1.2.3"
#       EXTRA_DEPS my_lib
#   )
#
function(nexus_add_application)
    cmake_parse_arguments(
        APP
        ""
        "TARGET;VERSION;LINKER_SCRIPT_DIR;GENERATE_BIN;GENERATE_HEX;PRINT_SIZE"
        "SOURCES;PLATFORM_DEPS;EXTRA_DEPS;INCLUDES"
        ${ARGN}
    )

    # Validate required arguments
    if(NOT APP_TARGET)
        nexus_fatal_error("nexus_add_application: TARGET is required")
    endif()

    if(NOT APP_SOURCES)
        nexus_fatal_error("nexus_add_application: SOURCES is required")
    endif()

    # Set default version
    if(NOT APP_VERSION)
        if(DEFINED CONFIG_APP_VERSION_MAJOR AND DEFINED CONFIG_APP_VERSION_MINOR AND DEFINED CONFIG_APP_VERSION_PATCH)
            set(APP_VERSION "${CONFIG_APP_VERSION_MAJOR}.${CONFIG_APP_VERSION_MINOR}.${CONFIG_APP_VERSION_PATCH}")
        else()
            set(APP_VERSION "1.0.0")
        endif()
    endif()

    nexus_log(VERBOSE "Creating application: ${APP_TARGET} v${APP_VERSION}")

    #-------------------------------------------------------------------------
    # Step 1: Create Executable
    #-------------------------------------------------------------------------

    add_executable(${APP_TARGET} ${APP_SOURCES})

    # Set executable suffix for embedded targets
    if(NOT NEXUS_PLATFORM STREQUAL "native")
        set_target_properties(${APP_TARGET} PROPERTIES SUFFIX ".elf")
    endif()

    #-------------------------------------------------------------------------
    # Step 2: Configure Include Directories
    #-------------------------------------------------------------------------

    if(APP_INCLUDES)
        target_include_directories(${APP_TARGET} PRIVATE ${APP_INCLUDES})
    endif()

    # Always include current source directory
    target_include_directories(${APP_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    #-------------------------------------------------------------------------
    # Step 3: Link Dependencies
    #-------------------------------------------------------------------------

    # Auto-detect platform dependencies if not provided
    if(NOT APP_PLATFORM_DEPS)
        if(TARGET platform_${NEXUS_PLATFORM})
            set(APP_PLATFORM_DEPS platform_${NEXUS_PLATFORM})
        elseif(TARGET platform_stm32)
            set(APP_PLATFORM_DEPS platform_stm32)
        endif()
    endif()

    # Link platform dependencies
    if(APP_PLATFORM_DEPS)
        target_link_libraries(${APP_TARGET} PRIVATE ${APP_PLATFORM_DEPS})
        nexus_log(VERBOSE "  Platform deps: ${APP_PLATFORM_DEPS}")
    endif()

    # Link HAL and OSAL
    if(TARGET hal)
        target_link_libraries(${APP_TARGET} PRIVATE hal hal_interface)
    endif()
    if(TARGET osal)
        target_link_libraries(${APP_TARGET} PRIVATE osal osal_interface)
    endif()

    # Link extra dependencies
    if(APP_EXTRA_DEPS)
        target_link_libraries(${APP_TARGET} PRIVATE ${APP_EXTRA_DEPS})
        nexus_log(VERBOSE "  Extra deps: ${APP_EXTRA_DEPS}")
    endif()

    #-------------------------------------------------------------------------
    # Step 4: Add Compile Definitions
    #-------------------------------------------------------------------------

    target_compile_definitions(${APP_TARGET}
        PRIVATE
            APP_NAME="${APP_TARGET}"
            APP_VERSION="${APP_VERSION}"
    )

    # Add version information if enabled
    if(CONFIG_APP_ENABLE_VERSION_INFO)
        string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S" UTC)
        target_compile_definitions(${APP_TARGET}
            PRIVATE
                APP_BUILD_TIMESTAMP="${BUILD_TIMESTAMP}"
        )
    endif()

    # Add debug output flag if enabled
    if(CONFIG_APP_ENABLE_DEBUG_OUTPUT)
        target_compile_definitions(${APP_TARGET}
            PRIVATE
                APP_DEBUG_OUTPUT=1
        )
    endif()

    #-------------------------------------------------------------------------
    # Step 5: Configure Linker Script (for embedded targets)
    #-------------------------------------------------------------------------

    if(NOT NEXUS_PLATFORM STREQUAL "native")
        # Configure linker script only if explicitly provided
        # Otherwise, linker script is inherited from platform library
        if(APP_LINKER_SCRIPT_DIR)
            nexus_configure_linker_script(${APP_TARGET} ${APP_LINKER_SCRIPT_DIR})
        else()
            nexus_log(VERBOSE "  Linker script inherited from platform library")
        endif()

        # Add garbage collection
        if(NOT NEXUS_COMPILER_ARM_CLANG)
            target_link_options(${APP_TARGET} PRIVATE -Wl,--gc-sections)

            if(CONFIG_APP_PRINT_MEMORY_USAGE)
                target_link_options(${APP_TARGET} PRIVATE -Wl,--print-memory-usage)
            endif()
        endif()
    endif()

    #-------------------------------------------------------------------------
    # Step 6: Post-Build Actions
    #-------------------------------------------------------------------------

    # Generate map file
    if(CONFIG_APP_GENERATE_MAP OR NOT DEFINED CONFIG_APP_GENERATE_MAP)
        nexus_generate_map(${APP_TARGET})
    endif()

    # Generate binary file
    set(SHOULD_GENERATE_BIN ${CONFIG_APP_GENERATE_BIN})
    if(DEFINED APP_GENERATE_BIN)
        set(SHOULD_GENERATE_BIN ${APP_GENERATE_BIN})
    endif()
    if(SHOULD_GENERATE_BIN AND CMAKE_OBJCOPY AND NOT NEXUS_PLATFORM STREQUAL "native")
        nexus_generate_bin(${APP_TARGET})
    endif()

    # Generate hex file
    set(SHOULD_GENERATE_HEX ${CONFIG_APP_GENERATE_HEX})
    if(DEFINED APP_GENERATE_HEX)
        set(SHOULD_GENERATE_HEX ${APP_GENERATE_HEX})
    endif()
    if(SHOULD_GENERATE_HEX AND CMAKE_OBJCOPY AND NOT NEXUS_PLATFORM STREQUAL "native")
        nexus_generate_hex(${APP_TARGET})
    endif()

    # Print size information
    set(SHOULD_PRINT_SIZE ${CONFIG_APP_PRINT_SIZE})
    if(DEFINED APP_PRINT_SIZE)
        set(SHOULD_PRINT_SIZE ${APP_PRINT_SIZE})
    endif()
    if(SHOULD_PRINT_SIZE AND CMAKE_SIZE)
        nexus_print_target_size(${APP_TARGET})
    endif()

    #-------------------------------------------------------------------------
    # Configuration Summary
    #-------------------------------------------------------------------------

    message(STATUS "")
    message(STATUS "Application: ${APP_TARGET}")
    message(STATUS "  Version:        ${APP_VERSION}")
    message(STATUS "  Platform:       ${NEXUS_PLATFORM}")
    if(APP_PLATFORM_DEPS)
        message(STATUS "  Platform Deps:  ${APP_PLATFORM_DEPS}")
    endif()
    if(APP_EXTRA_DEPS)
        message(STATUS "  Extra Deps:     ${APP_EXTRA_DEPS}")
    endif()
    list(LENGTH APP_SOURCES SOURCE_COUNT)
    message(STATUS "  Sources:        ${SOURCE_COUNT} files")
    message(STATUS "")

    nexus_log(STATUS "Application ${APP_TARGET} configured successfully")
endfunction()

##############################################################################
# Test Creation
##############################################################################

#
# Add a Nexus test with Google Test integration
# Arguments:
#   TARGET: Target name
#   SOURCES: Source files
#   DEPS: Dependencies
#   LABELS: Test labels (optional)
#   TIMEOUT: Test timeout in seconds (optional)
#
function(nexus_add_test TARGET)
    cmake_parse_arguments(
        ARG
        ""
        "TIMEOUT"
        "SOURCES;DEPS;LABELS"
        ${ARGN}
    )

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "nexus_add_test: SOURCES argument is required")
    endif()

    add_executable(${TARGET} ${ARG_SOURCES})

    target_link_libraries(${TARGET}
        PRIVATE
            ${ARG_DEPS}
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
    )

    set(TEST_PROPERTIES "")

    if(ARG_LABELS)
        list(APPEND TEST_PROPERTIES LABELS "${ARG_LABELS}")
    endif()

    if(ARG_TIMEOUT)
        list(APPEND TEST_PROPERTIES TIMEOUT ${ARG_TIMEOUT})
    endif()

    include(GoogleTest)
    gtest_discover_tests(${TARGET}
        PROPERTIES ${TEST_PROPERTIES}
    )

    message(STATUS "Added Nexus test: ${TARGET}")
endfunction()

##############################################################################
# Binary Generation
##############################################################################

#
# Generate .bin file from executable
# Arguments:
#   TARGET: Target name
#
function(nexus_generate_bin TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "Target does not exist: ${TARGET}")
    endif()

    # Use unified toolchain interface
    nexus_generate_outputs(${TARGET} FORMATS BIN)
endfunction()

#
# Generate .hex file from executable
# Arguments:
#   TARGET: Target name
#
function(nexus_generate_hex TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "Target does not exist: ${TARGET}")
    endif()

    # Use unified toolchain interface
    nexus_generate_outputs(${TARGET} FORMATS HEX)
endfunction()

#
# Generate .map file from executable
# Arguments:
#   TARGET: Target name
#
function(nexus_generate_map TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "Target does not exist: ${TARGET}")
    endif()

    # Detect toolchain
    nexus_get_current_toolchain(TOOLCHAIN)
    string(TOLOWER "${TOOLCHAIN}" TOOLCHAIN_LOWER)

    # Configure map file generation based on toolchain
    if(TOOLCHAIN_LOWER MATCHES "arm.*gcc|gnu.*arm")
        target_link_options(${TARGET} PRIVATE -Wl,-Map=${TARGET}.map)
    elseif(TOOLCHAIN_LOWER MATCHES "armclang")
        target_link_options(${TARGET} PRIVATE --map --list=${TARGET}.map)
    elseif(TOOLCHAIN_LOWER MATCHES "iar")
        target_link_options(${TARGET} PRIVATE --map ${TARGET}.map)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
        target_link_options(${TARGET} PRIVATE -Wl,-Map=${TARGET}.map)
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_link_options(${TARGET} PRIVATE -Wl,-Map,${TARGET}.map)
    elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
        target_link_options(${TARGET} PRIVATE /MAP:${TARGET}.map)
    endif()
endfunction()

#
# Generate binary files (.bin, .hex) from executable
# Arguments:
#   TARGET: Target name
#
function(nexus_generate_binary TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "Target does not exist: ${TARGET}")
    endif()

    nexus_generate_bin(${TARGET})
    nexus_generate_hex(${TARGET})
endfunction()

##############################################################################
# Memory Usage Report
##############################################################################

#
# Generate memory usage report for executable
# Arguments:
#   TARGET: Target name
#
function(nexus_generate_memory_report TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "Target does not exist: ${TARGET}")
    endif()

    if(CMAKE_SIZE)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${TARGET}>
            COMMENT "Memory usage for ${TARGET}:"
        )
    endif()
endfunction()

#
# Print target size information (alias for nexus_generate_memory_report)
# Arguments:
#   TARGET: Target name
#
function(nexus_print_target_size TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "Target does not exist: ${TARGET}")
    endif()

    if(CMAKE_SIZE)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET}>
            COMMENT "Size information for ${TARGET}:"
        )
    else()
        nexus_log(VERBOSE "CMAKE_SIZE not found, skipping size report for ${TARGET}")
    endif()
endfunction()

##############################################################################
# Precompiled Header Support
##############################################################################

#
# Add precompiled header to target
# Arguments:
#   TARGET: Target name
#   HEADER: Header file path
#
function(nexus_add_precompiled_header TARGET HEADER)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "Target does not exist: ${TARGET}")
    endif()

    if(NOT EXISTS ${HEADER})
        message(FATAL_ERROR "Header file not found: ${HEADER}")
    endif()

    target_precompile_headers(${TARGET} PRIVATE ${HEADER})

    message(STATUS "Added precompiled header to ${TARGET}: ${HEADER}")
endfunction()

##############################################################################
# Platform Library Creation
##############################################################################

#
# Create a platform library with unified configuration
# Arguments:
#   TARGET: Target name
#   PLATFORM: Platform name (stm32, native, esp32, etc.)
#   SERIES: Platform series (optional, e.g., f4 for STM32F4)
#   CORE_SOURCES: Core source files (always included)
#   MODULE_SOURCES: Module names to check against Kconfig
#   MODULE_BASE_DIR: Base directory for module sources (optional)
#   VENDOR_LIBS: Vendor libraries to link (optional)
#   INCLUDES: Public include directories (optional)
#   PRIVATE_INCLUDES: Private include directories (optional)
#
function(nexus_add_platform_library)
    cmake_parse_arguments(ARG
        ""
        "TARGET;PLATFORM;SERIES;MODULE_BASE_DIR"
        "CORE_SOURCES;MODULE_SOURCES;VENDOR_LIBS;INCLUDES;PRIVATE_INCLUDES"
        ${ARGN}
    )

    if(NOT ARG_TARGET)
        nexus_fatal_error("nexus_add_platform_library: TARGET is required")
    endif()

    if(NOT ARG_PLATFORM)
        nexus_fatal_error("nexus_add_platform_library: PLATFORM is required")
    endif()

    # Create library
    add_library(${ARG_TARGET} STATIC)

    # Add core source files
    if(ARG_CORE_SOURCES)
        target_sources(${ARG_TARGET} PRIVATE ${ARG_CORE_SOURCES})
        list(LENGTH ARG_CORE_SOURCES CORE_COUNT)
        nexus_log(VERBOSE "Added ${CORE_COUNT} core sources to ${ARG_TARGET}")
    endif()

    # Add module source files (based on Kconfig)
    if(ARG_MODULE_SOURCES)
        if(NOT ARG_MODULE_BASE_DIR)
            set(ARG_MODULE_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
        endif()

        nexus_configure_platform_modules(${ARG_TARGET}
            MODULES ${ARG_MODULE_SOURCES}
            BASE_DIR ${ARG_MODULE_BASE_DIR}
            PREFIX "HAL"
        )
    endif()

    # Add public include directories
    if(ARG_INCLUDES)
        target_include_directories(${ARG_TARGET} PUBLIC ${ARG_INCLUDES})
    endif()

    # Add private include directories
    if(ARG_PRIVATE_INCLUDES)
        target_include_directories(${ARG_TARGET} PRIVATE ${ARG_PRIVATE_INCLUDES})
    endif()

    # Link vendor libraries
    if(ARG_VENDOR_LIBS)
        target_link_libraries(${ARG_TARGET} PUBLIC ${ARG_VENDOR_LIBS})
    endif()

    # Link HAL and OSAL
    if(TARGET hal)
        target_link_libraries(${ARG_TARGET} PUBLIC hal)
    endif()

    if(TARGET osal)
        target_link_libraries(${ARG_TARGET} PUBLIC osal)
    endif()

    # Set platform-specific compile definitions
    string(TOUPPER ${ARG_PLATFORM} PLATFORM_UPPER)
    target_compile_definitions(${ARG_TARGET}
        PUBLIC
            NEXUS_PLATFORM_${PLATFORM_UPPER}
            $<$<CONFIG:Debug>:PLATFORM_DEBUG>
    )

    if(ARG_SERIES)
        string(TOUPPER ${ARG_SERIES} SERIES_UPPER)
        target_compile_definitions(${ARG_TARGET}
            PUBLIC NEXUS_SERIES_${SERIES_UPPER}
        )
    endif()

    message(STATUS "Platform library ${ARG_TARGET} configured")
endfunction()

##############################################################################
# Vendor Library Integration
##############################################################################

#
# Add a vendor library
# Arguments:
#   NAME: Library name
#   SOURCE_DIR: Vendor source directory
#   INTERFACE_ONLY: Create interface library (optional)
#   COMPILE_DEFINITIONS: Compile definitions (optional)
#
function(nexus_add_vendor_library)
    cmake_parse_arguments(
        ARG
        "INTERFACE_ONLY"
        "NAME;SOURCE_DIR"
        "COMPILE_DEFINITIONS"
        ${ARGN}
    )

    if(NOT ARG_NAME)
        message(FATAL_ERROR "nexus_add_vendor_library: NAME argument is required")
    endif()

    if(NOT ARG_SOURCE_DIR)
        message(FATAL_ERROR "nexus_add_vendor_library: SOURCE_DIR argument is required")
    endif()

    if(NOT EXISTS ${ARG_SOURCE_DIR})
        message(FATAL_ERROR "Vendor directory not found: ${ARG_SOURCE_DIR}")
    endif()

    if(ARG_INTERFACE_ONLY)
        add_library(${ARG_NAME} INTERFACE)

        target_include_directories(${ARG_NAME}
            INTERFACE ${ARG_SOURCE_DIR}/include
        )

        if(ARG_COMPILE_DEFINITIONS)
            target_compile_definitions(${ARG_NAME}
                INTERFACE ${ARG_COMPILE_DEFINITIONS}
            )
        endif()
    else()
        file(GLOB_RECURSE VENDOR_SOURCES
            ${ARG_SOURCE_DIR}/*.c
            ${ARG_SOURCE_DIR}/*.cpp
        )

        if(NOT VENDOR_SOURCES)
            message(FATAL_ERROR "No source files found in: ${ARG_SOURCE_DIR}")
        endif()

        add_library(${ARG_NAME} STATIC ${VENDOR_SOURCES})

        target_include_directories(${ARG_NAME}
            PUBLIC ${ARG_SOURCE_DIR}/include
        )

        if(ARG_COMPILE_DEFINITIONS)
            target_compile_definitions(${ARG_NAME}
                PUBLIC ${ARG_COMPILE_DEFINITIONS}
            )
        endif()
    endif()

    message(STATUS "Added vendor library: ${ARG_NAME}")
endfunction()

message(STATUS "NexusBuild module loaded")

##############################################################################
# End of NexusBuild.cmake
##############################################################################
