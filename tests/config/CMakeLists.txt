##############################################################################
# Config Framework Tests
##############################################################################

# Collect all test sources
set(CONFIG_TEST_SOURCES
    # Unit tests
    test_config_backend.cpp
    test_config_callback.cpp
    test_config_crypto.cpp
    test_config_default.cpp
    test_config_import_export.cpp
    test_config_namespace.cpp
    test_config_query.cpp
    test_config_store.cpp
    # Integration tests
    test_config_integration.cpp
    # Performance tests
    test_config_performance.cpp
    # Thread safety tests
    test_config_thread_safety.cpp
    # Property-based tests
    test_config_backend_properties.cpp
    test_config_callback_properties.cpp
    test_config_core_properties.cpp
    test_config_crypto_properties.cpp
    test_config_default_properties.cpp
    test_config_import_export_properties.cpp
    test_config_namespace_properties.cpp
    test_config_store_properties.cpp
)

# Create test executable
add_executable(config_tests
    ${CMAKE_SOURCE_DIR}/tests/test_main_common.cpp
    ${CONFIG_TEST_SOURCES}
)

# C++20 for designated initializers
target_compile_features(config_tests PRIVATE cxx_std_20)

# Test mode definition
target_compile_definitions(config_tests PRIVATE NX_STARTUP_TEST_MODE)

# Compile options
target_compile_options(config_tests PRIVATE ${NEXUS_TEST_COMPILE_OPTIONS})

# Include directories
target_include_directories(config_tests PRIVATE ${NEXUS_TEST_INCLUDES})

# Link libraries
target_link_libraries(config_tests PRIVATE ${NEXUS_TEST_LIBS})

# Discover tests
gtest_discover_tests(config_tests
    PROPERTIES
        TIMEOUT 300
        LABELS "config;framework;unit;property"
)
