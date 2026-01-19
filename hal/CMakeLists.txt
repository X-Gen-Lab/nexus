##############################################################################
# HAL Library
##############################################################################

# HAL interface library (header-only for now)
add_library(hal_interface INTERFACE)

target_include_directories(hal_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}  # 包含项目根目录以访问 nexus_config.h
)

# HAL implementation library
add_library(hal STATIC)

target_sources(hal
    PRIVATE
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

target_link_libraries(hal
    PUBLIC
        hal_interface
        osal
)

# Platform-specific sources are added by platform CMakeLists.txt
