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
    ${NEXUS_SOURCE_DIR}/soc/native/private
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
    osal
    ${THREAD_LIBS}
)
foreach(component log shell config init)
    if(TARGET "${component}_framework")
        list(APPEND NEXUS_TEST_LIBS "${component}_framework")
    endif()
endforeach()

# Full Native crypto checks explicitly compose their selected provider.
if(TARGET nexus_security_openssl)
    list(APPEND NEXUS_TEST_LIBS nexus_security_openssl)
endif()

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
add_subdirectory(arch)
add_subdirectory(boards)
add_subdirectory(hal)

# OSAL tests
add_subdirectory(osal)

# Framework tests
foreach(component config log shell init)
    if(TARGET "${component}_framework")
        add_subdirectory(${component})
    endif()
endforeach()

# Integration tests
if(TARGET config_framework AND TARGET shell_framework AND TARGET log_framework AND TARGET init_framework)
    add_subdirectory(integration)
endif()

# Platform tests
add_subdirectory(platform)
if(CONFIG_SERVICE_STORAGE OR CONFIG_SERVICE_SECURITY OR CONFIG_SERVICE_UPDATE)
    add_subdirectory(storage_security)
endif()
add_subdirectory(drivers)
if(CONFIG_SERVICE_PROTOCOLS AND CONFIG_SERVICE_INDUSTRIAL)
    add_subdirectory(industrial)
endif()

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


add_test(NAME board_package_resources_and_layout
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${NEXUS_SOURCE_DIR}/scripts/configure" -p "test_*.py")
set_tests_properties(board_package_resources_and_layout PROPERTIES LABELS "build;board;layout;contract" TIMEOUT 30)

add_test(NAME installed_source_sdk
    COMMAND "${Python3_EXECUTABLE}" "${NEXUS_SOURCE_DIR}/cmake/package/test_source_sdk.py")
set_tests_properties(installed_source_sdk PROPERTIES LABELS "build;sdk;installed-source" TIMEOUT 240)
