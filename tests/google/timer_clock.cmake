# Resolve real authored plans; each variant retains only its distinct clocks.
foreach(timer_clock pll84 hsi16)
    set(timer_clock_input "${NEXUS_SOURCE_DIR}/tests/contracts/timer_clock/${timer_clock}.toml")
    set(timer_clock_bundle "${CMAKE_CURRENT_BINARY_DIR}/timer-clock-${timer_clock}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${timer_clock_input}"
        --output "${timer_clock_bundle}" RESULT_VARIABLE timer_clock_result
        OUTPUT_VARIABLE timer_clock_output ERROR_VARIABLE timer_clock_error)
    if(NOT timer_clock_result EQUAL 0)
        message(FATAL_ERROR "Timer clock fixture rejected: ${timer_clock_output}${timer_clock_error}")
    endif()
    nexus_google_configuration_inputs("${timer_clock_bundle}")
    set(timer_clock_target "nexus_stm32_timer_clock_${timer_clock}_test")
    nexus_google_test(${timer_clock_target} STM32F4_LL
        SOURCES timer_clock_test.cpp
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/timer_capture.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/timer_capture_tim3.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/timer_capture_tim5.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/timer_capture_tim1.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/timer_capture_tim8.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/advanced_pwm.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/advanced_pwm_tim8.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/encoder.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
        LIBRARIES Nexus::Core
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/io/include"
            "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${timer_clock_target} BEFORE PRIVATE "${timer_clock_bundle}")
    target_include_directories(${timer_clock_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core(${timer_clock_target})
    target_compile_definitions(${timer_clock_target} PRIVATE STM32F407xx)
    target_compile_options(${timer_clock_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/timer_clock_model.h")
    foreach(small_timer 9 10 11 12 13 14)
        target_sources(${timer_clock_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/timer_capture_tim${small_timer}.c")
    endforeach()
endforeach()
