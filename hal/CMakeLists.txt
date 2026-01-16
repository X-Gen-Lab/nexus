##############################################################################
# HAL Library
##############################################################################

# HAL interface library (header-only for now)
add_library(hal_interface INTERFACE)

target_include_directories(hal_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# HAL implementation library
add_library(hal STATIC)

target_sources(hal
    PRIVATE
        src/nx_status.c
        src/nx_device.c
        src/nx_device_registry.c
        src/nx_hal.c
)

target_link_libraries(hal
    PUBLIC
        hal_interface
)

# Platform-specific sources are added by platform CMakeLists.txt
