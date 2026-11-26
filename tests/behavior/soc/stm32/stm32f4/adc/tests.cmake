# Explicit adc behavior and private model dependencies.
# Author: Nexus Team
include(
    "${nexus_google_root}/soc/stm32/stm32f4/adc/stm32_adc_cold_factory.cmake"
)

include("${nexus_google_root}/soc/stm32/stm32f4/adc/adc_control.cmake")

nexus_google_test(nexus_stm32_adc_stream_test STM32F4_LL
    SOURCES "${nexus_google_root}/soc/stm32/stm32f4/adc/stm32_adc_stream_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc/adc.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
)

target_include_directories(
    nexus_stm32_adc_stream_test
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}"
)

nexus_use_cmsis_core(nexus_stm32_adc_stream_test)

target_compile_definitions(
    nexus_stm32_adc_stream_test
    PRIVATE NEXUS_STM32_MODEL STM32F407xx
)

target_compile_options(
    nexus_stm32_adc_stream_test
    PRIVATE
        "-include${nexus_google_root}/soc/stm32/stm32f4/adc/stm32_adc_stream_model.h"
)

target_sources(
    nexus_stm32_adc_stream_test
    PRIVATE
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc/adc_stream.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc/adc_trigger.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc/adc_continuous.c"
)
