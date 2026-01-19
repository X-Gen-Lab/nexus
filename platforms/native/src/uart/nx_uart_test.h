/**
 * \file            nx_uart_test.h
 * \brief           UART test support functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_UART_TEST_H
#define NX_UART_TEST_H

#include "hal/nx_status.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Inject data into RX buffer (for testing)
 * \param[in]       index: UART instance index
 * \param[in]       data: Data to inject
 * \param[in]       len: Length of data
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_uart_native_inject_rx(uint8_t index, const uint8_t* data,
                                     size_t len);

/**
 * \brief           Get TX buffer data (for testing)
 * \param[in]       index: UART instance index
 * \param[out]      data: Buffer to store data
 * \param[in]       max_len: Maximum length to read
 * \param[out]      actual_len: Actual length read
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_uart_native_get_tx_data(uint8_t index, uint8_t* data,
                                       size_t max_len, size_t* actual_len);

/**
 * \brief           Get UART state (for testing)
 * \param[in]       index: UART instance index
 * \param[out]      initialized: Initialization flag
 * \param[out]      suspended: Suspend flag
 * \param[out]      tx_busy: TX busy flag
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_uart_native_get_state(uint8_t index, bool* initialized,
                                     bool* suspended, bool* tx_busy);

/**
 * \brief           Reset UART instance (for testing)
 * \param[in]       index: UART instance index
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_uart_native_reset(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* NX_UART_TEST_H */
