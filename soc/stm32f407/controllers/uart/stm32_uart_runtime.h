#ifndef STM32_UART_RUNTIME_H
#define STM32_UART_RUNTIME_H
#include "stm32_uart_types.h"
#include "stm32_uart_resource.h"
void uart_init_operations(stm32_uart_impl_t* impl);
void stm32_uart_tx_completed(stm32_uart_impl_t* impl);
void stm32_uart_rx_event(stm32_uart_impl_t* impl, bool has_data,
                          uint8_t data, nx_status_t status, uint32_t raw_error);
nx_status_t stm32_uart_arm_rx(stm32_uart_impl_t* impl);
nx_status_t uart_register_isr(stm32_uart_impl_t* impl);
nx_status_t uart_unregister_isr(stm32_uart_impl_t* impl);
#endif
