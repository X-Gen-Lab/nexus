##############################################################################
# NexusBuild.cmake - Core Build Module
##############################################################################
#
# NexusBuild.cmake
# Core build functions for Nexus build system
# Author: Nexus Team
#
# This module provides the core build functions for the Nexus build system,
# including library, executable, and test creation with standard settings.
#
##############################################################################

include_guard(GLOBAL)

# Include incremental build module
include(NexusIncremental)

# Include performance optimization module
include(NexusPerformance)

##############################################################################
# Library Creation
##############################################################################

#
# Add a Nexus library with standard settings
# TARGET: Target name
# SOURCES: Source files
# INCLUDES: Public include directories
# DEPS: Dependencies
# PRIVATE_INCLUDES: Private include directories (optional)
# COMPILE_OPTIONS: Additional compile options (optional)
# LINK_OPTIONS: Additional link options (optional)
#
function(nexus_add_library TARGET)
    cmake_parse_arguments(
        ARG
        ""
        ""
        "SOURCES;INCLUDES;DEPS;PRIVATE_INCLUDES;COMPILE_OPTIONS;LINK_OPTIONS"
        ${ARGN}
    )

    # Validate required arguments
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "nexus_add_library: SOURCES argument is required")
    endif()

    # Create static library
    add_library(${TARGET} STATIC ${ARG_SOURCES})

    # Set public include directories
    if(ARG_INCLUDES)
        target_include_directories(${TARGET}
            PUBLIC
                ${ARG_INCLUDES}
        )
    endif()

    # Set private include directories
    if(ARG_PRIVATE_INCLUDES)
        target_include_directories(${TARGET}
            PRIVATE
                ${ARG_PRIVATE_INCLUDES}
        )
    endif()

    # Link dependencies
    if(ARG_DEPS)
        target_link_libraries(${TARGET} PUBLIC ${ARG_DEPS})
    endif()

    # Apply standard compile options
    target_compile_options(${TARGET} PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
        $<$<C_COMPILER_ID:MSVC>:/W4>
    )

    # Apply additional compile options
    if(ARG_COMPILE_OPTIONS)
        target_compile_options(${TARGET} PRIVATE ${ARG_COMPILE_OPTIONS})
    endif()

    # Apply additional link options
    if(ARG_LINK_OPTIONS)
        target_link_options(${TARGET} PRIVATE ${ARG_LINK_OPTIONS})
    endif()

    # Generate CMake export target
    set_target_properties(${TARGET} PROPERTIES
        EXPORT_NAME ${TARGET}
        VERSION ${PROJECT_VERSION}
    )

    # Enable incremental build by default
    nexus_enable_incremental_build(${TARGET})

    message(STATUS "Added Nexus library: ${TARGET}")
endfunction()

##############################################################################
# Executable Creation
##############################################################################

#
# Add a Nexus executable with standard settings
# TARGET: Target name
# SOURCES: Source files
# DEPS: Dependencies
# LINKER_SCRIPT: Linker script path (optional)
# COMPILE_OPTIONS: Additional compile options (optional)
# LINK_OPTIONS: Additional link options (optional)
#
function(nexus_add_executable TARGET)
    cmake_parse_arguments(
        ARG
        ""
        "LINKER_SCRIPT"
        "SOURCES;DEPS;COMPILE_OPTIONS;LINK_OPTIONS"
        ${ARGN}
    )

    # Validate required arguments
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "nexus_add_executable: SOURCES argument is required")
    endif()

    # Create executable
    add_executable(${TARGET} ${ARG_SOURCES})

    # Link dependencies
    if(ARG_DEPS)
        target_link_libraries(${TARGET} PRIVATE ${ARG_DEPS})
    endif()

    # Apply linker script if provided
    if(ARG_LINKER_SCRIPT)
        if(NOT EXISTS ${ARG_LINKER_SCRIPT})
            message(FATAL_ERROR "Linker script not found: ${ARG_LINKER_SCRIPT}")
        endif()

        target_link_options(${TARGET} PRIVATE
            -T${ARG_LINKER_SCRIPT}
            -Wl,-Map=${TARGET}.map
        )

        set_target_properties(${TARGET} PROPERTIES
            LINK_DEPENDS ${ARG_LINKER_SCRIPT}
        )
    endif()

    # Apply additional compile options
    if(ARG_COMPILE_OPTIONS)
        target_compile_options(${TARGET} PRIVATE ${ARG_COMPILE_OPTIONS})
    endif()

    # Apply additional link options
    if(ARG_LINK_OPTIONS)
        target_link_options(${TARGET} PRIVATE ${ARG_LINK_OPTIONS})
    endif()

    # Generate binary files for embedded targets
    if(NOT NEXUS_PLATFORM STREQUAL "native")
        nexus_generate_binary(${TARGET})
    endif()

    # Generate memory usage report
    nexus_generate_memory_report(${TARGET})

    # Enable incremental build and linking by default
    nexus_enable_incremental_build(${TARGET})
    nexus_enable_incremental_linking(${TARGET})

    message(STATUS "Added Nexus executable: ${TARGET}")
endfunction()

##############################################################################
# Test Creation
##############################################################################

#
# Add a Nexus test with Google Test integration
# TARGET: Target name
# SOURCES: Source files
# DEPS: Dependencies
# LABELS: Test labels (optional)
# TIMEOUT: Test timeout in seconds (optional)
#
function(nexus_add_test TARGET)
    cmake_parse_arguments(
        ARG
        ""
        "TIMEOUT"
        "SOURCES;DEPS;LABELS"
        ${ARGN}
    )

    # Validate required arguments
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "nexus_add_test: SOURCES argument is required")
    endif()

    # Create test executable
    add_executable(${TARGET} ${ARG_SOURCES})

    # Link Google Test and dependencies
    target_link_libraries(${TARGET}
        PRIVATE
            ${ARG_DEPS}
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
    )

    # Set test properties
    set(TEST_PROPERTIES "")

    if(ARG_LABELS)
        list(APPEND TEST_PROPERTIES LABELS "${ARG_LABELS}")
    endif()

    if(ARG_TIMEOUT)
        list(APPEND TEST_PROPERTIES TIMEOUT ${ARG_TIMEOUT})
    endif()

    # Register with CTest
    include(GoogleTest)
    gtest_discover_tests(${TARGET}
        PROPERTIES ${TEST_PROPERTIES}
    )

    message(STATUS "Added Nexus test: ${TARGET}")
endfunction()

##############################################################################
# Precompiled Header Support
##############################################################################

#
# Add precompiled header to target
# TARGET: Target name
# HEADER: Header file path
#
function(nexus_add_precompiled_header TARGET HEADER)
    # Validate arguments
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "Target does not exist: ${TARGET}")
    endif()

    if(NOT EXISTS ${HEADER})
        message(FATAL_ERROR "Header file not found: ${HEADER}")
    endif()

    # Apply precompiled header
    target_precompile_headers(${TARGET} PRIVATE ${HEADER})

    message(STATUS "Added precompiled header to ${TARGET}: ${HEADER}")
endfunction()

##############################################################################
# Binary Generation (for embedded targets)
##############################################################################

#
# Generate binary files (.bin, .hex) from executable
# TARGET: Target name
#
function(nexus_generate_binary TARGET)
    # Validate target
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "Target does not exist: ${TARGET}")
    endif()

    # Generate .bin file
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> ${TARGET}.bin
        COMMENT "Generating binary file: ${TARGET}.bin"
    )

    # Generate .hex file
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET}> ${TARGET}.hex
        COMMENT "Generating hex file: ${TARGET}.hex"
    )
endfunction()

##############################################################################
# Memory Usage Report
##############################################################################

#
# Generate memory usage report for executable
# TARGET: Target name
#
function(nexus_generate_memory_report TARGET)
    # Validate target
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "Target does not exist: ${TARGET}")
    endif()

    # Generate memory report using size tool
    if(CMAKE_SIZE)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${TARGET}>
            COMMENT "Memory usage for ${TARGET}:"
        )
    endif()
endfunction()
