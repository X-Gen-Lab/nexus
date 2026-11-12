# Optional integer layout tools do not enlarge existing ADC device state.
nexus_google_test(nexus_adc_format_test SOURCES adc_format_test.cpp adc_group_test.cpp
    LIBRARIES Nexus::IO)
nexus_google_test(nexus_stm32_adc_group_test STM32F4_LL
    SOURCES stm32_adc_group_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_group.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_group_blocks.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_group_automatic.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_trigger.c"
    LIBRARIES Nexus::NativeModel Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_adc_group_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_adc_group_test)
target_compile_definitions(nexus_stm32_adc_group_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)
target_compile_options(nexus_stm32_adc_group_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_adc_group_model.h")
include("${CMAKE_CURRENT_LIST_DIR}/stm32_adc_group_factory.cmake")
