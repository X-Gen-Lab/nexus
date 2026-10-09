add_library(shell_interface INTERFACE)
target_include_directories(shell_interface INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
add_library(shell_framework STATIC
    src/shell.c src/shell_command.c src/shell_parser.c src/shell_line_editor.c
    src/shell_history.c src/shell_autocomplete.c src/shell_backend.c src/shell_builtin.c)
target_include_directories(shell_framework PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(shell_framework PUBLIC shell_interface nexus_build_options)
target_compile_definitions(shell_framework PRIVATE $<$<CONFIG:Debug>:SHELL_DEBUG_ENABLED>)
# Device/OS integration is an explicit final-application choice.
add_library(shell_uart_adapter STATIC src/shell_uart_backend.c)
target_link_libraries(shell_uart_adapter PUBLIC shell_interface Nexus::HALDevice PRIVATE osal)
