# Exercise production fixed PWM ownership with a register model.
# Author: Nexus Team
set(f1_pwm_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
function(nexus_f1_pwm_models target device source)
    nexus_google_test(
        ${target}
        SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/${source}"
            "${f1_pwm_dir}/drivers/gpio/gpio.c"
            "${f1_pwm_dir}/drivers/pwm/pwm.c"
        INCLUDES
            "${f1_pwm_dir}"
            "${f1_pwm_dir}/drivers/pwm"
            "${NEXUS_SOURCE_DIR}/io/private"
        LIBRARIES Nexus::Arch Nexus::Core Nexus::IOTimer Nexus::IOGPIO
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
        PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/pwm_model.h"
    )
endfunction()

nexus_f1_pwm_models(stm32f103_pwm_contracts STM32F103xB pwm_test.cpp)
nexus_f1_pwm_models(stm32f100_pwm_contracts STM32F100xB valueline_test.cpp)
