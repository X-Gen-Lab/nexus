# Selected uart sources and private dependencies.
nexus_controller_modes(uart_modes uart)
if(uart_modes)
    set(stm32_gpio_helpers ON)
    target_sources(nexus_soc_stm32f4 PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/uart.c"
        "${CMAKE_CURRENT_LIST_DIR}/uart_cold.c"
        "${CMAKE_CURRENT_LIST_DIR}/uart_hardware.c"
        "${CMAKE_CURRENT_LIST_DIR}/lifecycle_uart.c")
    if("irq-wire" IN_LIST uart_modes)
        target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/uart_wire.c")
    endif()
    if("irq-words" IN_LIST uart_modes)
        target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/uart_word.c")
    endif()
    if("dma-tx" IN_LIST uart_modes)
        set(stm32_uart_hal ON)
        target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/uart_hal.c")
        nexus_stm32f407_uart_hal(nexus_soc_stm32f4)
    endif()
    if("dma-duplex" IN_LIST uart_modes OR "dma-duplex-continuous" IN_LIST uart_modes)
        target_sources(nexus_soc_stm32f4 PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/uart_dma.c"
            "${CMAKE_CURRENT_LIST_DIR}/uart_duplex.c")
    endif()
    if("rx-dma-idle" IN_LIST uart_modes OR "rx-dma-continuous" IN_LIST uart_modes
            OR "dma-duplex" IN_LIST uart_modes OR "dma-duplex-continuous" IN_LIST uart_modes)
        target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/uart_receive.c")
    endif()
    if("irq-blocks" IN_LIST uart_modes)
        target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/uart_stream.c")
    endif()
endif()
