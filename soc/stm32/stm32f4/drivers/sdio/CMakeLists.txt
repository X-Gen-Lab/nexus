# Selected sdio sources and private dependencies.
nexus_controller_modes(block_modes block)
if(block_modes)
    set(stm32_gpio_helpers ON)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/sdio.c")
    nexus_stm32f407_sdio_ll(nexus_soc_stm32f4)
endif()
