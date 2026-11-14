/**
 * \file            stream.h
 * \brief           Selected UART IRQ block storage and methods
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_DRIVERS_UART_STREAM_H
#define NEXUS_STM32F4_DRIVERS_UART_STREAM_H

#include "drivers/uart/instance.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One IRQ writer fills only the currently reserved caller block. */
typedef struct {
    nx_stm32_uart_state_t uart;
    nx_stream_t* stream;
    nx_stream_fill_t fill;
    size_t length;
    uint32_t block_flags;
    bool filling;
    bool stopped;
    bool loss_latched;
} nx_stm32_uart_stream_state_t;

extern const nx_uart_ops_t nx_stm32_uart_stream_ops;

/**
 * \brief           Initialize UART TX IRQ and explicit block-only RX format.
 *
 * \param[in,out]   state: Static UART registers, IRQ, baud and block-only
 *                  profile.
 *
 * \param[in]       clock_hz: Exact selected UART peripheral clock.
 *
 * \return          Success or INVALID/UNSUPPORTED before a stream buffer loan.
 *
 * \note            RX block storage is supplied by rx_start, never allocated
 *                  here.
 */
nx_result_t nx_stm32_uart_stream_initialize(nx_stm32_uart_stream_state_t* state,
                                            uint32_t clock_hz);
/**
 * \brief           Fill one received byte and latch IDLE/full/loss boundaries.
 *
 * \param[in,out]   state: Exact fixed instance kept alive through successful
 *                  stop.
 *
 * \note            The generated USART1 vector calls this block-mode provider.
 *                  Backpressure never overwrites READY/BORROWED consumer
 *                  storage.
 */
void nx_stm32_uart_stream_irq(nx_stm32_uart_stream_state_t* state);

#ifdef __cplusplus
}
#endif
#endif
