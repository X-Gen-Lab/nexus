##############################################################################
# Log Framework Library
##############################################################################

# Log interface library (header-only)
add_library(log_interface INTERFACE)

target_include_directories(log_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Log implementation library
add_library(log_framework STATIC)

target_sources(log_framework
    PRIVATE
        src/log.c
        src/log_backend_uart.c
)

target_link_libraries(log_framework
    PUBLIC
        log_interface
    PRIVATE
        osal_interface
        hal_interface
)

message(STATUS "Log Framework: Enabled")
