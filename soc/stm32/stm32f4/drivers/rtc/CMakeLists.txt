# Selected rtc sources and private dependencies.
nexus_controller_modes(rtc_modes rtc)
if(rtc_modes)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/rtc.c")
endif()
if(NEXUS_RTC_PERIODIC_WAKEUP)
    target_compile_definitions(nexus_soc_stm32f4 PRIVATE NX_STM32_RTC_PERIODIC_WAKEUP=1)
endif()
