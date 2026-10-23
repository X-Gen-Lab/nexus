# Native providers model complete public interfaces independently of assembly.
add_library(nexus_io_native STATIC
    drivers/time.c drivers/gpio.c drivers/uart.c drivers/uart_word.c
    drivers/spi.c drivers/i2c.c
    drivers/flash.c drivers/watchdog.c drivers/exti.c
    drivers/timer.c drivers/adc.c
    drivers/spi_async.c drivers/spi_dma_transaction.c drivers/stream.c
    drivers/can.c drivers/ethernet.c drivers/ethernet_frames.c
    drivers/usb_device.c drivers/rtc_lptim.c)
target_link_libraries(nexus_io_native PUBLIC nexus_io nexus_arch
    PRIVATE nexus_build_options nexus_owned_warnings)
target_include_directories(nexus_io_native PRIVATE
    "${NEXUS_SOURCE_DIR}/io/private")
add_library(nexus_can_native_engine OBJECT
    "${NEXUS_SOURCE_DIR}/io/src/controllers/can_controller.c")
target_include_directories(nexus_can_native_engine PRIVATE
    "${NEXUS_SOURCE_DIR}/io/private"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers")
target_link_libraries(nexus_can_native_engine PRIVATE nexus_io nexus_arch
    nexus_build_options nexus_owned_warnings)
target_compile_options(nexus_can_native_engine PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/drivers/can_access.h")
target_sources(nexus_io_native PRIVATE $<TARGET_OBJECTS:nexus_can_native_engine>)
add_library(nexus_eth_native_engine OBJECT
    "${NEXUS_SOURCE_DIR}/io/src/controllers/eth_mac.c")
target_include_directories(nexus_eth_native_engine PRIVATE
    "${NEXUS_SOURCE_DIR}/io/private"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers")
target_link_libraries(nexus_eth_native_engine PRIVATE nexus_io nexus_arch
    nexus_build_options nexus_owned_warnings)
target_compile_definitions(nexus_eth_native_engine PRIVATE NX_ETH_NATIVE)
target_sources(nexus_io_native PRIVATE $<TARGET_OBJECTS:nexus_eth_native_engine>)
add_library(nexus_eth_frames_native_engine OBJECT
    "${NEXUS_SOURCE_DIR}/io/src/controllers/eth_frames_mac.c")
target_include_directories(nexus_eth_frames_native_engine PRIVATE
    "${NEXUS_SOURCE_DIR}/io/private"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers")
target_link_libraries(nexus_eth_frames_native_engine PRIVATE nexus_io nexus_arch
    nexus_build_options nexus_owned_warnings)
target_compile_definitions(nexus_eth_frames_native_engine PRIVATE
    NX_ETH_FRAMES_NATIVE)
target_sources(nexus_io_native PRIVATE
    $<TARGET_OBJECTS:nexus_eth_frames_native_engine>)
add_library(nexus_usb_native_engine OBJECT
    "${NEXUS_SOURCE_DIR}/io/src/controllers/usb_device_controller.c")
target_include_directories(nexus_usb_native_engine PRIVATE
    "${NEXUS_SOURCE_DIR}/io/private"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers")
target_link_libraries(nexus_usb_native_engine PRIVATE nexus_io nexus_arch
    nexus_build_options nexus_owned_warnings)
target_compile_options(nexus_usb_native_engine PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb_device_access.h")
target_sources(nexus_io_native PRIVATE $<TARGET_OBJECTS:nexus_usb_native_engine>)
add_library(Nexus::NativeModel ALIAS nexus_io_native)
