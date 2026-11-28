# Selected usb sources and private dependencies.
nexus_controller_modes(usb_device_modes usb_device)
nexus_controller_modes(usb_host_modes usb_host)
set(stm32_usb_host_hal OFF)
if(usb_device_modes)
    nexus_stm32f4_gpio(nexus_soc_stm32f4)
    target_sources(
        nexus_soc_stm32f4
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/usb_device.c"
            "${CMAKE_CURRENT_LIST_DIR}/usb_clock.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/common/dwc_otg_device/usb_device_controller.c"
    )
    if(
        "device-hs-fs" IN_LIST usb_device_modes
        OR "device-hs-ulpi" IN_LIST usb_device_modes
        OR "otg-hs-fs" IN_LIST usb_device_modes
        OR "otg-hs-ulpi" IN_LIST usb_device_modes
    )
        target_sources(
            nexus_soc_stm32f4
            PRIVATE "${CMAKE_CURRENT_LIST_DIR}/usb_hs_clock.c"
        )
    endif()
    if(
        "otg-fs" IN_LIST usb_device_modes
        OR "otg-hs-fs" IN_LIST usb_device_modes
        OR "otg-hs-ulpi" IN_LIST usb_device_modes
    )
        set(stm32_usb_host_hal ON)
        target_sources(
            nexus_soc_stm32f4
            PRIVATE
                "${CMAKE_CURRENT_LIST_DIR}/usb_otg.c"
                "${NEXUS_SOURCE_DIR}/soc/stm32/common/dwc_otg_host/usb_host.c"
                "${CMAKE_CURRENT_LIST_DIR}/usb_hs_clock.c"
        )
        target_link_libraries(
            nexus_soc_stm32f4
            PUBLIC Nexus::IOUSBOTG Nexus::USBRole
        )
    endif()
endif()
if(usb_host_modes)
    nexus_stm32f4_gpio(nexus_soc_stm32f4)
    set(stm32_usb_host_hal ON)
    target_sources(
        nexus_soc_stm32f4
        PRIVATE "${NEXUS_SOURCE_DIR}/soc/stm32/common/dwc_otg_host/usb_host.c"
    )
endif()
if(stm32_usb_host_hal)
    nexus_stm32f407_usb_hal(nexus_soc_stm32f4 HOST)
endif()
if(NEXUS_FEATURE_USB_REMOTE_WAKEUP)
    target_compile_definitions(
        nexus_soc_stm32f4
        PRIVATE NEXUS_STM32F4_USB_REMOTE_WAKEUP=1
    )
endif()
