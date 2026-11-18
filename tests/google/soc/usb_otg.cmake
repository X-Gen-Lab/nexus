# Actual composite plus both complete existing transports, hardware leaves mocked.
nexus_google_test(nexus_stm32_usb_otg_test STM32F4_LL
    SOURCES "${nexus_google_root}/soc/usb_otg_test.cpp"
        "${NEXUS_SOC_SOURCE_stm32f4}/drivers/usb/usb_otg.c"
        "${NEXUS_SOC_SOURCE_stm32f4}/drivers/usb/usb_device.c"
        "${NEXUS_SOC_SOURCE_stm32f4}/drivers/usb/usb_host.c"
        "${NEXUS_SOC_SOURCE_stm32f4}/drivers/usb/usb_clock.c"
        "${NEXUS_SOC_SOURCE_stm32f4}/drivers/usb/usb_hs_clock.c"
    LIBRARIES Nexus::IOUSBOTG Nexus::USBRole Nexus::NativeModel Nexus::Arch
    INCLUDES
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/dwc_otg_device" "${NEXUS_SOC_SOURCE_stm32f4}"
        "${NEXUS_SOC_SOURCE_stm32f4}/private/usb_hal"
        "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
)
target_compile_options(
    nexus_stm32_usb_otg_test
    PRIVATE "-include${nexus_google_root}/soc/usb_otg_model.h"
)
target_compile_definitions(
    nexus_stm32_usb_otg_test
    PRIVATE STM32F407xx NEXUS_STM32F4_USB_REMOTE_WAKEUP=1
)
target_include_directories(
    nexus_stm32_usb_otg_test
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
)
nexus_use_cmsis_core(nexus_stm32_usb_otg_test)
