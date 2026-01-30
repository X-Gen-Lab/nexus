##############################################################################
# Shell Framework Library
##############################################################################

#-----------------------------------------------------------------------------
# Shell Interface Library (Header-Only)
#-----------------------------------------------------------------------------

add_library(shell_interface INTERFACE)

target_include_directories(shell_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

#-----------------------------------------------------------------------------
# Shell Implementation Library
#-----------------------------------------------------------------------------

# Shell framework sources
set(SHELL_SOURCES
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

# Create shell framework static library
add_library(shell_framework STATIC ${SHELL_SOURCES})

# Include directories
target_include_directories(shell_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(shell_framework
    PUBLIC
        shell_interface
    PRIVATE
        osal_interface
        hal_interface
)

# Compile definitions
target_compile_definitions(shell_framework
    PRIVATE
        $<$<CONFIG:Debug>:SHELL_DEBUG_ENABLED>
)

# Compiler options
if(MSVC)
    target_compile_options(shell_framework PRIVATE /W4 /FS)
else()
    target_compile_options(shell_framework PRIVATE -Wall -Wextra)
endif()

message(STATUS "Shell Framework: Configured")

#-----------------------------------------------------------------------------
# End of Shell Framework Library
#-----------------------------------------------------------------------------
