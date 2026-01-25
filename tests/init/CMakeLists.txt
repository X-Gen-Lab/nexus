##############################################################################
# Init Framework Tests
##############################################################################

# Collect all test sources
set(INIT_TEST_SOURCES
    # Unit tests
    test_nx_firmware_info.cpp
    test_nx_init.cpp
    test_nx_startup.cpp
    # Property-based tests
    test_nx_init_properties.cpp
    # Integration tests
    test_init_integration.cpp
    # Performance tests
    test_init_performance.cpp
    # Test helpers
    test_init_helpers.cpp
)

# Create test executable
add_executable(init_tests
    ${CMAKE_SOURCE_DIR}/tests/test_main_common.cpp
    ${INIT_TEST_SOURCES}
)

# C++20 for designated initializers
target_compile_features(init_tests PRIVATE cxx_std_20)

# Test mode definition
target_compile_definitions(init_tests PRIVATE NX_STARTUP_TEST_MODE)

# Compile options
target_compile_options(init_tests PRIVATE ${NEXUS_TEST_COMPILE_OPTIONS})

# Include directories
target_include_directories(init_tests PRIVATE ${NEXUS_TEST_INCLUDES})

# Link libraries
target_link_libraries(init_tests PRIVATE ${NEXUS_TEST_LIBS})

# Discover tests
gtest_discover_tests(init_tests
    PROPERTIES
        TIMEOUT 300
        LABELS "init;framework;unit;property"
)
