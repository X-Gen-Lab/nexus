# Exercise production fixed PWM ownership with a register model.
# Author: Nexus Team
set(f1_pwm_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_pwm_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/pwm_test.cpp"
        "${f1_pwm_dir}/drivers/gpio/gpio.c"
        "${f1_pwm_dir}/drivers/pwm/pwm.c"
    INCLUDES
        "${f1_pwm_dir}"
        "${f1_pwm_dir}/drivers/pwm"
        "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOTimer Nexus::IOGPIO
)
nexus_use_cmsis_core(stm32f103_pwm_contracts)
target_include_directories(
    stm32f103_pwm_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(stm32f103_pwm_contracts PRIVATE STM32F103xB)
target_compile_options(
    stm32f103_pwm_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/pwm_model.h"
)
