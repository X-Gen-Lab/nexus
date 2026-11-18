# Selected dac sources and private dependencies.
nexus_controller_modes(dac_modes dac)
if(dac_modes)
    nexus_stm32f4_gpio(nexus_soc_stm32f4)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/dac.c")
    if(
        "trigger-dma" IN_LIST dac_modes
        OR "trigger-dma-circular" IN_LIST dac_modes
    )
        target_sources(
            nexus_soc_stm32f4
            PRIVATE "${CMAKE_CURRENT_LIST_DIR}/dac_dma.c"
        )
    endif()
endif()
