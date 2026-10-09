##############################################################################
# Log Framework Tests
##############################################################################

# Collect all test sources
set(LOG_TEST_SOURCES
    fixtures/level_abi.c
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
    ${NEXUS_SOURCE_DIR}/tests/test_main_common.cpp
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

if(UNIX)
    add_executable(log_contract_tests test_log_contracts.c)
    target_link_libraries(log_contract_tests PRIVATE log_framework osal Threads::Threads)
    target_compile_options(log_contract_tests PRIVATE -UNDEBUG -Werror)
    add_test(NAME log_contract_tests COMMAND log_contract_tests)
    set_tests_properties(log_contract_tests PROPERTIES TIMEOUT 30 LABELS "log;concurrency;contract")
endif()

# The formatter links as a core-only component with no runtime or HAL.
add_executable(log_format_core_contract_tests test_format_core_contracts.c)
target_link_libraries(log_format_core_contract_tests PRIVATE log_core)
if(MSVC)
    target_compile_options(log_format_core_contract_tests PRIVATE /UNDEBUG)
else()
    target_compile_options(log_format_core_contract_tests PRIVATE -UNDEBUG -Wall -Wextra -Werror)
endif()
add_test(NAME log_format_core_contract_tests COMMAND log_format_core_contract_tests)
set_tests_properties(log_format_core_contract_tests PROPERTIES TIMEOUT 10 LABELS "log;core;contract")

# Compile both allocation modes against the production facade and a hardware
# ownership fault fixture; this does not imply a Native UART provider or HIL.
foreach(mode 0 1)
    add_executable(log_uart_contract_${mode}
        test_uart_adapter_contracts.c fixtures/typed_uart_port.c
        "${NEXUS_SOURCE_DIR}/framework/log/src/log.c"
        "${NEXUS_SOURCE_DIR}/framework/log/src/log_format.c"
        "${NEXUS_SOURCE_DIR}/framework/log/src/log_backend_uart.c")
    target_compile_definitions(log_uart_contract_${mode} PRIVATE LOG_USE_STATIC_ALLOC=${mode})
    target_link_libraries(log_uart_contract_${mode} PRIVATE log_interface Nexus::HALDevice osal nexus_arch)
    if(MSVC)
        target_compile_options(log_uart_contract_${mode} PRIVATE /UNDEBUG)
    else()
        target_compile_options(log_uart_contract_${mode} PRIVATE -UNDEBUG -Wall -Wextra -Werror)
    endif()
    if(UNIX AND NOT APPLE)
        target_compile_definitions(log_uart_contract_${mode} PRIVATE LOG_TEST_WRAP_ALLOC)
        target_link_options(log_uart_contract_${mode} PRIVATE -Wl,--wrap=malloc)
    endif()
    add_test(NAME log_uart_contract_${mode} COMMAND log_uart_contract_${mode})
    set_tests_properties(log_uart_contract_${mode} PROPERTIES TIMEOUT 10 LABELS "log;uart;typed;contract")
endforeach()
