# Selected spi sources and private dependencies.
nexus_controller_modes(spi_modes spi)
if(spi_modes)
    nexus_stm32f4_gpio(nexus_soc_stm32f4)
    if("short-poll" IN_LIST spi_modes OR "dma" IN_LIST spi_modes)
        target_sources(
            nexus_soc_stm32f4
            PRIVATE
                "${CMAKE_CURRENT_LIST_DIR}/spi.c"
                "${CMAKE_CURRENT_LIST_DIR}/lifecycle_spi.c"
        )
    endif()
    if("irq-frames" IN_LIST spi_modes)
        target_sources(
            nexus_soc_stm32f4
            PRIVATE
                "${CMAKE_CURRENT_LIST_DIR}/spi_frames.c"
                "${CMAKE_CURRENT_LIST_DIR}/spi_frames_cold.c"
        )
    endif()
    if("dma" IN_LIST spi_modes)
        target_link_libraries(nexus_soc_stm32f4 PRIVATE Nexus::IODMA)
        target_sources(
            nexus_soc_stm32f4
            PRIVATE "${CMAKE_CURRENT_LIST_DIR}/spi_dma.c"
        )
    endif()
endif()
