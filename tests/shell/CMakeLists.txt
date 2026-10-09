# The fake is a test fixture and is never part of a production Shell archive.
add_library(shell_test_support STATIC fixtures/shell_mock_backend.c)
target_include_directories(shell_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/include")
target_link_libraries(shell_test_support PUBLIC shell_interface)

##############################################################################
# Shell Framework Tests
##############################################################################

# Collect all test sources
set(SHELL_TEST_SOURCES
    # Unit tests
    test_shell_autocomplete.cpp
    test_shell_backend.cpp
    test_shell_builtin.cpp
    test_shell_command.cpp
    test_shell_core.cpp
    test_shell_history.cpp
    test_shell_line_editor.cpp
    test_shell_parser.cpp
    # Property-based tests
    test_shell_autocomplete_properties.cpp
    test_shell_backend_properties.cpp
    test_shell_command_properties.cpp
    test_shell_core_properties.cpp
    test_shell_history_properties.cpp
    test_shell_line_editor_properties.cpp
    test_shell_parser_properties.cpp
)

# Create test executable
add_executable(shell_tests
    ${NEXUS_SOURCE_DIR}/tests/test_main_common.cpp
    ${SHELL_TEST_SOURCES}
)

# C++20 for designated initializers
target_compile_features(shell_tests PRIVATE cxx_std_20)

# Test mode definition
target_compile_definitions(shell_tests PRIVATE NX_STARTUP_TEST_MODE)

# Compile options
target_compile_options(shell_tests PRIVATE ${NEXUS_TEST_COMPILE_OPTIONS})

# Include directories
target_include_directories(shell_tests PRIVATE ${NEXUS_TEST_INCLUDES})

# Link libraries
target_link_libraries(shell_tests PRIVATE ${NEXUS_TEST_LIBS} shell_test_support)

# Discover tests
nexus_discover_tests(shell_tests
    PROPERTIES
        TIMEOUT 300
        LABELS "shell;framework;unit;property"
)

add_executable(shell_uart_contract_tests test_uart_adapter_contracts.c
    "${NEXUS_SOURCE_DIR}/tests/log/fixtures/typed_uart_port.c")
target_include_directories(shell_uart_contract_tests PRIVATE "${NEXUS_SOURCE_DIR}/tests/log/fixtures")
target_link_libraries(shell_uart_contract_tests PRIVATE shell_uart_adapter osal nexus_arch)
if(MSVC)
    target_compile_options(shell_uart_contract_tests PRIVATE /UNDEBUG)
else()
    target_compile_options(shell_uart_contract_tests PRIVATE -UNDEBUG -Wall -Wextra -Werror)
endif()
add_test(NAME shell_uart_contract_tests COMMAND shell_uart_contract_tests)
set_tests_properties(shell_uart_contract_tests PROPERTIES TIMEOUT 10 LABELS "shell;uart;typed;contract")
