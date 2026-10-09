/**
 * \file            stm32_uart_diagnostic.h
 * \brief           STM32 UART diagnostic interface declarations
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef STM32_UART_DIAGNOSTIC_H
#define STM32_UART_DIAGNOSTIC_H

#include "hal/nx_status.h"
#include "stm32_uart_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Diagnostic Functions                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get buffer statistics
 * \param[in]       impl: UART implementation pointer
 * \param[out]      tx_peak: TX buffer peak usage (can be NULL)
 * \param[out]      tx_overflow: TX buffer overflow count (can be NULL)
 * \param[out]      rx_peak: RX buffer peak usage (can be NULL)
 * \param[out]      rx_overflow: RX buffer overflow count (can be NULL)
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t uart_diag_get_buffer_stats(stm32_uart_impl_t* impl, size_t* tx_peak,
                                       uint32_t* tx_overflow, size_t* rx_peak,
                                       uint32_t* rx_overflow);

/**
 * \brief           Get UART state information
 * \param[in]       impl: UART implementation pointer
 * \param[out]      initialized: Initialization state (can be NULL)
 * \param[out]      suspended: Suspend state (can be NULL)
 * \param[out]      tx_busy: TX busy state (can be NULL)
 * \param[out]      rx_busy: RX busy state (can be NULL)
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t uart_diag_get_state(stm32_uart_impl_t* impl, bool* initialized,
                                bool* suspended, bool* tx_busy, bool* rx_busy);

/**
 * \brief           Reset statistics counters
 * \param[in]       impl: UART implementation pointer
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t uart_diag_reset_stats(stm32_uart_impl_t* impl);

#ifdef __cplusplus
}
#endif

#endif /* STM32_UART_DIAGNOSTIC_H */
