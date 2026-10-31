/**
 * \file            u575_uart_leaf_port.c
 * \brief           Execute locked U575 LL UART word accesses from host C11
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u5xx_ll_usart.h"
#include "u575_uart_leaf_model.h"
void nx_u575_uart_official_write(USART_TypeDef* registers, uint8_t value) {
    LL_USART_TransmitData8(registers, value);
}
uint8_t nx_u575_uart_official_read(USART_TypeDef* registers) {
    return LL_USART_ReceiveData8(registers);
}
