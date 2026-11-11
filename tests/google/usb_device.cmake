# Actual PIO mechanism, SDK register facts and retained 48MHz cold ownership.
nexus_google_test(nexus_usb_device_test SOURCES usb_device_test.cpp
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
        "${NEXUS_SOURCE_DIR}/io/private")
nexus_google_test(nexus_usb_device_dma_test SOURCES usb_device_dma_test.cpp
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
        "${NEXUS_SOURCE_DIR}/io/private")
foreach(usb_family stm32f4)
    set(usb_target "nexus_${usb_family}_usb_device_test")
    nexus_google_test(${usb_target} SOURCES usb_device_soc_test.cpp
        "${NEXUS_SOC_SOURCE_${usb_family}}/drivers/usb_device.c"
        "${NEXUS_SOC_SOURCE_${usb_family}}/usb_clock.c"
        LIBRARIES Nexus::NativeModel Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
            "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOC_SOURCE_${usb_family}}/private")
    target_compile_options(${usb_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/usb_device_soc_model.h")
    target_include_directories(${usb_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${usb_target})
    target_compile_definitions(${usb_target} PRIVATE STM32F407xx)
endforeach()

nexus_google_test(nexus_usb_control_test SOURCES usb_control_test.cpp
    LIBRARIES Nexus::USBControl Nexus::NativeModel Nexus::Arch)
nexus_google_test(nexus_usb_descriptor_test SOURCES usb_descriptor_test.cpp
    LIBRARIES Nexus::USBDescriptors)
nexus_google_test(nexus_usb_acm_test SOURCES usb_acm_test.cpp
    LIBRARIES Nexus::USBACM Nexus::NativeModel Nexus::Arch)
nexus_google_test(nexus_usb_role_test SOURCES usb_role_test.cpp
    LIBRARIES Nexus::USBRole Nexus::NativeModel Nexus::Arch)
nexus_google_test(nexus_stm32_usb_host_hal_test STM32F4_LL
    SOURCES usb_host_hal_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/usb_host.c"
    LIBRARIES Nexus::IOUSBHost Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private/usb_hal")
target_compile_definitions(nexus_stm32_usb_host_hal_test PRIVATE STM32F407xx
    nx_time_now_us=nx_usb_host_model_now NX_USB_HOST_SIGNAL_MODEL=1)
target_compile_options(nexus_stm32_usb_host_hal_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/usb_host_hal_model.h")
target_include_directories(nexus_stm32_usb_host_hal_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_usb_host_hal_test)
nexus_google_test(nexus_stm32_usb_hs_cold_test
    SOURCES usb_hs_cold_test.cpp
        "${NEXUS_SOC_SOURCE_stm32f4}/drivers/usb_device.c"
        "${NEXUS_SOC_SOURCE_stm32f4}/usb_clock.c"
        "${NEXUS_SOC_SOURCE_stm32f4}/usb_hs_clock.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
        "${NEXUS_SOC_SOURCE_stm32f4}/private")
target_compile_options(nexus_stm32_usb_hs_cold_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/usb_device_soc_model.h")
target_include_directories(nexus_stm32_usb_hs_cold_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_usb_hs_cold_test)
target_compile_definitions(nexus_stm32_usb_hs_cold_test PRIVATE STM32F407xx)
nexus_add_st_usb_host()
nexus_google_test(nexus_st_usb_host_stack_test SOURCES st_usb_host_stack_test.cpp
    LIBRARIES Nexus::STUSBHost Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/components/st_usb/private"
        "${NEXUS_SOURCE_DIR}/vendors/st/usb_host/Core/Inc")
include("${CMAKE_CURRENT_LIST_DIR}/st_usb_device.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/usb_otg.cmake")

nexus_google_test(nexus_st_usb_host_delay_test
    SOURCES st_usb_host_delay_test.cpp
        "${NEXUS_SOURCE_DIR}/components/st_usb/src/usb_host_stack.c"
    LIBRARIES Nexus::IOUSBHost Nexus::Arch nexus_st_usb_host_core
    INCLUDES "${NEXUS_SOURCE_DIR}/components/st_usb/private"
        "${NEXUS_SOURCE_DIR}/vendors/st/usb_host/Core/Inc")
target_compile_options(nexus_st_usb_host_delay_test PRIVATE
    "-include${CMAKE_CURRENT_LIST_DIR}/st_usb_host_delay_model.h")
