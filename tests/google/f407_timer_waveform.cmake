# Real C11 timer algorithms; only CPU, MMIO, time and IRQ leaves are external.
# Author: Nexus Team
nexus_google_test(nexus_f407_timer_waveform_test STM32F4_LL
    SOURCES f407_timer_waveform_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/av.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/timer_waveform.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_f407_timer_waveform_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_f407_timer_waveform_test)
target_compile_definitions(nexus_f407_timer_waveform_test PRIVATE STM32F407xx
    NEXUS_STM32_MODEL=1)
target_compile_options(nexus_f407_timer_waveform_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/f407_timer_waveform_model.h"
    -ffunction-sections -fdata-sections)
target_link_options(nexus_f407_timer_waveform_test PRIVATE -Wl,--gc-sections)
