# Selected adc sources and private dependencies.
nexus_controller_modes(adc_modes adc)
if(adc_modes)
    set(stm32_gpio_helpers ON)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/adc.c")
    if("injected-shot" IN_LIST adc_modes)
        target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/adc_injected.c")
    endif()
    if("trigger-dma" IN_LIST adc_modes OR "trigger-dma-continuous" IN_LIST adc_modes)
        target_sources(nexus_soc_stm32f4 PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/adc_stream.c" "${CMAKE_CURRENT_LIST_DIR}/adc_trigger.c")
    endif()
    if("trigger-dma-continuous" IN_LIST adc_modes)
        target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/adc_continuous.c")
    endif()
endif()
nexus_controller_modes(adc_group_modes adc_group)
if(adc_group_modes)
    set(stm32_gpio_helpers ON)
    target_link_libraries(nexus_soc_stm32f4 PRIVATE Nexus::IODMA)
    target_sources(nexus_soc_stm32f4 PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/adc_group.c" "${CMAKE_CURRENT_LIST_DIR}/adc_trigger.c")
    if("regular-blocks" IN_LIST adc_group_modes
            OR "regular-software-injected-blocks" IN_LIST adc_group_modes
            OR "regular-timer-injected-blocks" IN_LIST adc_group_modes
            OR "regular-auto-injected" IN_LIST adc_group_modes)
        target_link_libraries(nexus_soc_stm32f4 PRIVATE Nexus::IOStream)
        target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/adc_group_blocks.c")
    endif()
    if("regular-auto-injected" IN_LIST adc_group_modes)
        target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/adc_group_automatic.c")
    endif()
endif()
