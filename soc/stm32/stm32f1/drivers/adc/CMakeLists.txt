# Select the reviewed F103 regular acquisition implementation.
# Author: Nexus Team
nexus_controller_modes(adc_modes adc)
if("adc/ADC3/single-shot" IN_LIST NEXUS_SELECTED_CONTROLLERS)
    target_compile_definitions(nexus_soc_stm32f1 PRIVATE NX_STM32F1_ADC3_POLL=1)
endif()
if(
    "single-shot" IN_LIST adc_modes
    OR "single-trigger-dma" IN_LIST adc_modes
    OR "sequential-scan-dma" IN_LIST adc_modes
)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/adc.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOADC)
endif()
if("sequential-scan-dma" IN_LIST adc_modes)
    target_compile_definitions(
        nexus_soc_stm32f1
        PRIVATE NX_STM32F1_ADC_SCAN_DMA=1
    )
endif()
if(
    "single-trigger-dma" IN_LIST adc_modes
    OR "sequential-scan-dma" IN_LIST adc_modes
)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/trigger_dma.c"
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IODMA)
endif()
if("simultaneous-scan-dma" IN_LIST adc_modes)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/dual_dma.c"
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOADC Nexus::IODMA)
endif()
if("internal-scan" IN_LIST adc_modes)
    if(
        NOT NEXUS_EXACT_PART STREQUAL "STM32F103RBT6"
        OR NOT NEXUS_BACKEND STREQUAL "baremetal"
    )
        message(FATAL_ERROR "Internal ADC pair requires STM32F103RB baremetal")
    endif()
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/internal.c"
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOADC)
endif()
