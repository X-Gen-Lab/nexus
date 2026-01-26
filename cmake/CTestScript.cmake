##############################################################################
# CTest Script for Nexus Project
#
# Usage:
#   ctest -S cmake/CTestScript.cmake
#   ctest -S cmake/CTestScript.cmake,Preset=linux-gcc-debug
#   ctest -S cmake/CTestScript.cmake,Coverage=ON
#
# Environment Variables:
#   CTEST_PRESET       - CMake preset to use (e.g., windows-msvc-debug)
#   CTEST_COVERAGE     - Enable coverage (ON/OFF)
#   CTEST_SUBMIT       - Submit to CDash (ON/OFF)
##############################################################################

# Parse script arguments
if(CTEST_SCRIPT_ARG)
    string(REPLACE "," ";" SCRIPT_ARGS "${CTEST_SCRIPT_ARG}")
    foreach(ARG ${SCRIPT_ARGS})
        if(ARG MATCHES "^Preset=(.+)$")
            set(CTEST_PRESET "${CMAKE_MATCH_1}")
        elseif(ARG MATCHES "^Coverage=(ON|OFF)$")
            set(CTEST_COVERAGE "${CMAKE_MATCH_1}")
        elseif(ARG MATCHES "^Submit=(ON|OFF)$")
            set(CTEST_SUBMIT "${CMAKE_MATCH_1}")
        endif()
    endforeach()
endif()

# Set defaults from environment or use fallback
if(NOT DEFINED CTEST_PRESET)
    if(DEFINED ENV{CTEST_PRESET})
        set(CTEST_PRESET "$ENV{CTEST_PRESET}")
    else()
        # Auto-detect preset based on platform
        if(WIN32)
            set(CTEST_PRESET "windows-msvc-debug")
        elseif(APPLE)
            set(CTEST_PRESET "macos-clang-debug")
        else()
            set(CTEST_PRESET "linux-gcc-debug")
        endif()
    endif()
endif()

if(NOT DEFINED CTEST_COVERAGE)
    if(DEFINED ENV{CTEST_COVERAGE})
        set(CTEST_COVERAGE "$ENV{CTEST_COVERAGE}")
    else()
        set(CTEST_COVERAGE "OFF")
    endif()
endif()

if(NOT DEFINED CTEST_SUBMIT)
    if(DEFINED ENV{CTEST_SUBMIT})
        set(CTEST_SUBMIT "$ENV{CTEST_SUBMIT}")
    else()
        set(CTEST_SUBMIT "OFF")
    endif()
endif()

# Set the source and build directories
get_filename_component(CTEST_SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(CTEST_BINARY_DIRECTORY "${CTEST_SOURCE_DIRECTORY}/build/${CTEST_PRESET}")

# Set the site name (hostname)
site_name(CTEST_SITE)

# Set the build name
set(CTEST_BUILD_NAME "${CTEST_PRESET}")

# Configure using CMake preset
set(CTEST_CONFIGURE_COMMAND "${CMAKE_COMMAND} --preset ${CTEST_PRESET}")

# Override coverage if requested
if(CTEST_COVERAGE)
    set(CTEST_CONFIGURE_COMMAND "${CTEST_CONFIGURE_COMMAND} -DNEXUS_ENABLE_COVERAGE=ON")
endif()

# Print configuration
message("=== CTest Configuration ===")
message("  Source Dir:  ${CTEST_SOURCE_DIRECTORY}")
message("  Build Dir:   ${CTEST_BINARY_DIRECTORY}")
message("  Preset:      ${CTEST_PRESET}")
message("  Coverage:    ${CTEST_COVERAGE}")
message("  Submit:      ${CTEST_SUBMIT}")
message("===========================")

# Start the test process
ctest_start("Experimental")

# Configure the project
ctest_configure(RETURN_VALUE CONFIGURE_RESULT)
if(NOT CONFIGURE_RESULT EQUAL 0)
    message(FATAL_ERROR "Configuration failed!")
endif()

# Build the project
ctest_build(RETURN_VALUE BUILD_RESULT)
if(NOT BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR "Build failed!")
endif()

# Run the tests
ctest_test(RETURN_VALUE TEST_RESULT)

# Generate coverage report (if coverage is enabled)
if(CTEST_COVERAGE)
    ctest_coverage()
endif()

# Submit results (if requested)
if(CTEST_SUBMIT)
    ctest_submit()
endif()

# Return test result
if(NOT TEST_RESULT EQUAL 0)
    message(WARNING "Some tests failed!")
endif()
