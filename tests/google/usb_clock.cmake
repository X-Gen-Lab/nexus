# Selected clock implementations execute against the pinned SDK register
# layouts. Only the external MMIO and CPU/time boundaries are modeled.
foreach(clock_family stm32f4)
    set(clock_target "nexus_${clock_family}_usb_clock_test")
    nexus_google_test(${clock_target} SOURCES usb_clock_test.cpp
        "${NEXUS_SOC_SOURCE_${clock_family}}/usb_clock.c"
        LIBRARIES Nexus::Core
        INCLUDES "${NEXUS_SOC_SOURCE_${clock_family}}/private"
            "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/io/include"
            "${NEXUS_SOURCE_DIR}/io/private")
    target_compile_options(${clock_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/usb_clock_model.h")
    target_include_directories(${clock_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${clock_target})
    target_compile_definitions(${clock_target} PRIVATE STM32F407xx
        NEXUS_STM32F4_USB_REMOTE_WAKEUP=1)
endforeach()
nexus_google_test(nexus_stm32f4_usb_hs_clock_test
    SOURCES usb_hs_clock_test.cpp
        "${NEXUS_SOC_SOURCE_stm32f4}/usb_hs_clock.c"
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOC_SOURCE_stm32f4}/private"
        "${NEXUS_SOURCE_DIR}/io/include" "${NEXUS_SOURCE_DIR}/arch/include")
target_compile_options(nexus_stm32f4_usb_hs_clock_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/usb_clock_model.h")
target_include_directories(nexus_stm32f4_usb_hs_clock_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32f4_usb_hs_clock_test)
target_compile_definitions(nexus_stm32f4_usb_hs_clock_test PRIVATE STM32F407xx
    NEXUS_STM32F4_USB_REMOTE_WAKEUP=1)

nexus_google_test(nexus_stm32_usb_hal_time_test
    SOURCES usb_hal_time_test.cpp
        "${NEXUS_SOC_SOURCE_stm32f4}/drivers/usb_hal_time.c"
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOC_SOURCE_stm32f4}/private"
        "${NEXUS_SOURCE_DIR}/arch/include" "${NEXUS_SOURCE_DIR}/io/include")
target_include_directories(nexus_stm32_usb_hal_time_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_usb_hal_time_test)
target_compile_definitions(nexus_stm32_usb_hal_time_test PRIVATE STM32F407xx)
