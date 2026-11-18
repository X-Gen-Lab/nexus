# Selected sdio sources and private dependencies.
nexus_controller_modes(block_modes block)
if(block_modes)
    nexus_stm32f4_gpio(nexus_soc_stm32f4)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/sdio.c")
    nexus_stm32f407_sdio_ll(nexus_soc_stm32f4)
endif()
