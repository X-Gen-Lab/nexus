# Selected graphics sources and private dependencies.
nexus_controller_modes(display_modes display)
nexus_controller_modes(image_modes image)
if(display_modes)
    nexus_stm32f4_gpio(nexus_soc_stm32f4)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ltdc.c")
endif()
if(image_modes)
    target_sources(
        nexus_soc_stm32f4
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/dma2d.c"
    )
endif()
if(display_modes OR image_modes)
    nexus_stm32f4_graphics_hal(nexus_soc_stm32f4)
endif()
