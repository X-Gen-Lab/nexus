/**
 * \file            native_uart_test.h
 * \brief           Native UART Test Helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_UART_TEST_H
#define NATIVE_UART_TEST_H

#include "hal/interface/nx_uart.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Type Definitions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART configuration structure (opaque)
 */
typedef struct nx_uart_config_s nx_uart_config_t;

/**
 * \brief           Simulated UART state (opaque for testing)
 */
typedef struct native_uart_state native_uart_state_t;

/*---------------------------------------------------------------------------*/
/* Factory Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get UART instance (factory function)
 * \param[in]       index: UART index (0-3)
 * \return          UART interface pointer, NULL on failure
 */
nx_uart_t* nx_uart_native_get(uint8_t index);

/**
 * \brief           Get UART instance with configuration
 * \param[in]       index: UART index (0-3)
 * \param[in]       cfg: UART configuration
 * \return          UART interface pointer, NULL on failure
 */
nx_uart_t* nx_uart_native_get_with_config(uint8_t index,
                                          const nx_uart_config_t* cfg);

/*---------------------------------------------------------------------------*/
/* Reset Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset all UART instances (for testing)
 */
void nx_uart_native_reset_all(void);

/**
 * \brief           Reset all simulated UART states
 */
void native_uart_reset_all(void);

/*---------------------------------------------------------------------------*/
/* State Query Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get simulated UART state (for testing)
 * \param[in]       instance: UART instance
 * \return          Pointer to UART state or NULL
 */
native_uart_state_t* native_uart_get_state(int instance);

/**
 * \brief           Get actual configured baudrate (for testing)
 * \param[in]       instance: UART instance
 * \return          Actual baudrate or 0 if not initialized
 */
uint32_t native_uart_get_actual_baudrate(int instance);

/*---------------------------------------------------------------------------*/
/* UART-Specific Test Helpers                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Inject data into UART RX buffer (for testing)
 * \param[in]       instance: UART instance
 * \param[in]       data: Data to inject
 * \param[in]       len: Length of data
 * \return          true on success, false on failure
 */
bool native_uart_inject_rx_data(int instance, const uint8_t* data, size_t len);

/**
 * \brief           Get data from UART TX buffer (for testing)
 * \param[in]       instance: UART instance
 * \param[out]      data: Buffer to store data
 * \param[in]       max_len: Maximum length to read
 * \return          Number of bytes read
 */
size_t native_uart_get_tx_data(int instance, uint8_t* data, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_UART_TEST_H */
