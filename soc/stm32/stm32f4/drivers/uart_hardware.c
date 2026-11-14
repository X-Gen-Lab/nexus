/**
 * \file            uart_hardware.c
 * \brief           Immutable F4 UART controller and default DMA integration
 * facts \author          Nexus Team \version         1.0.0 \date 2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_uart_hardware.h"

/* Controller identities do not prescribe a PCB pad or DMA stream. Empty
 * slots preserve stable peripheral ordinals when the selected SDK omits IP. */
const nx_stm32_uart_hardware_t nx_stm32_uart_hardware[6] = {
    {USART1, NEXUS_APB2_HZ, RCC_APB2ENR_USART1EN, USART1_IRQn, 7U, true, DMA2},
    {USART2, NEXUS_APB1_HZ, RCC_APB1ENR_USART2EN, USART2_IRQn, 7U, false, DMA1},
#ifdef USART3
    {USART3, NEXUS_APB1_HZ, RCC_APB1ENR_USART3EN, USART3_IRQn, 7U, false, DMA1},
#else
    {0},
#endif
#ifdef UART4
    {UART4, NEXUS_APB1_HZ, RCC_APB1ENR_UART4EN, UART4_IRQn, 8U, false, DMA1},
#else
    {0},
#endif
#ifdef UART5
    {UART5, NEXUS_APB1_HZ, RCC_APB1ENR_UART5EN, UART5_IRQn, 8U, false, DMA1},
#else
    {0},
#endif
    {USART6, NEXUS_APB2_HZ, RCC_APB2ENR_USART6EN, USART6_IRQn, 8U, true, DMA2},
};
