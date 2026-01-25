##############################################################################
# HAL Native Platform Tests
##############################################################################

# Collect all test sources
set(HAL_NATIVE_TEST_SOURCES
    # Test helpers
    native_test_helpers.c
    # Device helpers
    devices/native_time_sim.c
    devices/native_adc_helpers.c
    devices/native_crc_helpers.c
    devices/native_dac_helpers.c
    devices/native_flash_helpers.c
    devices/native_gpio_helpers.c
    devices/native_i2c_helpers.c
    devices/native_rtc_helpers.c
    devices/native_spi_helpers.c
    devices/native_timer_helpers.c
    devices/native_uart_helpers.c
    devices/native_watchdog_helpers.c
    devices/native_usb_helpers.c
    devices/native_sdio_helpers.c
    devices/native_option_bytes_helpers.c
    # Unit tests
    test_nx_crc.cpp
    test_nx_flash.cpp
    test_nx_gpio.cpp
    test_nx_uart.cpp
    test_nx_spi.cpp
    test_nx_i2c.cpp
    test_nx_rtc.cpp
    test_nx_timer.cpp
    test_nx_adc.cpp
    test_nx_dac.cpp
    test_nx_sdio.cpp
    test_nx_usb.cpp
    test_nx_watchdog.cpp
    test_nx_option_bytes.cpp
    test_nx_platform.cpp
    test_nx_device_msvc.cpp
    # Property-based tests
    test_nx_crc_properties.cpp
    test_nx_flash_properties.cpp
    test_nx_gpio_properties.cpp
    test_nx_uart_properties.cpp
    test_nx_spi_properties.cpp
    test_nx_i2c_properties.cpp
    test_nx_rtc_properties.cpp
    test_nx_timer_properties.cpp
    test_nx_adc_properties.cpp
    test_nx_dac_properties.cpp
    test_nx_sdio_properties.cpp
    test_nx_usb_properties.cpp
    test_nx_watchdog_properties.cpp
    test_nx_option_bytes_properties.cpp
    test_nx_platform_properties.cpp
)

# Create test executable
add_executable(hal_native_tests
    ${CMAKE_SOURCE_DIR}/tests/test_main.cpp
    ${HAL_NATIVE_TEST_SOURCES}
)

# C++20 for designated initializers
target_compile_features(hal_native_tests PRIVATE cxx_std_20)

# Test mode definition
target_compile_definitions(hal_native_tests PRIVATE NX_STARTUP_TEST_MODE)

# Compile options
target_compile_options(hal_native_tests PRIVATE ${NEXUS_TEST_COMPILE_OPTIONS})

# Include directories (add current directory for local headers)
target_include_directories(hal_native_tests
    PRIVATE
        ${NEXUS_TEST_INCLUDES}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(hal_native_tests PRIVATE ${NEXUS_TEST_LIBS})

# Discover tests
gtest_discover_tests(hal_native_tests
    PROPERTIES
        TIMEOUT 300
        LABELS "hal;native;unit;property"
)
