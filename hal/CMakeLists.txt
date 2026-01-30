##############################################################################
# HAL Library
##############################################################################

#-----------------------------------------------------------------------------
# HAL Interface Library (Header-Only)
#-----------------------------------------------------------------------------

add_library(hal_interface INTERFACE)

target_include_directories(hal_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}
)

#-----------------------------------------------------------------------------
# HAL Implementation Library
#-----------------------------------------------------------------------------

# Core HAL sources
set(HAL_CORE_SOURCES
    src/nx_status.c
    src/nx_device.c
    src/nx_hal.c
    src/nx_adapter.c
    src/nx_assert.c
    src/nx_mem.c
    src/nx_mutex.c
    src/nx_power_manager.c
    src/nx_error.c
)

# Create HAL static library
add_library(hal STATIC ${HAL_CORE_SOURCES})

# Include directories
target_include_directories(hal
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(hal
    PUBLIC
        hal_interface
        osal
)

# Compile definitions
target_compile_definitions(hal
    PRIVATE
        $<$<CONFIG:Debug>:HAL_DEBUG>
)

# Compiler options
if(MSVC)
    target_compile_options(hal PRIVATE /W4 /FS)
else()
    target_compile_options(hal PRIVATE -Wall -Wextra -Wpedantic)
endif()

message(STATUS "HAL Library: Configured")

#-----------------------------------------------------------------------------
# Platform-specific sources are added by platform CMakeLists.txt
#-----------------------------------------------------------------------------
