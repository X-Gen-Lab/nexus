/**
 * \file            h743_uart_leaf_port.c
 * \brief           Execute pinned H743 UART word leaves
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_uart_leaf_model.h"
#include "stm32h7xx_ll_usart.h"
void nx_h743_uart_official_write(USART_TypeDef* registers, uint8_t value) {
    LL_USART_TransmitData8(registers, value);
}
uint8_t nx_h743_uart_official_read(USART_TypeDef* registers) {
    return LL_USART_ReceiveData8(registers);
}
