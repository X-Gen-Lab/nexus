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
    target_compile_definitions(${usb_target} PRIVATE STM32F407xx
        NEXUS_STM32F4_USB_REMOTE_WAKEUP=1)
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
target_compile_definitions(nexus_stm32_usb_hs_cold_test PRIVATE STM32F407xx
    NEXUS_STM32F4_USB_REMOTE_WAKEUP=1)
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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    set(usb_transport_target nexus_stm32_usb_host_transport_test)
    set(usb_transport_vendor "${usb_transport_target}_vendor")
    set(usb_transport_model
        "${CMAKE_CURRENT_SOURCE_DIR}/usb_host_transport_model.h")
    set(usb_transport_tool
        "${NEXUS_SOURCE_DIR}/tools/testing/instrument_usb_sdk.py")
    set(usb_transport_generated
        "${CMAKE_CURRENT_BINARY_DIR}/usb-host-transport-sdk")
    # This tool verifies exact upstream bytes and match counts each invocation.
    # Only MMIO operands change; it does not supply a transfer or stop outcome.
    add_custom_command(OUTPUT
        "${usb_transport_generated}/stm32f4xx_hal_hcd.c"
        "${usb_transport_generated}/stm32f4xx_ll_usb.c"
        "${usb_transport_generated}/instrumentation.json"
        COMMAND "${Python3_EXECUTABLE}" -B "${usb_transport_tool}"
            "${NEXUS_SOURCE_DIR}" "${usb_transport_generated}"
        DEPENDS "${usb_transport_tool}"
            "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_hcd.c"
            "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_ll_usb.c"
        VERBATIM)
    nexus_google_test(${usb_transport_target} STM32F4_LL
        SOURCES usb_host_transport_test.cpp
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/usb_host.c"
        LIBRARIES Nexus::IOUSBHost Nexus::NativeModel Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private/usb_hal")
    target_compile_definitions(${usb_transport_target} PRIVATE STM32F407xx)
    target_compile_options(${usb_transport_target} PRIVATE
        "-include${usb_transport_model}")
    target_include_directories(${usb_transport_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core(${usb_transport_target})
    add_library(${usb_transport_vendor} OBJECT
        "${usb_transport_generated}/stm32f4xx_hal_hcd.c"
        "${usb_transport_generated}/stm32f4xx_ll_usb.c")
    target_compile_features(${usb_transport_vendor} PRIVATE c_std_11)
    set_target_properties(${usb_transport_vendor} PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(${usb_transport_vendor} PRIVATE STM32F407xx)
    target_compile_options(${usb_transport_vendor} PRIVATE
        "-include${usb_transport_model}"
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic
        -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast)
    target_include_directories(${usb_transport_vendor} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private/usb_hal"
        "${NEXUS_SOURCE_DIR}/io/include"
        "${NEXUS_SOURCE_DIR}/core/include"
        "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${usb_transport_vendor} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    target_link_libraries(${usb_transport_vendor} PRIVATE
        nexus_build_options Nexus::Arch)
    nexus_stm32f4_official(${usb_transport_vendor})
    nexus_use_cmsis_core(${usb_transport_vendor})
    nexus_use_cmsis_host_model(${usb_transport_vendor})
    target_sources(${usb_transport_target} PRIVATE
        "$<TARGET_OBJECTS:${usb_transport_vendor}>")
endif()
