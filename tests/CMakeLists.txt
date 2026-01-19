##############################################################################
# Nexus Test Framework
##############################################################################

cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Fetch Google Test
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

# Test executable
add_executable(nexus_tests
    test_main.cpp
    # HAL tests - CRC
    hal/test_nx_crc.cpp
    hal/test_nx_crc_properties.cpp
    # HAL tests - Flash
    hal/test_nx_flash.cpp
    hal/test_nx_flash_properties.cpp
    # HAL tests - RTC
    hal/test_nx_rtc.cpp
    hal/test_nx_rtc_properties.cpp
    # HAL tests - SDIO
    hal/test_nx_sdio.cpp
    hal/test_nx_sdio_properties.cpp
    # HAL tests - USB
    hal/test_nx_usb.cpp
    hal/test_nx_usb_properties.cpp
    # HAL tests - Watchdog
    hal/test_nx_watchdog.cpp
    hal/test_nx_watchdog_properties.cpp
    # HAL tests - Option Bytes
    hal/test_nx_option_bytes.cpp
    hal/test_nx_option_bytes_properties.cpp
    # HAL tests - Integration
    # TODO: Update integration tests to match new HAL interfaces
    # hal/test_nx_integration.cpp
)

# Require C++20 for designated initializers
target_compile_features(nexus_tests PRIVATE cxx_std_20)

# Define test mode for startup framework tests
target_compile_definitions(nexus_tests PRIVATE NX_STARTUP_TEST_MODE)

# MSVC: Enable /FS flag to allow parallel compilation with shared PDB
if(MSVC)
    target_compile_options(nexus_tests PRIVATE /FS)
endif()

target_include_directories(nexus_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/hal/include
        ${CMAKE_SOURCE_DIR}/osal/include
        ${CMAKE_SOURCE_DIR}/platforms/native/include
        ${CMAKE_SOURCE_DIR}/platforms/stm32f4/include
        ${CMAKE_SOURCE_DIR}/framework/log/include
        ${CMAKE_SOURCE_DIR}/framework/shell/include
        ${CMAKE_SOURCE_DIR}/framework/config/include
        ${CMAKE_SOURCE_DIR}/framework/init/include
        ${CMAKE_SOURCE_DIR}
)

# Platform-specific threading libraries
if(WIN32)
    set(THREAD_LIBS "")
else()
    find_package(Threads REQUIRED)
    set(THREAD_LIBS Threads::Threads)
endif()

target_link_libraries(nexus_tests
    PRIVATE
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

# Discover tests
gtest_discover_tests(nexus_tests
    PROPERTIES
        TIMEOUT 300  # 5 minutes timeout per test
        LABELS "unit;property;integration"
)

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
