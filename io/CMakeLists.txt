# Contracts and shared checked helpers; physical providers are selected by SoC.
add_library(nexus_io STATIC
    src/api/adc.c
    src/api/adc_group.c
    src/api/advanced_pwm.c
    src/api/can.c
    src/api/can_fd.c
    src/api/encoder.c
    src/api/ethernet.c
    src/api/ethernet_frames.c
    src/api/exti.c
    src/api/flash.c
    src/api/gpio.c
    src/api/i2c.c
    src/api/lptim.c
    src/api/mdio.c
    src/api/pin_state.c
    src/api/pwm_adc.c
    src/api/pwm_group.c
    src/api/rtc.c
    src/api/spi.c
    src/api/timer.c
    src/api/timer_capture.c
    src/api/uart.c
    src/api/uart_word.c
    src/api/usb_device.c
    src/api/watchdog.c
    src/mechanisms/adc_format.c
    src/mechanisms/adc_observer.c
    src/mechanisms/diagnostic.c
    src/mechanisms/dma.c
    src/mechanisms/flash_region.c
    src/mechanisms/frame_stream.c
    src/mechanisms/i2c_bus_clear.c
    src/mechanisms/i2c_long.c
    src/mechanisms/observation.c
    src/mechanisms/spi_dma_owner_binding.c
    src/mechanisms/spi_dma_sequence.c
    src/mechanisms/spi_dma_transaction.c
    src/mechanisms/spi_transaction.c
    src/mechanisms/stream.c
    src/mechanisms/stream_budget.c
    src/mechanisms/stream_profile.c
    src/mechanisms/uart_rs485.c
    src/mechanisms/usb_profile.c
    src/mechanisms/wake.c)
target_include_directories(nexus_io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(nexus_io PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_link_libraries(nexus_io PUBLIC nexus_core
    PRIVATE nexus_build_options nexus_owned_warnings Nexus::Arch)
add_library(Nexus::IO ALIAS nexus_io)
add_library(nexus_io_cycle_probe STATIC EXCLUDE_FROM_ALL
    src/mechanisms/cycle_probe.c)
target_link_libraries(nexus_io_cycle_probe PUBLIC nexus_io
    PRIVATE nexus_arch nexus_build_options nexus_owned_warnings)
add_library(Nexus::IOCycleProbe ALIAS nexus_io_cycle_probe)
