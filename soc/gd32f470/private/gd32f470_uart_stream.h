/**
 * \file            gd32f470_uart_stream.h
 * \brief           Private statically selected USART IRQ receive block storage
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_UART_STREAM_H
#define NEXUS_GD32F470_UART_STREAM_H

#include "gd32f470_provider.h"

/** \brief Only the selected block mode pays for producer loan metadata. */
typedef struct {
    nx_gd32_uart_state_t uart;
    nx_stream_t* stream;
    nx_stream_fill_t fill;
    size_t length;
    uint32_t block_flags;
    bool filling;
    bool stopped;
    bool loss_latched;
} nx_gd32_uart_stream_state_t;

#ifdef __cplusplus
extern "C" {
#endif

extern const nx_uart_ops_t nx_gd32_uart_stream_ops;

/**
 * \brief           Initialize TX IRQ and block-only RX without a hidden ring.
 * \param[in,out]   state: Fresh exact instance kept through successful stop.
 * \param[in]       controller: Selected SoC resource with reviewed Board pins.
 * \param[in]       baud: Valid divider of the selected peripheral clock.
 * \param[in]       priority: Reviewed NVIC preemption priority 0 through 15.
 * \return          Success or INVALID/BUSY/CONTEXT without a block storage
 * loan. \note            Cold task-only assembly; rx_start supplies caller
 * blocks.
 */
nx_result_t
nx_gd32_uart_stream_initialize_at(nx_gd32_uart_stream_state_t* state,
                                  const nx_gd32_uart_controller_t* controller,
                                  uint32_t baud, unsigned priority);
/**
 * \brief           Fill one received byte and latch IDLE/full/error boundaries.
 * \param[in,out]   state: Exact instance selected by the generated USART
 * vector. \note            IRQ-only single producer. READY/BORROWED blocks stay
 * immutable; unavailable storage latches a loss interval rather than
 *                  overwriting a consumer. Wake runs after metadata
 * publication.
 */
void nx_gd32_uart_stream_irq(nx_gd32_uart_stream_state_t* state);

#ifdef __cplusplus
}
#endif

#endif
