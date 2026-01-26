##############################################################################
# Log Framework Tests
##############################################################################

# Collect all test sources
set(LOG_TEST_SOURCES
    # Unit tests
    test_log.cpp
    # Property-based tests
    test_log_properties.cpp
    # Integration tests
    test_log_integration.cpp
    # Performance tests
    test_log_performance.cpp
    # Thread safety tests
    test_log_thread_safety.cpp
)

# Create test executable
add_executable(log_tests
    ${CMAKE_SOURCE_DIR}/tests/test_main_common.cpp
    ${LOG_TEST_SOURCES}
)

# C++20 for designated initializers
target_compile_features(log_tests PRIVATE cxx_std_20)

# Test mode definition
target_compile_definitions(log_tests PRIVATE NX_STARTUP_TEST_MODE)

# Compile options
target_compile_options(log_tests PRIVATE ${NEXUS_TEST_COMPILE_OPTIONS})

# Include directories
target_include_directories(log_tests PRIVATE ${NEXUS_TEST_INCLUDES})

# Link libraries
target_link_libraries(log_tests PRIVATE ${NEXUS_TEST_LIBS})

# Discover tests
nexus_discover_tests(log_tests
    PROPERTIES
        TIMEOUT 300
        LABELS "log;framework;unit;property"
)
