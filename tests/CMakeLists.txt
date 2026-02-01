##############################################################################
# Nexus Test Framework
##############################################################################

cmake_minimum_required(VERSION 3.16)

message(STATUS "Configuring Nexus Test Framework...")

#-----------------------------------------------------------------------------
# Enable Testing
#-----------------------------------------------------------------------------

enable_testing()

#-----------------------------------------------------------------------------
# Google Test Setup
#-----------------------------------------------------------------------------

include(FetchContent)

# Declare Google Test dependency
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)

# Configure Google Test options
set(gtest_force_shared_crt ON CACHE BOOL "Force shared CRT for MSVC" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "Build GMock" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Disable GTest installation" FORCE)

# Make Google Test available
FetchContent_MakeAvailable(googletest)

# Include Google Test module for test discovery
include(GoogleTest)

message(STATUS "Google Test: v1.14.0")

#-----------------------------------------------------------------------------
# Common Test Configuration
#-----------------------------------------------------------------------------

# Platform-specific threading libraries
if(WIN32)
    set(THREAD_LIBS "")
else()
    find_package(Threads REQUIRED)
    set(THREAD_LIBS Threads::Threads)
endif()

# Common include directories for all tests
set(NEXUS_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/osal/include
    ${CMAKE_SOURCE_DIR}/platforms/native/include
    ${CMAKE_SOURCE_DIR}/framework/log/include
    ${CMAKE_SOURCE_DIR}/framework/shell/include
    ${CMAKE_SOURCE_DIR}/framework/config/include
    ${CMAKE_SOURCE_DIR}/framework/init/include
    ${CMAKE_SOURCE_DIR}
)

# Common link libraries for all tests
set(NEXUS_TEST_LIBS
    GTest::gtest
    GTest::gmock
    platform_native
    hal_interface
    hal
    log_framework
    shell_framework
    config_framework
    init_framework
    osal
    ${THREAD_LIBS}
)

# Common compile options
set(NEXUS_TEST_COMPILE_OPTIONS "")
if(MSVC)
    list(APPEND NEXUS_TEST_COMPILE_OPTIONS /W4 /FS)
else()
    list(APPEND NEXUS_TEST_COMPILE_OPTIONS -Wall -Wextra)
endif()

#-----------------------------------------------------------------------------
# Helper Function for Test Discovery
#-----------------------------------------------------------------------------

# Wrapper function for gtest_discover_tests
# Handles generator-specific issues and provides consistent interface
# Arguments:
#   TARGET: Test executable target name
#   PROPERTIES: Optional test properties
#   LABELS: Optional test labels
function(nexus_discover_tests TARGET)
    # Parse additional arguments
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs PROPERTIES LABELS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Use PRE_TEST mode for Visual Studio to avoid path issues
    # Use POST_BUILD for other generators for better performance
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        gtest_discover_tests(${TARGET}
            DISCOVERY_MODE PRE_TEST
            PROPERTIES ${ARG_PROPERTIES}
        )
    else()
        gtest_discover_tests(${TARGET}
            PROPERTIES ${ARG_PROPERTIES}
        )
    endif()

    message(STATUS "Test suite registered: ${TARGET}")
endfunction()

#-----------------------------------------------------------------------------
# Test Subdirectories
#-----------------------------------------------------------------------------

message(STATUS "Adding test subdirectories...")

# HAL tests
add_subdirectory(hal)

# OSAL tests
add_subdirectory(osal)

# Framework tests
add_subdirectory(config)
add_subdirectory(log)
add_subdirectory(shell)
add_subdirectory(init)

# Integration tests
add_subdirectory(integration)

# Platform tests
add_subdirectory(platform)

# Refactoring tests
add_subdirectory(refactoring)

# Build system tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/build_system)
    add_subdirectory(build_system)
endif()

#-----------------------------------------------------------------------------
# CTest Configuration
#-----------------------------------------------------------------------------

# Enable parallel test execution
if(NOT DEFINED CTEST_PARALLEL_LEVEL)
    include(ProcessorCount)
    ProcessorCount(N)
    if(NOT N EQUAL 0)
        set(CTEST_PARALLEL_LEVEL ${N} CACHE STRING "Number of parallel test jobs")
    else()
        set(CTEST_PARALLEL_LEVEL 1 CACHE STRING "Number of parallel test jobs")
    endif()
endif()

# Configure test timeout (default 5 minutes)
if(NOT DEFINED CTEST_TEST_TIMEOUT)
    set(CTEST_TEST_TIMEOUT 300 CACHE STRING "Test timeout in seconds")
endif()

# Configure test output on failure
set(CTEST_OUTPUT_ON_FAILURE ON CACHE BOOL "Show test output on failure")

#-----------------------------------------------------------------------------
# Test Summary
#-----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "=== Nexus Test Configuration ===")
message(STATUS "  Google Test:    v1.14.0")
message(STATUS "  Parallel level: ${CTEST_PARALLEL_LEVEL}")
message(STATUS "  Test timeout:   ${CTEST_TEST_TIMEOUT}s")
message(STATUS "  Output on fail: ${CTEST_OUTPUT_ON_FAILURE}")
message(STATUS "================================")
message(STATUS "")

#-----------------------------------------------------------------------------
# End of Nexus Test Framework
#-----------------------------------------------------------------------------
