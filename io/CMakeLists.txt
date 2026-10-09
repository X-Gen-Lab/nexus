# Contracts and shared checked helpers; physical providers are selected by SoC.
add_library(nexus_io STATIC src/uart.c src/flash_region.c)
target_include_directories(nexus_io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_io PUBLIC nexus_core PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::IO ALIAS nexus_io)
if(NEXUS_SOC_FAMILY STREQUAL "native")
    add_library(nexus_io_native STATIC
        native/time.c native/gpio.c native/uart.c native/spi.c native/i2c.c
        native/flash.c native/watchdog.c native/exti.c native/timer.c native/adc.c)
    target_link_libraries(nexus_io_native PUBLIC nexus_io nexus_arch
        PRIVATE nexus_build_options nexus_owned_warnings)
    add_library(Nexus::NativeModel ALIAS nexus_io_native)
endif()
