# Real finite coupled owner; the model replaces only physical CPU/MMIO leaves.
# Author: Nexus Team
nexus_google_test(nexus_f407_pwm_adc_test STM32F4_LL
    SOURCES "${nexus_google_root}/soc/stm32/stm32f4/pwm_adc/f407_pwm_adc_test.cpp"
        "${nexus_google_root}/io/pwm_adc_loop_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/pwm_adc/pwm_adc.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/pwm_adc/analog.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/pwm_adc/loop.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/av/av.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/timer/timer_waveform.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
        "${nexus_google_root}/soc/stm32/stm32f4/pwm_adc"
)
target_include_directories(
    nexus_f407_pwm_adc_test
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
)
nexus_use_cmsis_core(nexus_f407_pwm_adc_test)
target_compile_definitions(
    nexus_f407_pwm_adc_test
    PRIVATE STM32F407xx NEXUS_STM32_MODEL=1
)
target_compile_options(
    nexus_f407_pwm_adc_test
    PRIVATE
        "-include${nexus_google_root}/soc/stm32/stm32f4/timer/f407_timer_waveform_model.h"
        -ffunction-sections
        -fdata-sections
)
target_link_options(nexus_f407_pwm_adc_test PRIVATE -Wl,--gc-sections)
