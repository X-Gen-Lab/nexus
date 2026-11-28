# Select the reviewed F103 ADC1 regular acquisition implementation.
# Author: Nexus Team
nexus_controller_modes(adc_modes adc)
if("single-shot" IN_LIST adc_modes OR "single-trigger-dma" IN_LIST adc_modes)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/adc.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOADC)
endif()
if("single-trigger-dma" IN_LIST adc_modes)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/trigger_dma.c"
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IODMA)
endif()
