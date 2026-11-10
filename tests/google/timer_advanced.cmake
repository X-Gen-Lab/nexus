# Exact production SDK providers execute against explicit external CPU/MMIO
# boundaries. Firmware stays C11 and never depends on GoogleTest.
foreach(timer_family stm32f4)
    set(timer_target "nexus_${timer_family}_timer_advanced_test")
    nexus_google_test(${timer_target} STM32F4_LL SOURCES timer_advanced_test.cpp
        "${NEXUS_SOC_SOURCE_${timer_family}}/drivers/timer_capture.c"
        "${NEXUS_SOC_SOURCE_${timer_family}}/drivers/timer_capture_tim3.c"
        "${NEXUS_SOC_SOURCE_${timer_family}}/drivers/timer_capture_tim5.c"
        "${NEXUS_SOC_SOURCE_${timer_family}}/drivers/timer_capture_tim1.c"
        "${NEXUS_SOC_SOURCE_${timer_family}}/drivers/timer_capture_tim8.c"
        "${NEXUS_SOC_SOURCE_${timer_family}}/drivers/gpio.c"
        "${NEXUS_SOC_SOURCE_${timer_family}}/drivers/advanced_pwm.c"
        "${NEXUS_SOC_SOURCE_${timer_family}}/drivers/advanced_pwm_tim8.c"
        "${NEXUS_SOURCE_DIR}/io/src/api/timer_capture.c"
        "${NEXUS_SOURCE_DIR}/io/src/api/advanced_pwm.c"
        LIBRARIES Nexus::Core
        INCLUDES "${NEXUS_SOC_SOURCE_${timer_family}}/private"
            "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/io/include"
            "${NEXUS_SOURCE_DIR}/io/private")
    target_compile_options(${timer_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/timer_advanced_model.h")
    target_include_directories(${timer_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${timer_target})
    target_compile_definitions(${timer_target} PRIVATE STM32F407xx)
    foreach(small_timer 9 10 11 12 13 14)
        target_sources(${timer_target} PRIVATE
            "${NEXUS_SOC_SOURCE_${timer_family}}/drivers/timer_capture_tim${small_timer}.c")
    endforeach()
endforeach()
