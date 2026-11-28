function(nexus_f1_advanced_pwm_model target device)
    nexus_google_test(
        ${target}
        SOURCES
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/advanced_pwm_test.cpp"
            "${f1_dir}/drivers/advanced_pwm/advanced_pwm.c"
            "${f1_dir}/drivers/gpio/gpio.c"
        INCLUDES
            "${f1_dir}"
            "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/io/private"
        LIBRARIES Nexus::IOGPIO Nexus::IOAdvancedPWM
    )
    nexus_use_cmsis_core(${target})
    target_include_directories(
        ${target}
        SYSTEM
        PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
            "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
    )
    target_compile_definitions(${target} PRIVATE ${device})
    target_compile_options(
        ${target}
        PRIVATE "-include${CMAKE_CURRENT_FUNCTION_LIST_DIR}/model.h"
    )
endfunction()

nexus_f1_advanced_pwm_model(nexus_stm32f1_advanced_pwm_test STM32F103xB)
nexus_f1_advanced_pwm_model(nexus_stm32f1_tim8_pwm_test STM32F103xE)

nexus_f1_advanced_pwm_model(nexus_stm32f1_pwm_adc_test STM32F103xB)
target_sources(
    nexus_stm32f1_pwm_adc_test
    PRIVATE "${f1_dir}/drivers/pwm_adc/cycle.c"
)
target_compile_definitions(
    nexus_stm32f1_pwm_adc_test
    PRIVATE TEST_F1_CYCLE=1 NX_STM32F1_PWM_ADC_OWNER=1
)
target_link_libraries(nexus_stm32f1_pwm_adc_test PRIVATE Nexus::IOPWMADC)
