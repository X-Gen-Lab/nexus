/**
 * \file            atomic_vendor_port.c
 * \brief           Execute original vendor LL C bodies through the port
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4xx_ll_usart.h"

void nx_atomic_vendor_enable_tx(USART_TypeDef* uart) {
    LL_USART_EnableDMAReq_TX(uart);
}

void nx_atomic_vendor_disable_tx(USART_TypeDef* uart) {
    LL_USART_DisableDMAReq_TX(uart);
}
