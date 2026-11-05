/**
 * \file            uart_fifo_profile.h
 * \brief           Private immutable FIFO thresholds and IRQ work budgets
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_UART_FIFO_PROFILE_H
#define NEXUS_PRIVATE_UART_FIFO_PROFILE_H
#include <stdint.h>
/** \brief Selected-mode thresholds use reviewed LL encodings 0..5.
 * \note RX/TX budgets bound work per IRQ. The immutable profile outlives the
 *       retained provider; silicon depth and hardware ports remain separate.
 */
typedef struct {
    uint8_t rx_threshold;
    uint8_t tx_threshold;
    uint8_t rx_budget;
    uint8_t tx_budget;
} nx_uart_fifo_profile_t;
#endif
