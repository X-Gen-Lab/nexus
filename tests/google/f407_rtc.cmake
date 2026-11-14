# Real C11 RTC behavior; only external MMIO and CPU facts are modeled.
foreach(rtc_part f407 f411 f446 f479)
    if(rtc_part STREQUAL "f407")
        set(rtc_target nexus_f407_rtc_test)
        set(rtc_macro STM32F407xx)
        set(rtc_test_source f407_rtc_test.cpp)
    elseif(rtc_part STREQUAL "f411")
        set(rtc_target nexus_stm32_f411_rtc_test)
        set(rtc_macro STM32F411xE)
        set(rtc_test_source stm32_family_rtc_test.cpp)
    elseif(rtc_part STREQUAL "f446")
        set(rtc_target nexus_stm32_f446_rtc_test)
        set(rtc_macro STM32F446xx)
        set(rtc_test_source stm32_family_rtc_test.cpp)
    else()
        set(rtc_target nexus_stm32_f479_rtc_test)
        set(rtc_macro STM32F479xx)
        set(rtc_test_source stm32_family_rtc_test.cpp)
    endif()
    nexus_google_test(${rtc_target} STM32F4_LL
        SOURCES ${rtc_test_source} f407_rtc_model.cpp
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/rtc.c"
        LIBRARIES Nexus::IO
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOURCE_DIR}/tests/google")
    target_include_directories(${rtc_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core(${rtc_target})
    target_compile_definitions(${rtc_target} PRIVATE ${rtc_macro}
        nx_arch_is_privileged=nx_f407_rtc_privileged
        nx_arch_in_isr=nx_f407_rtc_in_isr
        nx_arch_irq_is_masked=nx_f407_rtc_masked
        nx_arch_irq_masks=nx_f407_rtc_masks
        nx_arch_dsb=nx_f407_rtc_dsb)
    if(rtc_part STREQUAL "f407")
        target_compile_definitions(${rtc_target} PRIVATE
            NX_STM32_RTC_PERIODIC_WAKEUP=1)
    endif()
endforeach()
set_source_files_properties(
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/rtc.c" PROPERTIES
    COMPILE_OPTIONS "-include${NEXUS_SOURCE_DIR}/tests/google/f407_rtc_model.h")
