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

# Use the exact repository-pinned dependency. Configuration performs no downloads.
if(NOT EXISTS "${NEXUS_SOURCE_DIR}/ext/googletest/googletest/src/gtest-all.cc")
    message(FATAL_ERROR "Initialize ext/googletest with git submodule update --init")
endif()
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
# The pinned dependency declares CMake 2.8 compatibility. CMake 4 removed it.
# Limit the compatibility floor to this known legacy dependency subtree.
set(_saved_policy_minimum "${CMAKE_POLICY_VERSION_MINIMUM}")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory("${NEXUS_SOURCE_DIR}/ext/googletest" "${NEXUS_BINARY_DIR}/ext/googletest")
set(CMAKE_POLICY_VERSION_MINIMUM "${_saved_policy_minimum}")

# Include Google Test module for test discovery
include(GoogleTest)

message(STATUS "Google Test: pinned submodule")

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
    ${NEXUS_SOURCE_DIR}/hal/include
    ${NEXUS_SOURCE_DIR}/osal/include
    ${NEXUS_SOURCE_DIR}/platforms/native/include
    ${NEXUS_SOURCE_DIR}/framework/log/include
    ${NEXUS_SOURCE_DIR}/framework/shell/include
    ${NEXUS_SOURCE_DIR}/framework/config/include
    ${NEXUS_SOURCE_DIR}/framework/init/include
    ${NEXUS_CONFIG_DIR}
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

    # LABELS is parsed separately even when callers place it after PROPERTIES.
    # Preserve the list as one property value rather than silently dropping it.
    if(ARG_LABELS)
        string(REPLACE ";" "\\;" _labels "${ARG_LABELS}")
        list(APPEND ARG_PROPERTIES LABELS "${_labels}")
    endif()

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
add_subdirectory(storage_security)
add_subdirectory(drivers)
add_subdirectory(industrial)

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
message(STATUS "  Google Test:    pinned submodule")
message(STATUS "  Parallel level: ${CTEST_PARALLEL_LEVEL}")
message(STATUS "  Test timeout:   ${CTEST_TEST_TIMEOUT}s")
message(STATUS "  Output on fail: ${CTEST_OUTPUT_ON_FAILURE}")
message(STATUS "================================")
message(STATUS "")

#-----------------------------------------------------------------------------
# End of Nexus Test Framework
#-----------------------------------------------------------------------------

# Run real configuration generation and CMake consumer regressions.
add_test(NAME effective_configuration
    COMMAND "${Python3_EXECUTABLE}" "${NEXUS_SOURCE_DIR}/scripts/kconfig/test_effective_config.py")
set_tests_properties(effective_configuration PROPERTIES LABELS "build;configuration" TIMEOUT 60)
add_test(NAME external_sdk_consumer
    COMMAND "${Python3_EXECUTABLE}" "${NEXUS_SOURCE_DIR}/scripts/kconfig/test_external_consumer.py")
set_tests_properties(external_sdk_consumer PROPERTIES LABELS "build;sdk;configuration" TIMEOUT 120)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_test(NAME platform_component_objects
        COMMAND "${Python3_EXECUTABLE}" "${NEXUS_SOURCE_DIR}/scripts/kconfig/test_component_objects.py")
    set_tests_properties(platform_component_objects PROPERTIES LABELS "build;platform;host-link" TIMEOUT 120)
endif()

if(TARGET blinky)
    # Execute the real application and production factory, without fixture setup.
    add_test(NAME native_blinky_smoke COMMAND blinky --cycles 3)
    set_tests_properties(native_blinky_smoke PROPERTIES LABELS "application;native;contract" TIMEOUT 10)
endif()

if(TARGET nexus_industrial_controller)
    add_test(NAME native_industrial_controller_smoke COMMAND nexus_industrial_controller)
    set_tests_properties(native_industrial_controller_smoke PROPERTIES LABELS "application;industrial;native;contract" TIMEOUT 10)
endif()

if(TARGET config_demo)
    add_test(NAME native_config_demo_smoke COMMAND config_demo)
    set_tests_properties(native_config_demo_smoke PROPERTIES LABELS "application;configuration;native;contract" TIMEOUT 10)
endif()
if(TARGET shell_demo)
    add_test(NAME native_shell_demo_smoke COMMAND shell_demo --smoke)
    set_tests_properties(native_shell_demo_smoke PROPERTIES LABELS "application;shell;native;contract" TIMEOUT 10)
endif()

if(TARGET freertos_demo)
    add_test(NAME native_freertos_demo_smoke COMMAND freertos_demo --run-ms 500)
    set_tests_properties(native_freertos_demo_smoke PROPERTIES LABELS "application;osal;native;contract" TIMEOUT 10)
endif()
