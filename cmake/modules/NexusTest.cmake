##############################################################################
# Nexus Test Integration Module
# File: NexusTest.cmake
# Brief: Nexus Test Integration Module - Google Test, CTest, Coverage, Filtering
# Author: Nexus Team
##############################################################################
#
# This module provides comprehensive test integration for the Nexus build system:
# - Google Test framework integration
# - CTest parallel execution
# - Test report generation
# - Code coverage analysis
# - Test filtering by labels and names
#
# Requirements validated:
# - 10.1: Google Test integration
# - 10.3: Parallel test execution
# - 10.4: Test report generation
# - 10.5: Code coverage analysis
# - 10.7: Test filtering
#
##############################################################################

if(NEXUS_TEST_MODULE_INCLUDED)
    return()
endif()
set(NEXUS_TEST_MODULE_INCLUDED TRUE)

##############################################################################
# Module Configuration
##############################################################################

# Test framework options
option(NEXUS_TEST_ENABLE_GTEST "Enable Google Test framework" ON)
option(NEXUS_TEST_ENABLE_PARALLEL "Enable parallel test execution" ON)
option(NEXUS_TEST_ENABLE_COVERAGE "Enable code coverage analysis" OFF)
option(NEXUS_TEST_ENABLE_REPORTS "Enable test report generation" ON)
option(NEXUS_TEST_ENABLE_FILTERING "Enable test filtering" ON)

# Test configuration variables
set(NEXUS_TEST_PARALLEL_JOBS "AUTO" CACHE STRING "Number of parallel test jobs (AUTO = CPU count)")
set(NEXUS_TEST_TIMEOUT 300 CACHE STRING "Default test timeout in seconds")
set(NEXUS_TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/test_reports" CACHE PATH "Test report output directory")
set(NEXUS_TEST_COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage" CACHE PATH "Coverage report output directory")

# Test filtering options
set(NEXUS_TEST_FILTER_LABELS "" CACHE STRING "Comma-separated list of test labels to run")
set(NEXUS_TEST_FILTER_REGEX "" CACHE STRING "Regular expression to filter test names")
set(NEXUS_TEST_EXCLUDE_REGEX "" CACHE STRING "Regular expression to exclude test names")

##############################################################################
# Google Test Integration (Requirement 10.1)
##############################################################################

#
# Initialize Google Test framework
# Configures Google Test with FetchContent or finds installed version
#
function(nexus_test_init_gtest)
    if(NOT NEXUS_TEST_ENABLE_GTEST)
        message(STATUS "Google Test integration disabled")
        return()
    endif()

    # Check if Google Test is already available
    if(TARGET GTest::gtest)
        message(STATUS "Google Test already configured")
        return()
    endif()

    message(STATUS "Initializing Google Test framework...")

    # Try to find installed Google Test first
    find_package(GTest QUIET)

    if(GTest_FOUND)
        message(STATUS "Using installed Google Test: ${GTEST_VERSION}")
    else()
        # Fetch Google Test from GitHub
        message(STATUS "Fetching Google Test from GitHub...")
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0
            GIT_SHALLOW    TRUE
        )

        # Configure Google Test options
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

        # Make Google Test available
        FetchContent_MakeAvailable(googletest)

        # Create aliases for consistency
        if(NOT TARGET GTest::gtest)
            add_library(GTest::gtest ALIAS gtest)
        endif()
        if(NOT TARGET GTest::gtest_main)
            add_library(GTest::gtest_main ALIAS gtest_main)
        endif()
        if(NOT TARGET GTest::gmock)
            add_library(GTest::gmock ALIAS gmock)
        endif()
        if(NOT TARGET GTest::gmock_main)
            add_library(GTest::gmock_main ALIAS gmock_main)
        endif()

        message(STATUS "Google Test fetched successfully")
    endif()

    # Include GoogleTest module for test discovery
    include(GoogleTest)

    message(STATUS "Google Test framework initialized")
endfunction()

##############################################################################
# Test Discovery and Registration
##############################################################################

#
# Discover and register Google Test tests
# TARGET: Test executable target
# LABELS: Optional test labels
# TIMEOUT: Optional test timeout (overrides default)
# PROPERTIES: Optional CTest properties
#
function(nexus_test_discover TARGET)
    # Parse arguments
    set(options "")
    set(oneValueArgs TIMEOUT)
    set(multiValueArgs LABELS PROPERTIES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Set test timeout
    set(TEST_TIMEOUT ${NEXUS_TEST_TIMEOUT})
    if(ARG_TIMEOUT)
        set(TEST_TIMEOUT ${ARG_TIMEOUT})
    endif()

    # Build test properties list
    set(TEST_PROPERTIES
        TIMEOUT ${TEST_TIMEOUT}
    )

    # Add labels if provided
    if(ARG_LABELS)
        list(APPEND TEST_PROPERTIES LABELS "${ARG_LABELS}")
    endif()

    # Add custom properties
    if(ARG_PROPERTIES)
        list(APPEND TEST_PROPERTIES ${ARG_PROPERTIES})
    endif()

    # Discover tests using Google Test
    # Use PRE_TEST mode for Visual Studio to avoid path issues
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

##############################################################################
# Parallel Test Execution (Requirement 10.3)
##############################################################################

#
# Configure parallel test execution
# Sets up CTest to run tests in parallel based on CPU count
#
function(nexus_test_configure_parallel)
    if(NOT NEXUS_TEST_ENABLE_PARALLEL)
        message(STATUS "Parallel test execution disabled")
        set(CTEST_PARALLEL_LEVEL 1 PARENT_SCOPE)
        return()
    endif()

    # Determine parallel job count
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

    # Ensure at least 1 job
    if(PARALLEL_JOBS LESS 1)
        set(PARALLEL_JOBS 1)
    endif()

    # Set CTest parallel level
    set(CTEST_PARALLEL_LEVEL ${PARALLEL_JOBS} CACHE STRING "Number of parallel test jobs" FORCE)

    message(STATUS "Parallel test execution configured: ${PARALLEL_JOBS} jobs")
endfunction()

##############################################################################
# Test Report Generation (Requirement 10.4)
##############################################################################

#
# Generate test execution report
# Creates test summary with pass rate and execution time
# OUTPUT_FILE: Path to output report file
# FORMAT: Report format (TEXT, HTML, JSON)
#
function(nexus_test_generate_report)
    if(NOT NEXUS_TEST_ENABLE_REPORTS)
        return()
    endif()

    # Parse arguments
    set(options "")
    set(oneValueArgs OUTPUT_FILE FORMAT)
    set(multiValueArgs "")
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Set defaults
    if(NOT ARG_FORMAT)
        set(ARG_FORMAT "TEXT")
    endif()
    if(NOT ARG_OUTPUT_FILE)
        set(ARG_OUTPUT_FILE "${NEXUS_TEST_OUTPUT_DIR}/test_report.${ARG_FORMAT}")
    endif()

    # Create output directory
    file(MAKE_DIRECTORY "${NEXUS_TEST_OUTPUT_DIR}")

    # Generate report script
    set(REPORT_SCRIPT "${CMAKE_BINARY_DIR}/generate_test_report.cmake")
    file(WRITE "${REPORT_SCRIPT}" "
# Test Report Generation Script
cmake_minimum_required(VERSION 3.16)

# Run tests and capture results
execute_process(
    COMMAND \${CMAKE_CTEST_COMMAND} --output-on-failure --no-compress-output -T Test
    WORKING_DIRECTORY \${CMAKE_BINARY_DIR}
    RESULT_VARIABLE TEST_RESULT
)

# Parse test results from CTest XML
set(TEST_XML \"\${CMAKE_BINARY_DIR}/Testing/TAG\")
if(EXISTS \"\${TEST_XML}\")
    file(READ \"\${TEST_XML}\" TAG_CONTENT)
    string(REGEX MATCH \"[^\\n]+\" TEST_TAG \"\${TAG_CONTENT}\")
    set(TEST_RESULTS_XML \"\${CMAKE_BINARY_DIR}/Testing/\${TEST_TAG}/Test.xml\")

    if(EXISTS \"\${TEST_RESULTS_XML}\")
        file(READ \"\${TEST_RESULTS_XML}\" XML_CONTENT)

        # Extract test statistics
        string(REGEX MATCH \"<Test Status=\\\"passed\\\">([0-9]+)</Test>\" _ \"\${XML_CONTENT}\")
        set(TESTS_PASSED \"\${CMAKE_MATCH_1}\")
        string(REGEX MATCH \"<Test Status=\\\"failed\\\">([0-9]+)</Test>\" _ \"\${XML_CONTENT}\")
        set(TESTS_FAILED \"\${CMAKE_MATCH_1}\")

        if(NOT TESTS_PASSED)
            set(TESTS_PASSED 0)
        endif()
        if(NOT TESTS_FAILED)
            set(TESTS_FAILED 0)
        endif()

        math(EXPR TESTS_TOTAL \"\${TESTS_PASSED} + \${TESTS_FAILED}\")

        if(TESTS_TOTAL GREATER 0)
            math(EXPR PASS_RATE \"(\${TESTS_PASSED} * 100) / \${TESTS_TOTAL}\")
        else()
            set(PASS_RATE 0)
        endif()

        # Generate report
        set(REPORT_CONTENT \"Nexus Test Report\\n\")
        set(REPORT_CONTENT \"\${REPORT_CONTENT}==================\\n\\n\")
        set(REPORT_CONTENT \"\${REPORT_CONTENT}Total Tests: \${TESTS_TOTAL}\\n\")
        set(REPORT_CONTENT \"\${REPORT_CONTENT}Passed:      \${TESTS_PASSED}\\n\")
        set(REPORT_CONTENT \"\${REPORT_CONTENT}Failed:      \${TESTS_FAILED}\\n\")
        set(REPORT_CONTENT \"\${REPORT_CONTENT}Pass Rate:   \${PASS_RATE}%\\n\")

        file(WRITE \"${ARG_OUTPUT_FILE}\" \"\${REPORT_CONTENT}\")
        message(STATUS \"Test report generated: ${ARG_OUTPUT_FILE}\")
    endif()
endif()
")

    # Add custom target to generate report
    add_custom_target(test_report
        COMMAND ${CMAKE_COMMAND} -P "${REPORT_SCRIPT}"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating test report..."
    )

    message(STATUS "Test report generation configured: ${ARG_OUTPUT_FILE}")
endfunction()

##############################################################################
# Code Coverage Analysis (Requirement 10.5)
##############################################################################

#
# Configure code coverage analysis
# Sets up compiler flags and coverage report generation
#
function(nexus_test_configure_coverage)
    if(NOT NEXUS_TEST_ENABLE_COVERAGE)
        message(STATUS "Code coverage analysis disabled")
        return()
    endif()

    # Check compiler support
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Configuring code coverage for ${CMAKE_CXX_COMPILER_ID}...")

        # Add coverage compiler flags
        set(COVERAGE_FLAGS "--coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_FLAGS}" PARENT_SCOPE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS}" PARENT_SCOPE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${COVERAGE_FLAGS}" PARENT_SCOPE)

        # Create coverage output directory
        file(MAKE_DIRECTORY "${NEXUS_TEST_COVERAGE_DIR}")

        # Generate coverage report script
        set(COVERAGE_SCRIPT "${CMAKE_BINARY_DIR}/generate_coverage.cmake")
        file(WRITE "${COVERAGE_SCRIPT}" "
# Coverage Report Generation Script
cmake_minimum_required(VERSION 3.16)

# Run tests first
execute_process(
    COMMAND \${CMAKE_CTEST_COMMAND} --output-on-failure
    WORKING_DIRECTORY \${CMAKE_BINARY_DIR}
)

# Find gcov or llvm-cov
find_program(GCOV_PATH gcov)
find_program(LLVM_COV_PATH llvm-cov)

if(GCOV_PATH)
    set(COV_TOOL \${GCOV_PATH})
elseif(LLVM_COV_PATH)
    set(COV_TOOL \${LLVM_COV_PATH} gcov)
else()
    message(FATAL_ERROR \"Coverage tool not found (gcov or llvm-cov)\")
endif()

# Find lcov for HTML report generation
find_program(LCOV_PATH lcov)
find_program(GENHTML_PATH genhtml)

if(LCOV_PATH AND GENHTML_PATH)
    # Generate coverage data
    execute_process(
        COMMAND \${LCOV_PATH} --capture --directory . --output-file coverage.info
        WORKING_DIRECTORY \${CMAKE_BINARY_DIR}
    )

    # Filter out system and test files
    execute_process(
        COMMAND \${LCOV_PATH} --remove coverage.info '/usr/*' '*/tests/*' '*/ext/*' --output-file coverage_filtered.info
        WORKING_DIRECTORY \${CMAKE_BINARY_DIR}
    )

    # Generate HTML report
    execute_process(
        COMMAND \${GENHTML_PATH} coverage_filtered.info --output-directory ${NEXUS_TEST_COVERAGE_DIR}
        WORKING_DIRECTORY \${CMAKE_BINARY_DIR}
    )

    message(STATUS \"Coverage report generated: ${NEXUS_TEST_COVERAGE_DIR}/index.html\")
else()
    message(WARNING \"lcov/genhtml not found, HTML coverage report not generated\")
endif()
")

        # Add custom target for coverage
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -P "${COVERAGE_SCRIPT}"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report..."
        )

        message(STATUS "Code coverage configured: ${NEXUS_TEST_COVERAGE_DIR}")
    else()
        message(WARNING "Code coverage not supported for compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endfunction()

##############################################################################
# Test Filtering (Requirement 10.7)
##############################################################################

#
# Configure test filtering
# Sets up CTest filters for labels and name patterns
#
function(nexus_test_configure_filtering)
    if(NOT NEXUS_TEST_ENABLE_FILTERING)
        return()
    endif()

    set(FILTER_ARGS "")

    # Filter by labels
    if(NEXUS_TEST_FILTER_LABELS)
        string(REPLACE "," ";" LABEL_LIST "${NEXUS_TEST_FILTER_LABELS}")
        foreach(LABEL ${LABEL_LIST})
            list(APPEND FILTER_ARGS "-L" "${LABEL}")
        endforeach()
        message(STATUS "Test filtering by labels: ${NEXUS_TEST_FILTER_LABELS}")
    endif()

    # Filter by name regex (include)
    if(NEXUS_TEST_FILTER_REGEX)
        list(APPEND FILTER_ARGS "-R" "${NEXUS_TEST_FILTER_REGEX}")
        message(STATUS "Test filtering by regex: ${NEXUS_TEST_FILTER_REGEX}")
    endif()

    # Filter by name regex (exclude)
    if(NEXUS_TEST_EXCLUDE_REGEX)
        list(APPEND FILTER_ARGS "-E" "${NEXUS_TEST_EXCLUDE_REGEX}")
        message(STATUS "Test exclusion by regex: ${NEXUS_TEST_EXCLUDE_REGEX}")
    endif()

    # Store filter arguments for use in test commands
    if(FILTER_ARGS)
        set(NEXUS_TEST_FILTER_ARGS "${FILTER_ARGS}" PARENT_SCOPE)
    endif()
endfunction()

##############################################################################
# Test Isolation
##############################################################################

#
# Ensure test isolation for parallel execution
# Configures tests to run in isolated environments
# TARGET: Test target to isolate
#
function(nexus_test_ensure_isolation TARGET)
    # Set test properties for isolation
    set_tests_properties(${TARGET} PROPERTIES
        RUN_SERIAL FALSE  # Allow parallel execution
        RESOURCE_LOCK ""  # No resource locks by default
    )

    # Add environment isolation
    set_tests_properties(${TARGET} PROPERTIES
        ENVIRONMENT "NEXUS_TEST_ISOLATED=1"
    )
endfunction()

##############################################################################
# Main Initialization Function
##############################################################################

#
# Initialize Nexus test integration
# Sets up all test framework components
#
function(nexus_test_init)
    message(STATUS "")
    message(STATUS "=== Nexus Test Integration ===")

    # Initialize Google Test
    nexus_test_init_gtest()

    # Configure parallel execution
    nexus_test_configure_parallel()

    # Configure coverage if enabled
    nexus_test_configure_coverage()

    # Configure test filtering
    nexus_test_configure_filtering()

    # Configure test report generation
    nexus_test_generate_report()

    # Set CTest options
    set(CTEST_OUTPUT_ON_FAILURE ON CACHE BOOL "Show test output on failure" FORCE)

    message(STATUS "  Google Test:    ${NEXUS_TEST_ENABLE_GTEST}")
    message(STATUS "  Parallel:       ${NEXUS_TEST_ENABLE_PARALLEL} (${CTEST_PARALLEL_LEVEL} jobs)")
    message(STATUS "  Coverage:       ${NEXUS_TEST_ENABLE_COVERAGE}")
    message(STATUS "  Reports:        ${NEXUS_TEST_ENABLE_REPORTS}")
    message(STATUS "  Filtering:      ${NEXUS_TEST_ENABLE_FILTERING}")
    message(STATUS "  Timeout:        ${NEXUS_TEST_TIMEOUT}s")
    message(STATUS "==============================")
    message(STATUS "")
endfunction()

##############################################################################
# Module Loaded
##############################################################################

message(STATUS "NexusTest module loaded")
