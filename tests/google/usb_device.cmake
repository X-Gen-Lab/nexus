# Actual PIO mechanism, SDK register facts and retained 48MHz cold ownership.
nexus_google_test(nexus_usb_device_test SOURCES usb_device_test.cpp
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
