# Granted converter leaf; complete-cycle state remains in its owning domain.
# Author: Nexus Team
nexus_google_test(nexus_f407_single_shunt_test STM32F4_LL
    SOURCES "${nexus_google_root}/soc/stm32/stm32f4/pwm_adc/single_shunt_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/pwm_adc/single_shunt.c"
    LIBRARIES Nexus::IOPowerStage Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
        "${nexus_google_root}/soc/stm32/stm32f4/pwm_adc"
)
target_include_directories(
    nexus_f407_single_shunt_test
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
)
nexus_use_cmsis_core(nexus_f407_single_shunt_test)
target_compile_definitions(
    nexus_f407_single_shunt_test
    PRIVATE STM32F407xx NEXUS_STM32_MODEL=1
)
target_compile_options(
    nexus_f407_single_shunt_test
    PRIVATE
        "-include${nexus_google_root}/soc/stm32/stm32f4/audio_video/f407_capture_audio_model.h"
)
