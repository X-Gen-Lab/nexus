/**
 * \file            stm32h743_uart_fifo.h
 * \brief           Selected USART3 bounded FIFO byte provider
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32H743_UART_FIFO_H
#define NEXUS_STM32H743_UART_FIFO_H

#include "stm32h743_cold.h"

/** \brief Immutable exact-mode budgets; encodings 0..5 follow reviewed LL. */
typedef struct {
    uint8_t rx_threshold;
    uint8_t tx_threshold;
    uint8_t rx_budget;
    uint8_t tx_budget;
} nx_stm32h743_uart_fifo_profile_t;

/** \brief Only selected FIFO mode retains this extra immutable-profile link. */
typedef struct {
    nx_stm32h743_uart_state_t uart;
    const nx_stm32h743_uart_fifo_profile_t* fifo;
} nx_stm32h743_uart_fifo_state_t;

#ifdef __cplusplus
extern "C" {
#endif

extern const nx_uart_ops_t nx_stm32h743_uart_fifo_ops;
/**
 * \brief           Initialize exact USART3 HSI baseline, 8N1 FIFO byte mode.
 * \param[in,out]   port: Zeroed idle state with fixed prefix and profile.
 * \param[in]       clock: Existing admitted HSI peripheral clock.
 * \param[in]       limit: Finite ACK polling bound.
 * \return          SUCCESS only after FIFO/frame/NVIC readback.
 * \note            Task-only. Rejected validation has no effects. Failure
 *                  after effects retains uart.retained and closes admission
 *                  until public stop proves all sources and vectors joined.
 *                  Ring/profile/state must outlive that successful stop.
 */
nx_result_t
nx_stm32h743_uart_fifo_initialize(nx_stm32h743_uart_fifo_state_t* port,
                                  uint32_t clock, uint32_t limit);
/**
 * \brief           Exact vector entry with separate bounded RX/TX work.
 * \note            ISR-only. Never release accepted request storage here;
 *                  task service observes wire TC, joins sources and settles.
 */
void nx_stm32h743_uart_fifo_irq(nx_stm32h743_uart_fifo_state_t* port);

/** \brief Prepare selected byte-ring/FIFO state and its exact pad route.
 * \note Task-only; Platform retains pads/gates after partial failure, and
 *       immutable configuration/profile remain valid through public stop.
 */
nx_result_t
nx_stm32h743_uart_fifo_prepare(nx_stm32h743_uart_fifo_state_t* port,
                               const nx_stm32h743_uart_config_t* config,
                               const nx_stm32h743_uart_fifo_profile_t* fifo);

#ifdef __cplusplus
}
#endif
#endif
