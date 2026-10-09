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
    ${NEXUS_SOURCE_DIR}/tests/test_main_common.cpp
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

if(UNIX)
    add_executable(osal_event_mask_tests test_event_mask_contract.c)
    target_link_libraries(osal_event_mask_tests PRIVATE osal)
    target_compile_options(osal_event_mask_tests PRIVATE -UNDEBUG -Werror)
    add_test(NAME osal_event_mask_tests COMMAND osal_event_mask_tests)
    set_tests_properties(osal_event_mask_tests PROPERTIES TIMEOUT 15 LABELS "osal;contract;capability")

    foreach(backend native baremetal)
        add_library(osal_${backend}_small_budget STATIC
            "${NEXUS_SOURCE_DIR}/osal/adapters/${backend}/osal_${backend}.c")
        target_link_libraries(osal_${backend}_small_budget PUBLIC osal_interface)
        target_compile_definitions(osal_${backend}_small_budget PUBLIC OSAL_LIFETIME_TOKEN_LIMIT=6)
        target_compile_options(osal_${backend}_small_budget PRIVATE -Werror)
        if(backend STREQUAL "native")
            target_link_libraries(osal_${backend}_small_budget PRIVATE Threads::Threads)
            target_compile_definitions(osal_${backend}_small_budget PUBLIC OSAL_STATS_ENABLE=0)
        else()
            target_link_libraries(osal_${backend}_small_budget PRIVATE nexus_arch)
        endif()
        add_executable(osal_${backend}_token_budget_tests test_token_budget_contract.c)
        target_link_libraries(osal_${backend}_token_budget_tests PRIVATE osal_${backend}_small_budget)
        target_compile_options(osal_${backend}_token_budget_tests PRIVATE -UNDEBUG -Werror)
        add_test(NAME osal_${backend}_token_budget_tests COMMAND osal_${backend}_token_budget_tests)
        set_tests_properties(osal_${backend}_token_budget_tests PROPERTIES
            TIMEOUT 15 LABELS "osal;contract;resource-budget")
    endforeach()

    add_executable(osal_contract_tests test_core_contracts.c)
    target_link_libraries(osal_contract_tests PRIVATE osal hal Threads::Threads)
    target_compile_options(osal_contract_tests PRIVATE -UNDEBUG)
    add_test(NAME osal_contract_tests COMMAND osal_contract_tests)
    set_tests_properties(osal_contract_tests PROPERTIES TIMEOUT 30 LABELS "osal;contract")

    add_executable(osal_task_mutex_quarantine_tests test_task_mutex_quarantine.c)
    target_link_libraries(osal_task_mutex_quarantine_tests PRIVATE osal)
    target_compile_options(osal_task_mutex_quarantine_tests PRIVATE -UNDEBUG -Werror)
    add_test(NAME osal_task_mutex_quarantine_tests COMMAND osal_task_mutex_quarantine_tests)
    set_tests_properties(osal_task_mutex_quarantine_tests PROPERTIES TIMEOUT 15 LABELS "osal;contract;fault")

    add_executable(baremetal_contract_tests
        test_baremetal_contracts.c
        ${NEXUS_SOURCE_DIR}/osal/adapters/baremetal/osal_baremetal.c)
    target_include_directories(baremetal_contract_tests PRIVATE ${NEXUS_SOURCE_DIR}/osal/include)
    target_link_libraries(baremetal_contract_tests PRIVATE nexus_arch_interface)
    target_compile_options(baremetal_contract_tests PRIVATE -UNDEBUG -Werror)
    add_test(NAME baremetal_contract_tests COMMAND baremetal_contract_tests)
    set_tests_properties(baremetal_contract_tests PROPERTIES TIMEOUT 30 LABELS "osal;baremetal;host-model;contract")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(freertos_runtime)
endif()
