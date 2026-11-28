# Compile the selected SPI owner only when the application selects it.
# Author: Nexus Team
if("short-poll" IN_LIST f1_spi OR "dma" IN_LIST f1_spi)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/spi.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOSPI Nexus::IOGPIO)
endif()
if("dma" IN_LIST f1_spi)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/spi_dma.c"
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IODMA)
    file(READ "${NEXUS_CONFIG_DIR}/resolved.json" f1_spi_selection)
    string(JSON count LENGTH "${f1_spi_selection}" controllers)
    math(EXPR last "${count} - 1")
    foreach(index RANGE "${last}")
        string(
            JSON face
            ERROR_VARIABLE face_error
            GET "${f1_spi_selection}"
            controllers
            "${index}"
            interface_kind
        )
        if(face STREQUAL "spi_dma_transaction")
            target_compile_definitions(
                nexus_soc_stm32f1
                PRIVATE NX_STM32F1_SPI_DMA_SEQUENCE=1
            )
            break()
        endif()
    endforeach()
endif()
