# Select the reviewed TIM1 complementary pair only when requested.
if("complementary-pwm" IN_LIST f1_advanced_pwm)
    target_sources(nexus_soc_stm32f1 PRIVATE advanced_pwm.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOAdvancedPWM)
endif()
