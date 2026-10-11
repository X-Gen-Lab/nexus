# Actual common controller and two production register specializations.
nexus_google_test(nexus_can_controller_test SOURCES can_controller_test.cpp
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/io/native"
        "${NEXUS_SOURCE_DIR}/io/private")
foreach(can_family stm32 gd32)
    set(can_target "nexus_${can_family}_can_test")
    nexus_google_test(${can_target} SOURCES can_soc_test.cpp
        LIBRARIES Nexus::NativeModel Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/io/native"
            "${NEXUS_SOURCE_DIR}/io/private")
    target_compile_options(${can_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/can_soc_model.h")
    if(can_family STREQUAL "stm32")
        target_sources(${can_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/can.c")
        target_include_directories(${can_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32f407/private")
        target_include_directories(${can_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
        target_compile_definitions(${can_target} PRIVATE
            NX_CAN_SOC_STM32 NEXUS_STM32_MODEL STM32F407xx)
    else()
        target_sources(${can_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/gd32f470/drivers/can.c")
        target_include_directories(${can_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/gd32f470/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_include_directories(${can_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware/CMSIS/GD/GD32F4xx/Include"
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${can_target} PRIVATE GD32F470
            HXTAL_VALUE=25000000)
        target_compile_options(${can_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    endif()
endforeach()
