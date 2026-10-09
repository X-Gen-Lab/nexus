##############################################################################
# Init Framework Tests
##############################################################################

# Collect all test sources
set(INIT_TEST_SOURCES
    # Unit tests
    test_nx_firmware_info.cpp
    test_nx_init.cpp
    # Property-based tests
    test_nx_init_properties.cpp
    # Integration tests
    test_init_integration.cpp
    # Test helpers
    test_init_helpers.cpp
)

# Create test executable
add_executable(init_tests
    ${NEXUS_SOURCE_DIR}/tests/test_main_common.cpp
    ${INIT_TEST_SOURCES}
)

# C++20 for designated initializers
target_compile_features(init_tests PRIVATE cxx_std_20)

# Test mode definition
target_compile_definitions(init_tests PRIVATE NX_INIT_TEST_MODE)

# Compile options
target_compile_options(init_tests PRIVATE ${NEXUS_TEST_COMPILE_OPTIONS})

# Include directories
target_include_directories(init_tests PRIVATE ${NEXUS_TEST_INCLUDES})

# Link libraries
target_link_libraries(init_tests PRIVATE ${NEXUS_TEST_LIBS})

# Discover tests
nexus_discover_tests(init_tests
    PROPERTIES
        TIMEOUT 300
        LABELS "init;framework;unit;property"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(init_registry_tests test_init_registry.cpp)
    target_link_libraries(init_registry_tests PRIVATE init_framework)
    target_compile_options(init_registry_tests PRIVATE -UNDEBUG)
    add_test(NAME init_registry_tests COMMAND init_registry_tests)
    set_tests_properties(init_registry_tests PROPERTIES LABELS "init;contract" TIMEOUT 10)
endif()
