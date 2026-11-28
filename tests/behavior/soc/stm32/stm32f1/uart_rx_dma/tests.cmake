# Run F103 finite USART RX against the side-effectful DMA/USART model.
# Author: Nexus Team
set(f1_uart_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_uart_rx_dma_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/uart_rx_dma_test.cpp"
        "${f1_uart_dir}/drivers/gpio/gpio.c"
        "${f1_uart_dir}/drivers/uart/uart.c"
        "${f1_uart_dir}/drivers/uart/uart_rx_dma.c"
        "${f1_uart_dir}/drivers/uart/uart_dma.c"
        "${f1_uart_dir}/drivers/uart/uart_duplex.c"
    INCLUDES
        "${f1_uart_dir}"
        "${f1_uart_dir}/drivers/uart"
        "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOUART Nexus::IOStream
)
nexus_use_cmsis_core(stm32f103_uart_rx_dma_contracts)
target_include_directories(
    stm32f103_uart_rx_dma_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(
    stm32f103_uart_rx_dma_contracts
    PRIVATE STM32F103xB NEXUS_STM32F1_UART_DUPLEX=1
)
target_compile_options(
    stm32f103_uart_rx_dma_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
)
