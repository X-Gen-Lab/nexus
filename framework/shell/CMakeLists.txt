##############################################################################
# Shell Framework Library
##############################################################################

# Shell interface library (header-only)
add_library(shell_interface INTERFACE)

target_include_directories(shell_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Shell implementation library
add_library(shell_framework STATIC)

target_sources(shell_framework
    PRIVATE
        src/shell.c
        src/shell_command.c
        src/shell_parser.c
        src/shell_line_editor.c
        src/shell_history.c
        src/shell_autocomplete.c
        src/shell_backend.c
        src/shell_uart_backend.c
        src/shell_mock_backend.c
        src/shell_builtin.c
)

target_link_libraries(shell_framework
    PUBLIC
        shell_interface
    PRIVATE
        osal_interface
        hal_interface
)

message(STATUS "Shell Framework: Enabled")
