add_library(board_stm32f4discovery INTERFACE)
target_include_directories(board_stm32f4discovery INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(board_stm32f4discovery INTERFACE nexus_build_options)

set(board_wiring_sources)
if(CONFIG_STM32_SPI_ENABLE)
    list(APPEND board_wiring_sources spi.c)
endif()
if(CONFIG_STM32_UART_ENABLE)
    list(APPEND board_wiring_sources uart.c)
endif()
if(board_wiring_sources)
    add_library(board_stm32f4discovery_wiring OBJECT ${board_wiring_sources})
    target_include_directories(board_stm32f4discovery_wiring PRIVATE
        "${NEXUS_SOURCE_DIR}/platforms/stm32/src/hal/spi"
        "${NEXUS_SOURCE_DIR}/platforms/stm32/src/hal/uart")
    target_link_libraries(board_stm32f4discovery_wiring PRIVATE
        board_stm32f4discovery sdk_stm32f4 hal osal)
endif()
