# Keep the Connectivity B-device adapter and shared DWC core selection-local.
# Author: Nexus Team
if("device-fs" IN_LIST f1_usb)
    set(dwc "${NEXUS_SOURCE_DIR}/soc/stm32/common/dwc_otg_device")
    target_sources(
        nexus_soc_stm32f1
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/connectivity_device.c"
            "${dwc}/usb_device_controller.c"
    )
    target_include_directories(nexus_soc_stm32f1 PRIVATE "${dwc}")
    target_link_libraries(
        nexus_soc_stm32f1
        PRIVATE Nexus::IOUSBDevice Nexus::IODMA
    )
endif()

if("device-pma" IN_LIST f1_usb OR f1_pma_ep1)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE pma.c ep0.c ep0_mmio.c lp.c link.c pma_device.c pma_board.c
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOUSBDevice)
endif()

if(f1_usb_host)
    set(host_core "${NEXUS_SOURCE_DIR}/soc/stm32/common/dwc_otg_host")
    set(host_hal_config "${CMAKE_CURRENT_LIST_DIR}/../../sdk/hal_config/usb")
    add_library(
        nexus_stm32f1_usb_hal
        OBJECT
        "${f1_hal}/Src/stm32f1xx_hal_hcd.c"
        "${f1_hal}/Src/stm32f1xx_ll_usb.c"
    )
    target_include_directories(
        nexus_stm32f1_usb_hal
        PRIVATE "${host_hal_config}"
    )
    target_include_directories(
        nexus_stm32f1_usb_hal
        SYSTEM
        PRIVATE "${f1_hal}/Inc" "${f1_device}/Include"
    )
    target_compile_definitions(
        nexus_stm32f1_usb_hal
        PRIVATE "${NEXUS_VENDOR_DEVICE_DEFINE}"
    )
    target_link_libraries(nexus_stm32f1_usb_hal PRIVATE nexus_build_options)
    nexus_use_cmsis_core(nexus_stm32f1_usb_hal)

    target_sources(
        nexus_soc_stm32f1
        PRIVATE
            connectivity_host.c
            "${host_core}/usb_host.c"
            $<TARGET_OBJECTS:nexus_stm32f1_usb_hal>
    )
    target_include_directories(
        nexus_soc_stm32f1
        PRIVATE "${host_core}" "${host_hal_config}"
    )
    target_compile_definitions(nexus_soc_stm32f1 PRIVATE NX_STM32F1_USB_HOST=1)
    if(f1_usb_host_core)
        target_compile_definitions(
            nexus_soc_stm32f1
            PRIVATE NX_STM32F1_USB_HOST_CORE=1
        )
    elseif(f1_usb_host_bulk)
        target_compile_definitions(
            nexus_soc_stm32f1
            PRIVATE NX_STM32F1_USB_HOST_BULK=1
        )
    endif()

    target_link_libraries(
        nexus_soc_stm32f1
        PRIVATE Nexus::IOUSBHost Nexus::IODMA
    )
endif()

if(f1_pma_ep1)
    target_sources(nexus_soc_stm32f1 PRIVATE pma_ep1.c)
    target_compile_definitions(
        nexus_soc_stm32f1
        PRIVATE NX_STM32F1_USB_PMA_EP1=1
    )
endif()
