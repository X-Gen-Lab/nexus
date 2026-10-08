add_library(board_stm32f4discovery INTERFACE)
target_include_directories(board_stm32f4discovery INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(board_stm32f4discovery INTERFACE nexus_build_options)

if(CONFIG_STM32_SPI_ENABLE)
    add_library(board_stm32f4discovery_wiring OBJECT spi.c)
    target_include_directories(board_stm32f4discovery_wiring PRIVATE
        "${NEXUS_SOURCE_DIR}/platforms/stm32/src/hal/spi")
    target_link_libraries(board_stm32f4discovery_wiring PRIVATE
        board_stm32f4discovery sdk_stm32f4 hal osal)
endif()
