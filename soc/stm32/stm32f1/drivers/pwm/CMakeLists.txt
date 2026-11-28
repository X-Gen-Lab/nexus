# Compile the exclusive TIM3 CH1 owner for the selected fixed PWM mode.
# Author: Nexus Team
if("fixed-pwm" IN_LIST f1_pwm)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/pwm.c")
    target_link_libraries(
        nexus_soc_stm32f1
        PRIVATE Nexus::IOTimer Nexus::IOGPIO
    )
endif()
