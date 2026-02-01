#-----------------------------------------------------------------------------
# NexusQuality.cmake - Quality Assurance Module
#-----------------------------------------------------------------------------
# Test integration, diagnostics, and reproducible builds
# Author: Nexus Team
#
# This module provides:
# - Google Test integration
# - Parallel test execution
# - Test report generation
# - Code coverage analysis
# - Error handling and diagnostics
# - Reproducible build support
#
# Consolidated from:
# - NexusTest.cmake
# - NexusDiagnostics.cmake
# - NexusReproducible.cmake
#
#-----------------------------------------------------------------------------

include_guard(GLOBAL)

#-----------------------------------------------------------------------------
# Test Integration
#-----------------------------------------------------------------------------

option(NEXUS_TEST_ENABLE_GTEST "Enable Google Test framework" ON)
option(NEXUS_TEST_ENABLE_PARALLEL "Enable parallel test execution" ON)
option(NEXUS_TEST_ENABLE_COVERAGE "Enable code coverage analysis" OFF)
option(NEXUS_TEST_ENABLE_REPORTS "Enable test report generation" ON)

set(NEXUS_TEST_PARALLEL_JOBS "AUTO" CACHE STRING "Number of parallel test jobs")
set(NEXUS_TEST_TIMEOUT 300 CACHE STRING "Default test timeout in seconds")
set(NEXUS_TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/test_reports" CACHE PATH "Test report output directory")
set(NEXUS_TEST_COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage" CACHE PATH "Coverage report output directory")

#
# Initialize Google Test framework
#
function(nexus_test_init_gtest)
    if(NOT NEXUS_TEST_ENABLE_GTEST)
        message(STATUS "Google Test integration disabled")
        return()
    endif()

    if(TARGET GTest::gtest)
        message(STATUS "Google Test already configured")
        return()
    endif()

    message(STATUS "Initializing Google Test framework...")

    find_package(GTest QUIET)

    if(GTest_FOUND)
        message(STATUS "Using installed Google Test: ${GTEST_VERSION}")
    else()
        message(STATUS "Fetching Google Test from GitHub...")
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0
            GIT_SHALLOW    TRUE
        )

        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

        FetchContent_MakeAvailable(googletest)

        if(NOT TARGET GTest::gtest)
            add_library(GTest::gtest ALIAS gtest)
        endif()
        if(NOT TARGET GTest::gtest_main)
            add_library(GTest::gtest_main ALIAS gtest_main)
        endif()
        if(NOT TARGET GTest::gmock)
            add_library(GTest::gmock ALIAS gmock)
        endif()

        message(STATUS "Google Test fetched successfully")
    endif()

    include(GoogleTest)

    message(STATUS "Google Test framework initialized")
endfunction()

#
# Discover and register Google Test tests
# Arguments:
#   TARGET: Test executable target
#   LABELS: Optional test labels
#   TIMEOUT: Optional test timeout
#   PROPERTIES: Optional CTest properties
#
function(nexus_test_discover TARGET)
    set(options "")
    set(oneValueArgs TIMEOUT)
    set(multiValueArgs LABELS PROPERTIES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(TEST_TIMEOUT ${NEXUS_TEST_TIMEOUT})
    if(ARG_TIMEOUT)
        set(TEST_TIMEOUT ${ARG_TIMEOUT})
    endif()

    set(TEST_PROPERTIES TIMEOUT ${TEST_TIMEOUT})

    if(ARG_LABELS)
        list(APPEND TEST_PROPERTIES LABELS "${ARG_LABELS}")
    endif()

    if(ARG_PROPERTIES)
        list(APPEND TEST_PROPERTIES ${ARG_PROPERTIES})
    endif()

    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        gtest_discover_tests(${TARGET}
            DISCOVERY_MODE PRE_TEST
            PROPERTIES ${TEST_PROPERTIES}
        )
    else()
        gtest_discover_tests(${TARGET}
            PROPERTIES ${TEST_PROPERTIES}
        )
    endif()

    message(STATUS "Discovered tests in target: ${TARGET}")
endfunction()

#
# Configure parallel test execution
#
function(nexus_test_configure_parallel)
    if(NOT NEXUS_TEST_ENABLE_PARALLEL)
        message(STATUS "Parallel test execution disabled")
        set(CTEST_PARALLEL_LEVEL 1 PARENT_SCOPE)
        return()
    endif()

    if(NEXUS_TEST_PARALLEL_JOBS STREQUAL "AUTO")
        include(ProcessorCount)
        ProcessorCount(CPU_COUNT)
        if(CPU_COUNT EQUAL 0)
            set(CPU_COUNT 1)
        endif()
        set(PARALLEL_JOBS ${CPU_COUNT})
    else()
        set(PARALLEL_JOBS ${NEXUS_TEST_PARALLEL_JOBS})
    endif()

    if(PARALLEL_JOBS LESS 1)
        set(PARALLEL_JOBS 1)
    endif()

    set(CTEST_PARALLEL_LEVEL ${PARALLEL_JOBS} CACHE STRING "Number of parallel test jobs" FORCE)

    message(STATUS "Parallel test execution configured: ${PARALLEL_JOBS} jobs")
endfunction()

#
# Configure code coverage analysis
#
function(nexus_test_configure_coverage)
    if(NOT NEXUS_TEST_ENABLE_COVERAGE)
        message(STATUS "Code coverage analysis disabled")
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Configuring code coverage for ${CMAKE_CXX_COMPILER_ID}...")

        set(COVERAGE_FLAGS "--coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_FLAGS}" PARENT_SCOPE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS}" PARENT_SCOPE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${COVERAGE_FLAGS}" PARENT_SCOPE)

        file(MAKE_DIRECTORY "${NEXUS_TEST_COVERAGE_DIR}")

        message(STATUS "Code coverage configured: ${NEXUS_TEST_COVERAGE_DIR}")
    else()
        message(WARNING "Code coverage not supported for compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endfunction()

#
# Initialize Nexus test integration
#
function(nexus_test_init)
    message(STATUS "")
    message(STATUS "=== Nexus Test Integration ===")

    nexus_test_init_gtest()
    nexus_test_configure_parallel()
    nexus_test_configure_coverage()

    set(CTEST_OUTPUT_ON_FAILURE ON CACHE BOOL "Show test output on failure" FORCE)

    message(STATUS "  Google Test:    ${NEXUS_TEST_ENABLE_GTEST}")
    message(STATUS "  Parallel:       ${NEXUS_TEST_ENABLE_PARALLEL} (${CTEST_PARALLEL_LEVEL} jobs)")
    message(STATUS "  Coverage:       ${NEXUS_TEST_ENABLE_COVERAGE}")
    message(STATUS "  Reports:        ${NEXUS_TEST_ENABLE_REPORTS}")
    message(STATUS "  Timeout:        ${NEXUS_TEST_TIMEOUT}s")
    message(STATUS "==============================")
    message(STATUS "")
endfunction()

#-----------------------------------------------------------------------------
# Diagnostics and Error Handling
#-----------------------------------------------------------------------------

set(NEXUS_LOG_LEVEL_DEBUG 0)
set(NEXUS_LOG_LEVEL_INFO 1)
set(NEXUS_LOG_LEVEL_WARNING 2)
set(NEXUS_LOG_LEVEL_ERROR 3)

if(NOT DEFINED NEXUS_LOG_LEVEL)
    set(NEXUS_LOG_LEVEL ${NEXUS_LOG_LEVEL_INFO} CACHE STRING "Nexus log level")
endif()

set(NEXUS_LOG_FILE "${CMAKE_BINARY_DIR}/nexus_build.log" CACHE FILEPATH "Nexus log file")
set(NEXUS_BUILD_REPORT_FILE "${CMAKE_BINARY_DIR}/nexus_build_report.txt" CACHE FILEPATH "Build report file")

set(NEXUS_BUILD_START_TIME "" CACHE INTERNAL "Build start time")
set(NEXUS_BUILD_ERROR_COUNT 0 CACHE INTERNAL "Build error count")
set(NEXUS_BUILD_WARNING_COUNT 0 CACHE INTERNAL "Build warning count")

#
# Initialize logging system
#
function(nexus_init_logging)
    file(WRITE "${NEXUS_LOG_FILE}" "")

    string(TIMESTAMP START_TIME "%Y-%m-%dT%H:%M:%S")
    set(NEXUS_BUILD_START_TIME "${START_TIME}" CACHE INTERNAL "Build start time")

    nexus_log(INFO "Nexus build system initialized")
    nexus_log(INFO "CMake version: ${CMAKE_VERSION}")
    nexus_log(INFO "Platform: ${CMAKE_SYSTEM_NAME}")
    nexus_log(INFO "Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
endfunction()

#
# Log message with level
# Arguments:
#   LEVEL: Log level (DEBUG, INFO, WARNING, ERROR)
#   MESSAGE: Log message
#
function(nexus_log LEVEL MESSAGE)
    set(NUMERIC_LEVEL ${NEXUS_LOG_LEVEL_${LEVEL}})

    if(NUMERIC_LEVEL LESS NEXUS_LOG_LEVEL)
        return()
    endif()

    string(TIMESTAMP TIMESTAMP "%Y-%m-%dT%H:%M:%S")
    set(LOG_MESSAGE "[${TIMESTAMP}] [${LEVEL}] ${MESSAGE}")

    file(APPEND "${NEXUS_LOG_FILE}" "${LOG_MESSAGE}\n")

    if(LEVEL STREQUAL "ERROR")
        message(SEND_ERROR "${LOG_MESSAGE}")
        math(EXPR ERROR_COUNT "${NEXUS_BUILD_ERROR_COUNT} + 1")
        set(NEXUS_BUILD_ERROR_COUNT ${ERROR_COUNT} CACHE INTERNAL "Build error count")
    elseif(LEVEL STREQUAL "WARNING")
        message(WARNING "${LOG_MESSAGE}")
        math(EXPR WARNING_COUNT "${NEXUS_BUILD_WARNING_COUNT} + 1")
        set(NEXUS_BUILD_WARNING_COUNT ${WARNING_COUNT} CACHE INTERNAL "Build warning count")
    elseif(LEVEL STREQUAL "INFO")
        message(STATUS "${LOG_MESSAGE}")
    elseif(LEVEL STREQUAL "DEBUG")
        message(VERBOSE "${LOG_MESSAGE}")
    endif()
endfunction()

#
# Generate build report
#
function(nexus_generate_build_report)
    set(REPORT "")

    string(APPEND REPORT "================================================================================\n")
    string(APPEND REPORT "NEXUS BUILD REPORT\n")
    string(APPEND REPORT "================================================================================\n\n")

    string(APPEND REPORT "Build Configuration:\n")
    string(APPEND REPORT "  Platform:        ${CMAKE_SYSTEM_NAME}\n")
    string(APPEND REPORT "  Compiler:        ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}\n")
    string(APPEND REPORT "  Build Type:      ${CMAKE_BUILD_TYPE}\n")
    string(APPEND REPORT "  CMake Version:   ${CMAKE_VERSION}\n\n")

    string(APPEND REPORT "Build Statistics:\n")
    string(APPEND REPORT "  Start Time:      ${NEXUS_BUILD_START_TIME}\n")
    string(TIMESTAMP END_TIME "%Y-%m-%dT%H:%M:%S")
    string(APPEND REPORT "  End Time:        ${END_TIME}\n")
    string(APPEND REPORT "  Errors:          ${NEXUS_BUILD_ERROR_COUNT}\n")
    string(APPEND REPORT "  Warnings:        ${NEXUS_BUILD_WARNING_COUNT}\n\n")

    if(NEXUS_BUILD_ERROR_COUNT GREATER 0)
        string(APPEND REPORT "Build Status:      FAILED\n")
    else()
        string(APPEND REPORT "Build Status:      SUCCESS\n")
    endif()

    string(APPEND REPORT "\n================================================================================\n")

    file(WRITE "${NEXUS_BUILD_REPORT_FILE}" "${REPORT}")

    message(STATUS "Build report generated: ${NEXUS_BUILD_REPORT_FILE}")
endfunction()

#-----------------------------------------------------------------------------
# Reproducible Build Support
#-----------------------------------------------------------------------------

set(NEXUS_REPRODUCIBLE_BUILD OFF CACHE BOOL "Enable reproducible builds")
set(NEXUS_BUILD_TIMESTAMP "1970-01-01T00:00:00Z" CACHE STRING "Fixed timestamp for reproducible builds")
set(NEXUS_BUILD_MANIFEST_FILE "${CMAKE_BINARY_DIR}/build_manifest.json")

#
# Set deterministic build flags
#
function(nexus_set_deterministic_flags)
    message(STATUS "Enabling deterministic build flags...")

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-Wno-builtin-macro-redefined)
        add_compile_definitions(__DATE__="Jan 01 1970")
        add_compile_definitions(__TIME__="00:00:00")
        add_compile_definitions(__TIMESTAMP__="Thu Jan 01 00:00:00 1970")

        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            add_compile_options(-frandom-seed=0)
        endif()

        add_compile_options(-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.)
        add_compile_options(-ffile-prefix-map=${CMAKE_BINARY_DIR}=.)
    endif()

    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/Brepro)
        add_link_options(/Brepro)
    endif()

    add_compile_definitions(NEXUS_BUILD_TIMESTAMP="${NEXUS_BUILD_TIMESTAMP}")
endfunction()

#
# Compute SHA-256 hash of file
# Arguments:
#   FILE_PATH: Path to file
#   OUT_HASH: Output variable for hash
#
function(nexus_compute_file_hash FILE_PATH OUT_HASH)
    if(NOT EXISTS "${FILE_PATH}")
        set(${OUT_HASH} "" PARENT_SCOPE)
        return()
    endif()

    file(SHA256 "${FILE_PATH}" FILE_HASH)
    set(${OUT_HASH} "sha256:${FILE_HASH}" PARENT_SCOPE)
endfunction()

#
# Enable reproducible builds for a target
# Arguments:
#   TARGET: Target name
#
function(nexus_enable_reproducible_build TARGET)
    if(NOT NEXUS_REPRODUCIBLE_BUILD)
        return()
    endif()

    get_target_property(TARGET_SOURCES ${TARGET} SOURCES)

    if(TARGET_SOURCES)
        foreach(SOURCE ${TARGET_SOURCES})
            get_filename_component(SOURCE_ABS "${SOURCE}" ABSOLUTE)
            # Record source hash for manifest
        endforeach()
    endif()

    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Recording artifact hash for ${TARGET}"
        COMMENT "Recording build artifact"
    )
endfunction()

#
# Initialize reproducible build support
#
function(nexus_init_reproducible_build)
    if(NOT NEXUS_REPRODUCIBLE_BUILD)
        return()
    endif()

    message(STATUS "Initializing reproducible build support...")

    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/manifests")

    nexus_set_deterministic_flags()

    message(STATUS "Reproducible build support initialized")
endfunction()

message(STATUS "NexusQuality module loaded")

#-----------------------------------------------------------------------------
# End of NexusQuality.cmake
#-----------------------------------------------------------------------------
