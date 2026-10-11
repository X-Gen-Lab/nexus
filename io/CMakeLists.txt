# Contracts and shared checked helpers; physical providers are selected by SoC.
add_library(nexus_io STATIC
    src/gpio.c src/uart.c src/spi.c src/i2c.c src/flash.c
    src/watchdog.c src/exti.c src/timer.c src/adc.c src/flash_region.c
    src/dma.c src/stream.c src/wake.c)
target_include_directories(nexus_io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_io PUBLIC nexus_core PRIVATE nexus_build_options nexus_owned_warnings Nexus::Arch)
add_library(Nexus::IO ALIAS nexus_io)
if(NEXUS_SOC_FAMILY STREQUAL "native")
    add_library(nexus_io_native STATIC
        native/time.c native/gpio.c native/uart.c native/spi.c native/i2c.c
        native/flash.c native/watchdog.c native/exti.c native/timer.c native/adc.c
        native/spi_async.c native/stream.c)
    target_link_libraries(nexus_io_native PUBLIC nexus_io nexus_arch
        PRIVATE nexus_build_options nexus_owned_warnings)
    add_library(Nexus::NativeModel ALIAS nexus_io_native)
endif()
