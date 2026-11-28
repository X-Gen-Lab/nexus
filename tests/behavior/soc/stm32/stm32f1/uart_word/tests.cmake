# Exercise the exact F103 USART2 nine-bit transport owner.
# Author: Nexus Team
set(f1_uart_word_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_uart_word_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/uart_word_test.cpp"
        "${f1_uart_word_dir}/drivers/gpio/gpio.c"
        "${f1_uart_word_dir}/drivers/uart/uart_word.c"
    INCLUDES "${f1_uart_word_dir}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOGPIO Nexus::IOUARTWord
)
nexus_use_cmsis_core(stm32f103_uart_word_contracts)
target_include_directories(
    stm32f103_uart_word_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(stm32f103_uart_word_contracts PRIVATE STM32F103xB)
target_compile_options(
    stm32f103_uart_word_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
)
