##############################################################################
# Native Platform Support (for host testing)
##############################################################################

# Platform library
add_library(platform_native STATIC)

#-----------------------------------------------------------------------------
# Platform Initialization
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/platform/nx_platform_init.c
        src/platform/nx_device_registry_stub.c
)

#-----------------------------------------------------------------------------
# Resource Managers
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/resource/nx_dma_native.c
        src/resource/nx_isr_native.c
)

#-----------------------------------------------------------------------------
# GPIO Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/gpio/nx_gpio_device.c
        src/gpio/nx_gpio_helpers.c
        src/gpio/nx_gpio_read.c
        src/gpio/nx_gpio_write.c
        src/gpio/nx_gpio_read_write.c
        src/gpio/nx_gpio_lifecycle.c
        src/gpio/nx_gpio_power.c
)

#-----------------------------------------------------------------------------
# UART Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/uart/nx_uart_device.c
        src/uart/nx_uart_helpers.c
        src/uart/nx_uart_async.c
        src/uart/nx_uart_sync.c
        src/uart/nx_uart_lifecycle.c
        src/uart/nx_uart_power.c
        src/uart/nx_uart_diagnostic.c
)

#-----------------------------------------------------------------------------
# SPI Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/spi/nx_spi_device.c
        src/spi/nx_spi_helpers.c
        src/spi/nx_spi_async.c
        src/spi/nx_spi_sync.c
        src/spi/nx_spi_lifecycle.c
        src/spi/nx_spi_power.c
        src/spi/nx_spi_diagnostic.c
)

#-----------------------------------------------------------------------------
# I2C Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/i2c/nx_i2c_device.c
        src/i2c/nx_i2c_helpers.c
        src/i2c/nx_i2c_async.c
        src/i2c/nx_i2c_sync.c
        src/i2c/nx_i2c_lifecycle.c
        src/i2c/nx_i2c_power.c
        src/i2c/nx_i2c_diagnostic.c
)

#-----------------------------------------------------------------------------
# Timer Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/timer/nx_timer_device.c
        src/timer/nx_timer_helpers.c
        src/timer/nx_timer_lifecycle.c
        src/timer/nx_timer_power.c
)

#-----------------------------------------------------------------------------
# ADC Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/adc/nx_adc_device.c
        src/adc/nx_adc_buffer_device.c
        src/adc/nx_adc_lifecycle.c
        src/adc/nx_adc_power.c
        src/adc/nx_adc_diagnostic.c
        src/adc/nx_adc_buffer_lifecycle.c
        src/adc/nx_adc_buffer_power.c
)

#-----------------------------------------------------------------------------
# DAC Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/dac/nx_dac_device.c
        src/dac/nx_dac_lifecycle.c
        src/dac/nx_dac_power.c
)

#-----------------------------------------------------------------------------
# CRC Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/crc/nx_crc_device.c
        src/crc/nx_crc_helpers.c
        src/crc/nx_crc_interface.c
        src/crc/nx_crc_lifecycle.c
        src/crc/nx_crc_power.c
)

#-----------------------------------------------------------------------------
# Flash Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/flash/nx_flash_device.c
        src/flash/nx_flash_helpers.c
        src/flash/nx_flash_interface.c
        src/flash/nx_flash_lifecycle.c
)

#-----------------------------------------------------------------------------
# RTC Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/rtc/nx_rtc_device.c
        src/rtc/nx_rtc_helpers.c
        src/rtc/nx_rtc_interface.c
        src/rtc/nx_rtc_lifecycle.c
        src/rtc/nx_rtc_power.c
)

#-----------------------------------------------------------------------------
# USB Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/usb/nx_usb_device.c
        src/usb/nx_usb_helpers.c
        src/usb/nx_usb_interface.c
        src/usb/nx_usb_lifecycle.c
        src/usb/nx_usb_power.c
)

#-----------------------------------------------------------------------------
# Watchdog Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/watchdog/nx_watchdog_device.c
        src/watchdog/nx_watchdog_helpers.c
        src/watchdog/nx_watchdog_interface.c
        src/watchdog/nx_watchdog_lifecycle.c
        src/watchdog/nx_watchdog_power.c
)

#-----------------------------------------------------------------------------
# SDIO Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/sdio/nx_sdio_device.c
        src/sdio/nx_sdio_helpers.c
)

#-----------------------------------------------------------------------------
# Option Bytes Peripheral
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/option_bytes/nx_option_bytes_device.c
        src/option_bytes/nx_option_bytes_helpers.c
        src/option_bytes/nx_option_bytes_interface.c
        src/option_bytes/nx_option_bytes_lifecycle.c
        src/option_bytes/nx_option_bytes_power.c
)

#-----------------------------------------------------------------------------
# Legacy Peripheral Files (to be refactored)
#-----------------------------------------------------------------------------
# target_sources(platform_native
#     PRIVATE
#         src/nx_flash_native.c
#         src/nx_rtc_native.c
#         src/nx_watchdog_native.c
#         # src/nx_crc_native.c  # Replaced by new CRC implementation
#         src/nx_usb_native.c
#         src/nx_sdio_native.c
#         src/nx_option_bytes_native.c
# )

#-----------------------------------------------------------------------------
# Platform Initialization
#-----------------------------------------------------------------------------
target_sources(platform_native
    PRIVATE
        src/platform/nx_platform_init.c
)

#-----------------------------------------------------------------------------
# Include Directories
#-----------------------------------------------------------------------------
target_include_directories(platform_native
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

#-----------------------------------------------------------------------------
# Link Libraries
#-----------------------------------------------------------------------------
target_link_libraries(platform_native
    PUBLIC
        hal_interface
)

#-----------------------------------------------------------------------------
# Compiler Options
#-----------------------------------------------------------------------------
# Fix MSVC parallel compilation PDB issue
if(MSVC)
    target_compile_options(platform_native PRIVATE /FS)
    # Disable warnings caused by Kconfig macro expansion
    target_compile_options(platform_native PRIVATE
        /wd4131  # 'function': uses old-style declarator
        /wd4431  # missing type specifier - int assumed
        /wd4218  # nonstandard extension used: must specify storage class
    )
endif()

# Native platform doesn't need linker script
