# Selected clock implementations execute against the pinned SDK register
# layouts. Only the external MMIO and CPU/time boundaries are modeled.
foreach(clock_family stm32f407 gd32f470)
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
    if(clock_family STREQUAL "stm32f407")
        target_include_directories(${clock_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
        target_compile_definitions(${clock_target} PRIVATE STM32F407xx)
    else()
        set(clock_vendor "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
        target_include_directories(${clock_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_include_directories(${clock_target} SYSTEM PRIVATE
            "${clock_vendor}/CMSIS/GD/GD32F4xx/Include"
            "${clock_vendor}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${clock_target} PRIVATE
            GD32F470 HXTAL_VALUE=25000000 NX_USB_GD_MODEL)
        target_compile_options(${clock_target} PRIVATE -Wno-int-to-pointer-cast)
    endif()
endforeach()
