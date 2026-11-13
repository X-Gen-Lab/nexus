/**
 * \file            stm32f407_uart_hardware.h
 * \brief           Reviewed F407 UART pads, clocks and fixed DMA routes
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_UART_HARDWARE_H
#define NX_STM32F407_UART_HARDWARE_H

#include "stm32f4_uart.h"

#ifdef NEXUS_STM32_MODEL
extern GPIO_TypeDef g_nx_stm32_gpioa_model;
#undef GPIOA
#define GPIOA (&g_nx_stm32_gpioa_model)
#endif

/** \brief Static DMA transmit route; completion facts remain in the owner. */
typedef struct {
    DMA_Stream_TypeDef* registers;
    IRQn_Type irq;
    uint8_t stream;
    uint8_t channel;
} nx_stm32_uart_tx_cold_config_t;

/** \brief Immutable hardware facts shared by cold admission and DMA owners. */
typedef struct {
    USART_TypeDef* registers;
    GPIO_TypeDef* tx_gpio;
    GPIO_TypeDef* rx_gpio;
    uint32_t clock_hz;
    uint32_t gate;
    IRQn_Type irq;
    uint8_t tx_port;
    uint8_t tx_pin;
    uint8_t rx_port;
    uint8_t rx_pin;
    uint8_t alternate;
    bool apb2;
    DMA_TypeDef* dma;
    nx_stm32_uart_tx_cold_config_t transmit;
    DMA_Stream_TypeDef* receive;
    IRQn_Type receive_irq;
    uint8_t receive_shift;
    bool receive_high;
} nx_stm32_uart_hardware_t;

#ifdef __cplusplus
extern "C" {
#endif
/** \brief RM0090 Tables 43/44 routes and DS8626 Table 9 pad pairs. */
extern const nx_stm32_uart_hardware_t nx_stm32_uart_hardware[6];
#ifdef __cplusplus
}
#endif

/** \brief Select one reviewed publisher without a mutable instance lookup. */
static inline const nx_stm32_uart_hardware_t*
nx_stm32_uart_hardware_for_irq(IRQn_Type irq) {
    switch (irq) {
        case USART1_IRQn:
            return &nx_stm32_uart_hardware[0];
        case USART2_IRQn:
            return &nx_stm32_uart_hardware[1];
#if defined(USART3)
        case USART3_IRQn:
            return &nx_stm32_uart_hardware[2];
#endif
#if defined(UART4)
        case UART4_IRQn:
            return &nx_stm32_uart_hardware[3];
#endif
#if defined(UART5)
        case UART5_IRQn:
            return &nx_stm32_uart_hardware[4];
#endif
        case USART6_IRQn:
            return &nx_stm32_uart_hardware[5];
        default:
            return NULL;
    }
}
#endif
