##############################################################################
# OSAL Tests
##############################################################################

# Collect all test sources
set(OSAL_TEST_SOURCES
    # Unit tests
    test_osal_event.cpp
    test_osal_mem.cpp
    test_osal_mutex.cpp
    test_osal_queue.cpp
    test_osal_sem.cpp
    test_osal_task.cpp
    test_osal_timer.cpp
    # Property-based tests
    test_osal_core_properties.cpp
    test_osal_diag_properties.cpp
    test_osal_error_handling_properties.cpp
    test_osal_event_properties.cpp
    test_osal_handle_validation_properties.cpp
    test_osal_mem_properties.cpp
    test_osal_mutex_properties.cpp
    test_osal_queue_properties.cpp
    test_osal_sem_properties.cpp
    test_osal_task_properties.cpp
    test_osal_timer_properties.cpp
)

# Create test executable
add_executable(osal_tests
    ${CMAKE_SOURCE_DIR}/tests/test_main_common.cpp
    ${OSAL_TEST_SOURCES}
)

# C++20 for designated initializers
target_compile_features(osal_tests PRIVATE cxx_std_20)

# Test mode definition
target_compile_definitions(osal_tests PRIVATE NX_STARTUP_TEST_MODE)

# Compile options
target_compile_options(osal_tests PRIVATE ${NEXUS_TEST_COMPILE_OPTIONS})

# Include directories
target_include_directories(osal_tests PRIVATE ${NEXUS_TEST_INCLUDES})

# Link libraries
target_link_libraries(osal_tests PRIVATE ${NEXUS_TEST_LIBS})

# Discover tests
nexus_discover_tests(osal_tests
    PROPERTIES
        TIMEOUT 300
        LABELS "osal;unit;property"
)
