# Private build-time header paths; exact SoC targets still own source selection.
add_library(nexus_driver_headers INTERFACE)
target_include_directories(nexus_driver_headers INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/can/classic_can"
    "${CMAKE_CURRENT_SOURCE_DIR}/ethernet/legacy_mac"
    "${CMAKE_CURRENT_SOURCE_DIR}/gpio/owned_pin_state"
    "${CMAKE_CURRENT_SOURCE_DIR}/timer/advanced_timer"
    "${CMAKE_CURRENT_SOURCE_DIR}/uart/usart_fifo"
    "${CMAKE_CURRENT_SOURCE_DIR}/usb/fs_device_pio")
add_library(Nexus::DriverPrivateHeaders ALIAS nexus_driver_headers)
