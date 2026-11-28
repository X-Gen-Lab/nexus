# Select one coupled complementary PWM and injected ADC owner on the exact part.
if("finite-injected" IN_LIST f1_pwm_adc)
    if(
        NOT NEXUS_EXACT_PART STREQUAL "STM32F103RBT6"
        OR NOT NEXUS_BACKEND STREQUAL "baremetal"
    )
        message(FATAL_ERROR "Finite injected PWM/ADC requires F103RB baremetal")
    endif()
    target_sources(nexus_soc_stm32f1 PRIVATE cycle.c)
    target_compile_definitions(
        nexus_soc_stm32f1
        PRIVATE NX_STM32F1_PWM_ADC_OWNER=1
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOPWMADC)
endif()
