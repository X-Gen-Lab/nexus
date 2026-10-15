# Real provider isolation, including the normal STM32 NVIC path. Only external
# CPU/MMIO behavior is modeled; firmware and SDK consumers never enter here.
foreach(i2c_multi_family stm32f407 gd32f470)
    set(i2c_multi_target "nexus_${i2c_multi_family}_i2c_multi_controller_test")
    nexus_google_test(${i2c_multi_target} SOURCES i2c_multi_controller_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/common/i2c_v1_async.c"
        "${NEXUS_SOURCE_DIR}/soc/${i2c_multi_family}/drivers/i2c_async.c"
        LIBRARIES Nexus::IO
        INCLUDES "${NEXUS_SOURCE_DIR}/tests/contracts/multi_controller"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
            "${NEXUS_SOURCE_DIR}/soc/common"
            "${NEXUS_SOURCE_DIR}/soc/${i2c_multi_family}/private"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_compile_options(${i2c_multi_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_multi_controller_model.h")
    if(i2c_multi_family STREQUAL "stm32f407")
        target_include_directories(${i2c_multi_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
        target_compile_definitions(${i2c_multi_target} PRIVATE
            STM32F407xx NX_I2C_MULTI_STM32)
    else()
        set(i2c_multi_vendor
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
        target_include_directories(${i2c_multi_target} SYSTEM PRIVATE
            "${i2c_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
            "${i2c_multi_vendor}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${i2c_multi_target} PRIVATE
            GD32F470 HXTAL_VALUE=25000000)
        target_compile_options(${i2c_multi_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    endif()
endforeach()
