##############################################################################
# Log Framework Library
##############################################################################

#-----------------------------------------------------------------------------
# Log Interface Library (Header-Only)
#-----------------------------------------------------------------------------

add_library(log_interface INTERFACE)

target_include_directories(log_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

#-----------------------------------------------------------------------------
# Log Implementation Library
#-----------------------------------------------------------------------------

# Log framework sources
set(LOG_SOURCES
    src/log.c
    src/log_backend_uart.c
)

# Create log framework static library
add_library(log_framework STATIC ${LOG_SOURCES})

# Include directories
target_include_directories(log_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(log_framework
    PUBLIC
        log_interface
    PRIVATE
        osal_interface
        hal_interface
)

# Compile definitions
target_compile_definitions(log_framework
    PRIVATE
        $<$<CONFIG:Debug>:LOG_DEBUG_ENABLED>
)

# MSVC parallel compilation fix
if(MSVC)
    target_compile_options(log_framework PRIVATE /FS)
endif()

message(STATUS "Log Framework: Configured")

#-----------------------------------------------------------------------------
# End of Log Framework Library
#-----------------------------------------------------------------------------
