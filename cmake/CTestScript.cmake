##############################################################################
# CTest Script for Nexus Project
# Usage: ctest -S cmake/CTestScript.cmake
##############################################################################

# Set the source and build directories
get_filename_component(CTEST_SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(CTEST_BINARY_DIRECTORY "${CTEST_SOURCE_DIRECTORY}/build")

# Set the build configuration
set(CTEST_BUILD_CONFIGURATION "Debug")
set(CTEST_CMAKE_GENERATOR "Ninja")

# Set the site name (hostname)
site_name(CTEST_SITE)

# Set the build name
set(CTEST_BUILD_NAME "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CTEST_BUILD_CONFIGURATION}")

# Configure CMake options
set(CTEST_CONFIGURE_COMMAND "${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${CTEST_BUILD_CONFIGURATION}")
set(CTEST_CONFIGURE_COMMAND "${CTEST_CONFIGURE_COMMAND} -DNEXUS_PLATFORM=native")
set(CTEST_CONFIGURE_COMMAND "${CTEST_CONFIGURE_COMMAND} -DNEXUS_BUILD_TESTS=ON")
set(CTEST_CONFIGURE_COMMAND "${CTEST_CONFIGURE_COMMAND} -DNEXUS_ENABLE_COVERAGE=ON")
set(CTEST_CONFIGURE_COMMAND "${CTEST_CONFIGURE_COMMAND} -G ${CTEST_CMAKE_GENERATOR}")
set(CTEST_CONFIGURE_COMMAND "${CTEST_CONFIGURE_COMMAND} ${CTEST_SOURCE_DIRECTORY}")

# Start the test process
ctest_start("Experimental")

# Configure the project
ctest_configure()

# Build the project
ctest_build()

# Run the tests
ctest_test()

# Generate coverage report (if coverage is enabled)
if(NEXUS_ENABLE_COVERAGE)
    ctest_coverage()
endif()

# Submit results (if CDash is configured)
# ctest_submit()
