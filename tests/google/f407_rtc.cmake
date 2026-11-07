# Real F407 C11 RTC behavior; only external MMIO and CPU facts are modeled.
nexus_google_test(nexus_f407_rtc_test STM32F4_LL
    SOURCES f407_rtc_test.cpp f407_rtc_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/rtc.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/io/private"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_f407_rtc_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_f407_rtc_test)
target_compile_definitions(nexus_f407_rtc_test PRIVATE STM32F407xx
    nx_arch_is_privileged=nx_f407_rtc_privileged
    nx_arch_in_isr=nx_f407_rtc_in_isr
    nx_arch_irq_is_masked=nx_f407_rtc_masked
    nx_arch_dsb=nx_f407_rtc_dsb)
set_source_files_properties(
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/rtc.c" PROPERTIES
    COMPILE_OPTIONS "-include${NEXUS_SOURCE_DIR}/tests/google/f407_rtc_model.h")
