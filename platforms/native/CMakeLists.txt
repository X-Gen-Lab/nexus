##############################################################################
# Native Platform Support (for host testing)
##############################################################################

# Platform library
add_library(platform_native STATIC)

target_sources(platform_native
    PRIVATE
        src/hal_gpio_native.c
        src/hal_uart_native.c
        src/hal_spi_native.c
        src/hal_i2c_native.c
        src/hal_timer_native.c
        src/hal_adc_native.c
        src/hal_system_native.c
        src/nx_dma_native.c
        src/nx_isr_native.c
        src/nx_gpio_native.c
        src/nx_uart_native.c
        src/nx_spi_native.c
        src/nx_i2c_native.c
        src/nx_timer_native.c
        src/nx_adc_native.c
        src/nx_factory_native.c
)

target_include_directories(platform_native
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(platform_native
    PUBLIC
        hal_interface
)

# Native platform doesn't need linker script
