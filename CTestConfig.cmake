##############################################################################
# CTest Configuration for Nexus Project
##############################################################################

# Project information
set(CTEST_PROJECT_NAME "Nexus")
set(CTEST_NIGHTLY_START_TIME "00:00:00 UTC")

# Drop site configuration (for CDash integration if needed in future)
set(CTEST_DROP_METHOD "http")
set(CTEST_DROP_SITE "my.cdash.org")
set(CTEST_DROP_LOCATION "/submit.php?project=Nexus")
set(CTEST_DROP_SITE_CDASH TRUE)

# Test configuration
set(CTEST_CUSTOM_MAXIMUM_NUMBER_OF_ERRORS 50)
set(CTEST_CUSTOM_MAXIMUM_NUMBER_OF_WARNINGS 50)
set(CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE 51200)
set(CTEST_CUSTOM_MAXIMUM_FAILED_TEST_OUTPUT_SIZE 102400)

# Coverage configuration
set(CTEST_COVERAGE_COMMAND "gcov")
set(CTEST_COVERAGE_EXTRA_FLAGS "-l -p")

# Memory check configuration (for valgrind if available)
set(CTEST_MEMORYCHECK_COMMAND "valgrind")
set(CTEST_MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full")
set(CTEST_MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_SOURCE_DIR}/tests/valgrind.supp")
