# One selected coupled transport owns its timer, converter and DMA route.
# Author: Nexus Team
nexus_controller_modes(pwm_adc_modes pwm_adc)
if(pwm_adc_modes)
    nexus_stm32f4_gpio(nexus_soc_stm32f4)
    target_link_libraries(
        nexus_soc_stm32f4
        PRIVATE Nexus::IOPWMGroup Nexus::IODMA
    )
    target_sources(
        nexus_soc_stm32f4
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/analog.c"
            "${CMAKE_CURRENT_LIST_DIR}/../timer/timer_waveform.c"
            "${CMAKE_CURRENT_LIST_DIR}/../ip/av/av.c"
    )
    if("finite-regular-dma" IN_LIST pwm_adc_modes)
        target_link_libraries(nexus_soc_stm32f4 PRIVATE Nexus::IOPWMADC)
        target_sources(
            nexus_soc_stm32f4
            PRIVATE "${CMAKE_CURRENT_LIST_DIR}/pwm_adc.c"
        )
    endif()
    if("injected-control" IN_LIST pwm_adc_modes)
        target_link_libraries(nexus_soc_stm32f4 PRIVATE Nexus::IOPWMADCLoop)
        target_sources(
            nexus_soc_stm32f4
            PRIVATE "${CMAKE_CURRENT_LIST_DIR}/loop.c"
        )
    endif()
    if("injected-power-stage" IN_LIST pwm_adc_modes)
        target_link_libraries(nexus_soc_stm32f4 PRIVATE Nexus::IOPowerStage)
        target_sources(
            nexus_soc_stm32f4
            PRIVATE "${CMAKE_CURRENT_LIST_DIR}/power_stage.c"
        )
    endif()
endif()
