/**
 * \file            shell_uart_backend.c
 * \brief           Shell UART backend implementation
 * \author          Nexus Team
 * \version         2.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Implements the UART backend for Shell I/O operations using the new
 * nx_uart_t interface.
 *
 * Requirements: 8.3, 8.4, 8.5
 */

#include "hal/interface/nx_uart.h"
#include "hal/nx_factory.h"
#include "shell/shell_backend.h"
#include <string.h>

/**
 * \addtogroup      SHELL_UART_BACKEND
 * \{
 */

/*---------------------------------------------------------------------------*/
/* Private Data                                                              */
/*---------------------------------------------------------------------------*/

/** UART interface pointer */
static nx_uart_t* g_uart = NULL;

/** UART index */
static uint8_t g_uart_index = 0;

/** Backend initialization flag */
static bool g_uart_backend_initialized = false;

/** Non-blocking read timeout in milliseconds */
#define UART_READ_TIMEOUT_MS 0

/** Blocking write timeout in milliseconds */
#define UART_WRITE_TIMEOUT_MS 1000

/*---------------------------------------------------------------------------*/
/* Private Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Non-blocking read from UART
 */
static int uart_backend_read(uint8_t* data, int max_len) {
    if (!g_uart_backend_initialized || g_uart == NULL) {
        return 0;
    }

    if (data == NULL || max_len <= 0) {
        return 0;
    }

    /* Get async RX interface */
    nx_rx_async_t* rx_async = g_uart->get_rx_async(g_uart);
    if (rx_async == NULL) {
        return 0;
    }

    /* Read available data using new interface */
    size_t len = (size_t)max_len;
    nx_status_t status = rx_async->receive(rx_async, data, &len);
    if (status != NX_OK) {
        return 0;
    }
    return (int)len;
}

/**
 * \brief           Blocking write to UART
 * \param[in]       data: Data buffer to write
 * \param[in]       len: Number of bytes to write
 * \return          Number of bytes actually written
 */
static int uart_backend_write(const uint8_t* data, int len) {
    if (!g_uart_backend_initialized || g_uart == NULL) {
        return 0;
    }

    if (data == NULL || len <= 0) {
        return 0;
    }

    /* Get sync TX interface */
    nx_tx_sync_t* tx_sync = g_uart->get_tx_sync(g_uart);
    if (tx_sync == NULL) {
        return 0;
    }

    /* Transmit data */
    nx_status_t status =
        tx_sync->send(tx_sync, data, (size_t)len, UART_WRITE_TIMEOUT_MS);
    if (status != NX_OK) {
        return 0;
    }

    return len;
}

/*---------------------------------------------------------------------------*/
/* Public Data                                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART backend instance
 */
const shell_backend_t shell_uart_backend = {.read = uart_backend_read,
                                            .write = uart_backend_write};

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

shell_status_t shell_uart_backend_init(int uart_instance) {
    /* Validate UART instance */
    if (uart_instance < 0 || uart_instance > 5) {
        return SHELL_ERROR_INVALID_PARAM;
    }

    /* Get UART interface from factory */
    g_uart = nx_factory_uart((uint8_t)uart_instance);
    if (g_uart == NULL) {
        return SHELL_ERROR;
    }

    /* Initialize UART */
    nx_lifecycle_t* lifecycle = g_uart->get_lifecycle(g_uart);
    if (lifecycle != NULL) {
        nx_status_t status = lifecycle->init(lifecycle);
        if (status != NX_OK) {
            nx_factory_uart_release(g_uart);
            g_uart = NULL;
            return SHELL_ERROR;
        }
    }

    g_uart_index = (uint8_t)uart_instance;
    g_uart_backend_initialized = true;

    return SHELL_OK;
}

shell_status_t shell_uart_backend_deinit(void) {
    if (g_uart != NULL) {
        /* Deinitialize UART */
        nx_lifecycle_t* lifecycle = g_uart->get_lifecycle(g_uart);
        if (lifecycle != NULL) {
            lifecycle->deinit(lifecycle);
        }

        /* Release UART */
        nx_factory_uart_release(g_uart);
        g_uart = NULL;
    }

    g_uart_backend_initialized = false;
    return SHELL_OK;
}

bool shell_uart_backend_is_initialized(void) {
    return g_uart_backend_initialized;
}

/**
 * \}
 */
