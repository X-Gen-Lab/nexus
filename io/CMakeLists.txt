# Contracts and shared checked helpers; physical providers are selected by SoC.
add_library(nexus_io STATIC
    src/gpio.c src/uart.c src/spi.c src/i2c.c src/flash.c
    src/watchdog.c src/exti.c src/timer.c src/adc.c src/flash_region.c
    src/dma.c src/stream.c src/wake.c src/i2c_bus_clear.c src/adc_observer.c
    src/uart_rs485.c src/stream_budget.c src/can.c src/ethernet.c src/mdio.c
    src/usb_device.c src/timer_capture.c src/advanced_pwm.c)
target_include_directories(nexus_io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(nexus_io PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_link_libraries(nexus_io PUBLIC nexus_core PRIVATE nexus_build_options nexus_owned_warnings Nexus::Arch)
add_library(Nexus::IO ALIAS nexus_io)
if(NEXUS_SOC_FAMILY STREQUAL "native")
    add_library(nexus_io_native STATIC
        native/time.c native/gpio.c native/uart.c native/spi.c native/i2c.c
        native/flash.c native/watchdog.c native/exti.c native/timer.c native/adc.c
        native/spi_async.c native/stream.c native/can.c native/ethernet.c
        native/usb_device.c)
    target_link_libraries(nexus_io_native PUBLIC nexus_io nexus_arch
        PRIVATE nexus_build_options nexus_owned_warnings)
    target_include_directories(nexus_io_native PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private")
    add_library(nexus_can_native_engine OBJECT src/can_controller.c)
    target_include_directories(nexus_can_native_engine PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private"
        "${CMAKE_CURRENT_SOURCE_DIR}/native")
    target_link_libraries(nexus_can_native_engine PRIVATE nexus_io nexus_arch
        nexus_build_options nexus_owned_warnings)
    target_compile_options(nexus_can_native_engine PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/native/can_access.h")
    target_sources(nexus_io_native PRIVATE
        $<TARGET_OBJECTS:nexus_can_native_engine>)
    add_library(nexus_eth_native_engine OBJECT src/eth_mac.c)
    target_include_directories(nexus_eth_native_engine PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private"
        "${CMAKE_CURRENT_SOURCE_DIR}/native")
    target_link_libraries(nexus_eth_native_engine PRIVATE nexus_io nexus_arch
        nexus_build_options nexus_owned_warnings)
    target_compile_definitions(nexus_eth_native_engine PRIVATE NX_ETH_NATIVE)
    target_sources(nexus_io_native PRIVATE
        $<TARGET_OBJECTS:nexus_eth_native_engine>)
    add_library(nexus_usb_native_engine OBJECT src/usb_device_controller.c)
    target_include_directories(nexus_usb_native_engine PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private"
        "${CMAKE_CURRENT_SOURCE_DIR}/native")
    target_link_libraries(nexus_usb_native_engine PRIVATE nexus_io nexus_arch
        nexus_build_options nexus_owned_warnings)
    target_compile_options(nexus_usb_native_engine PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/native/usb_device_access.h")
    target_sources(nexus_io_native PRIVATE
        $<TARGET_OBJECTS:nexus_usb_native_engine>)
    add_library(Nexus::NativeModel ALIAS nexus_io_native)
endif()
