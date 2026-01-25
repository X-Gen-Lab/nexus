/**
 * \file            native_uart_helpers.h
 * \brief           Native UART test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_UART_HELPERS_H
#define NATIVE_UART_HELPERS_H

#include "hal/interface/nx_uart.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* UART State Structure for Testing                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART state structure for testing
 *
 * Contains runtime state information that can be queried by tests.
 */
typedef struct native_uart_state_s {
    bool initialized;        /**< Initialization flag */
    bool suspended;          /**< Suspend flag */
    uint32_t baudrate;       /**< Configured baud rate */
    uint8_t word_length;     /**< Word length (data bits) */
    uint8_t stop_bits;       /**< Stop bits */
    uint8_t parity;          /**< Parity setting */
    uint8_t flow_control;    /**< Flow control setting */
    bool tx_busy;            /**< TX busy flag */
    bool rx_busy;            /**< RX busy flag */
    uint32_t tx_count;       /**< Total bytes transmitted */
    uint32_t rx_count;       /**< Total bytes received */
    uint32_t tx_errors;      /**< TX error count */
    uint32_t rx_errors;      /**< RX error count */
    uint32_t overrun_errors; /**< Overrun error count */
    uint32_t framing_errors; /**< Framing error count */
    size_t tx_buf_count;     /**< Bytes in TX buffer */
    size_t rx_buf_count;     /**< Bytes in RX buffer */
} native_uart_state_t;

/*---------------------------------------------------------------------------*/
/* UART Test Helpers                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get UART device state
 * \param[in]       instance: UART instance ID
 * \param[out]      state: State structure to fill
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_uart_get_state(uint8_t instance, native_uart_state_t* state);

/**
 * \brief           Inject receive data (simulate hardware reception)
 * \param[in]       instance: UART instance ID
 * \param[in]       data: Data buffer to inject
 * \param[in]       len: Data length
 * \return          NX_OK on success, error code otherwise
 * \note            This simulates data arriving from the hardware into
 *                  the RX buffer, making it available for reading.
 */
nx_status_t native_uart_inject_rx_data(uint8_t instance, const uint8_t* data,
                                       size_t len);

/**
 * \brief           Get transmitted data (capture hardware transmission)
 * \param[in]       instance: UART instance ID
 * \param[out]      data: Buffer to store captured data
 * \param[in,out]   len: Input: buffer size, Output: actual data length
 * \return          NX_OK on success, error code otherwise
 * \note            This captures data that was transmitted through the
 *                  TX buffer, simulating what would be sent to hardware.
 */
nx_status_t native_uart_get_tx_data(uint8_t instance, uint8_t* data,
                                    size_t* len);

/**
 * \brief           Reset specific UART instance to initial state
 * \param[in]       instance: UART instance ID
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_uart_reset(uint8_t instance);

/**
 * \brief           Reset all UART instances to initial state
 */
void native_uart_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_UART_HELPERS_H */
