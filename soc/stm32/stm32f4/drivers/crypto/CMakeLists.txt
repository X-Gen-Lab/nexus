# Selected crypto sources and private dependencies.
foreach(engine cipher hash)
    nexus_controller_modes(crypto_modes "${engine}")
    if(crypto_modes)
        target_sources(
            nexus_soc_stm32f4
            PRIVATE
                "${CMAKE_CURRENT_LIST_DIR}/${engine}.c"
                "${CMAKE_CURRENT_LIST_DIR}/crypto_irq.c"
        )
        if("dma-blocks" IN_LIST crypto_modes)
            target_sources(
                nexus_soc_stm32f4
                PRIVATE
                    "${CMAKE_CURRENT_LIST_DIR}/${engine}_dma.c"
                    "${CMAKE_CURRENT_LIST_DIR}/crypto_dma.c"
            )
        endif()
    endif()
endforeach()
if("cipher" IN_LIST NEXUS_SELECTED_KINDS OR "hash" IN_LIST NEXUS_SELECTED_KINDS)
    nexus_stm32f4_crypto_hal(nexus_soc_stm32f4)
endif()
