##############################################################################
# Integration Tests
##############################################################################

# Collect all test sources
set(INTEGRATION_TEST_SOURCES
    test_config_integration.cpp
    test_freertos_adapter_integration.cpp
    test_shell_integration.cpp
)

# Create test executable
add_executable(integration_tests
    ${CMAKE_SOURCE_DIR}/tests/test_main_common.cpp
    ${INTEGRATION_TEST_SOURCES}
)

# C++20 for designated initializers
target_compile_features(integration_tests PRIVATE cxx_std_20)

# Test mode definition
target_compile_definitions(integration_tests PRIVATE NX_STARTUP_TEST_MODE)

# Compile options
target_compile_options(integration_tests PRIVATE ${NEXUS_TEST_COMPILE_OPTIONS})

# Include directories
target_include_directories(integration_tests PRIVATE ${NEXUS_TEST_INCLUDES})

# Link libraries
target_link_libraries(integration_tests PRIVATE ${NEXUS_TEST_LIBS})

# Discover tests
nexus_discover_tests(integration_tests
    PROPERTIES
        TIMEOUT 300
        LABELS "integration"
)
