# Exercise the exact F103 USART2 Smartcard owner.
# Author: Nexus Team
set(f1_uart_smartcard_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_uart_smartcard_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/smartcard_test.cpp"
        "${f1_uart_smartcard_dir}/drivers/gpio/gpio.c"
        "${f1_uart_smartcard_dir}/drivers/uart/uart.c"
    INCLUDES "${f1_uart_smartcard_dir}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOGPIO Nexus::IOUART
)
nexus_use_cmsis_core(stm32f103_uart_smartcard_contracts)
target_include_directories(
    stm32f103_uart_smartcard_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(
    stm32f103_uart_smartcard_contracts
    PRIVATE STM32F103xB "NEXUS_STM32F1_UART_WIRE=(1U << NX_UART_WIRE_SMARTCARD)"
)
target_compile_options(
    stm32f103_uart_smartcard_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
)
