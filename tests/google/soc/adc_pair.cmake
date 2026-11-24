# Granted converter leaf; domain, clock and interrupt ownership stay external.
# Author: Nexus Team
nexus_google_test(nexus_f407_adc_pair_test STM32F4_LL
    SOURCES "${nexus_google_root}/soc/adc_pair_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/pwm_adc/adc_pair.c"
    LIBRARIES Nexus::IOPowerStage Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
        "${nexus_google_root}/soc"
)
target_include_directories(
    nexus_f407_adc_pair_test
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
)
nexus_use_cmsis_core(nexus_f407_adc_pair_test)
target_compile_definitions(
    nexus_f407_adc_pair_test
    PRIVATE STM32F407xx NEXUS_STM32_MODEL=1
)
target_compile_options(
    nexus_f407_adc_pair_test
    PRIVATE "-include${nexus_google_root}/soc/f407_capture_audio_model.h"
)
