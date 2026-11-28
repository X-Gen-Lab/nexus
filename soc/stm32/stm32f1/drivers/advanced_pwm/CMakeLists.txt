# Select the reviewed complementary pair only when requested.
if("complementary-pwm" IN_LIST f1_advanced_pwm OR f1_pwm_adc)
    target_sources(nexus_soc_stm32f1 PRIVATE advanced_pwm.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOAdvancedPWM)
endif()
