/**
 * \file            stm32f407_uart_hardware.h
 * \brief           Reviewed F4 UART controllers, clocks and DMA routes
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_UART_HARDWARE_H
#define NX_STM32F407_UART_HARDWARE_H

#include "stm32f4_dma_route.h"
#include "stm32f4_uart.h"

/** \brief Immutable hardware facts shared by cold admission and DMA owners. */
typedef struct {
    USART_TypeDef* registers;
    uint32_t clock_hz;
    uint32_t gate;
    IRQn_Type irq;
    uint8_t alternate;
    bool apb2;
    DMA_TypeDef* dma;
} nx_stm32_uart_hardware_t;

/** \brief Validate the F4 UART request selector without selecting a lane.
 * \note RM0090 Tables43/44 use CH4, USART6 uses CH5; USART3 TX also has
 *       stream4/CH7. Exact route availability remains a SoC/Board fact.
 */
static inline bool
nx_stm32_uart_dma_connection_valid(const nx_stm32_uart_hardware_t* hardware,
                                   const nx_stm32_dma_route_t* selected,
                                   bool receive) {
    if (hardware == NULL || !nx_stm32_dma_route_valid(selected))
        return false;
    uint8_t channel = hardware->irq == USART6_IRQn ? 5U : 4U;
    bool alternate = false;
#ifdef USART3
    alternate = !receive && hardware->irq == USART3_IRQn &&
                selected->lane == 4U && selected->channel == 7U;
#else
    (void)receive;
#endif
    if (selected->channel != channel && !alternate)
        return false;
#ifndef NEXUS_STM32_MODEL
    return selected->dma == hardware->dma;
#else
    return true;
#endif
}

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Peripheral identities and bus gates; routes come from static wiring.
 */
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
