##############################################################################
# Nexus Test Framework
##############################################################################

cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

##############################################################################
# Google Test Setup
##############################################################################

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include Google Test
include(GoogleTest)

##############################################################################
# Common Test Configuration
##############################################################################

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
    osal
    log_framework
    shell_framework
    config_framework
    init_framework
    ${THREAD_LIBS}
)

# Common compile options
set(NEXUS_TEST_COMPILE_OPTIONS "")
if(MSVC)
    list(APPEND NEXUS_TEST_COMPILE_OPTIONS /FS)
endif()

##############################################################################
# Test Subdirectories
##############################################################################

add_subdirectory(hal)
add_subdirectory(osal)
add_subdirectory(config)
add_subdirectory(log)
add_subdirectory(shell)
add_subdirectory(init)
add_subdirectory(integration)

##############################################################################
# CTest Configuration
##############################################################################

# Enable parallel test execution
if(NOT DEFINED CTEST_PARALLEL_LEVEL)
    include(ProcessorCount)
    ProcessorCount(N)
    if(NOT N EQUAL 0)
        set(CTEST_PARALLEL_LEVEL ${N} CACHE STRING "Number of parallel test jobs")
        message(STATUS "CTest parallel level: ${CTEST_PARALLEL_LEVEL}")
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

# Configure test discovery mode
set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST CACHE STRING "Test discovery mode")

message(STATUS "")
message(STATUS "=== CTest Configuration ===")
message(STATUS "  Parallel level: ${CTEST_PARALLEL_LEVEL}")
message(STATUS "  Test timeout:   ${CTEST_TEST_TIMEOUT}s")
message(STATUS "  Output on fail: ${CTEST_OUTPUT_ON_FAILURE}")
message(STATUS "===========================")
message(STATUS "")
