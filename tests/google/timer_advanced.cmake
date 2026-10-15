# Exact production SDK providers execute against explicit external CPU/MMIO
# boundaries. Firmware stays C11 and never depends on GoogleTest.
foreach(timer_family stm32f407 gd32f470)
    set(timer_target "nexus_${timer_family}_timer_advanced_test")
    nexus_google_test(${timer_target} SOURCES timer_advanced_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/${timer_family}/drivers/timer_capture.c"
        "${NEXUS_SOURCE_DIR}/soc/${timer_family}/drivers/advanced_pwm.c"
        "${NEXUS_SOURCE_DIR}/io/src/timer_capture.c"
        "${NEXUS_SOURCE_DIR}/io/src/advanced_pwm.c"
        LIBRARIES Nexus::Core
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/${timer_family}/private"
            "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/io/include"
            "${NEXUS_SOURCE_DIR}/io/private")
    target_compile_options(${timer_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/timer_advanced_model.h")
    if(timer_family STREQUAL "stm32f407")
        target_include_directories(${timer_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
        target_compile_definitions(${timer_target} PRIVATE STM32F407xx)
    else()
        set(timer_vendor "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
        target_include_directories(${timer_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_include_directories(${timer_target} SYSTEM PRIVATE
            "${timer_vendor}/CMSIS/GD/GD32F4xx/Include"
            "${timer_vendor}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${timer_target} PRIVATE
            GD32F470 HXTAL_VALUE=25000000 NX_TIMER_GD_MODEL)
        target_compile_options(${timer_target} PRIVATE -Wno-int-to-pointer-cast)
    endif()
endforeach()
