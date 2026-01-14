/**
 * \file            shell_uart_backend.c
 * \brief           Shell UART backend implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Implements the UART backend for Shell I/O operations using HAL UART interface.
 *
 * Requirements: 8.3, 8.4, 8.5
 */

#include "shell/shell_backend.h"
#include "hal/hal_uart.h"
#include <string.h>

/**
 * \defgroup        SHELL_UART_BACKEND_IMPL Shell UART Backend Implementation
 * \brief           UART backend implementation
 * \{
 */

/**
 * \name            Private Data
 * \{
 */

/** UART instance used by the backend */
static hal_uart_instance_t g_uart_instance = HAL_UART_0;

/** Backend initialization flag */
static bool g_uart_backend_initialized = false;

/** Non-blocking read timeout in milliseconds */
#define UART_READ_TIMEOUT_MS    0

/** Blocking write timeout in milliseconds */
#define UART_WRITE_TIMEOUT_MS   1000

/**
 * \}
 */

/**
 * \name            Private Functions
 * \{
 */

/**
 * \brief           Non-blocking read from UART
 * \param[out]      data: Buffer to store read data
 * \param[in]       max_len: Maximum number of bytes to read
 * \return          Number of bytes actually read, 0 if no data available
 *
 * Requirement 8.4: Non-blocking read operation
 */
static int
uart_backend_read(uint8_t* data, int max_len) {
    if (!g_uart_backend_initialized) {
        return 0;
    }

    if (data == NULL || max_len <= 0) {
        return 0;
    }

    int count = 0;

    /* Try to read available bytes without blocking */
    while (count < max_len) {
        hal_status_t status = hal_uart_getc(g_uart_instance, &data[count],
                                            UART_READ_TIMEOUT_MS);
        if (status != HAL_OK) {
            /* No more data available */
            break;
        }
        count++;
    }

    return count;
}

/**
 * \brief           Blocking write to UART
 * \param[in]       data: Data buffer to write
 * \param[in]       len: Number of bytes to write
 * \return          Number of bytes actually written
 *
 * Requirement 8.5: Blocking write operation
 */
static int
uart_backend_write(const uint8_t* data, int len) {
    if (!g_uart_backend_initialized) {
        return 0;
    }

    if (data == NULL || len <= 0) {
        return 0;
    }

    hal_status_t status = hal_uart_transmit(g_uart_instance, data,
                                            (size_t)len, UART_WRITE_TIMEOUT_MS);
    if (status != HAL_OK) {
        return 0;
    }

    return len;
}

/**
 * \}
 */

/**
 * \name            Public Data
 * \{
 */

/**
 * \brief           UART backend instance
 *
 * Pre-configured backend for UART communication.
 * Requirement 8.3: UART backend implementation
 */
const shell_backend_t shell_uart_backend = {
    .read = uart_backend_read,
    .write = uart_backend_write
};

/**
 * \}
 */

/**
 * \name            Public API Implementation
 * \{
 */

/**
 * \brief           Initialize UART backend
 * \param[in]       uart_instance: UART instance number to use
 * \return          SHELL_OK on success, error code otherwise
 */
shell_status_t
shell_uart_backend_init(int uart_instance) {
    /* Validate UART instance */
    if (uart_instance < 0 || uart_instance >= HAL_UART_MAX) {
        return SHELL_ERROR_INVALID_PARAM;
    }

    g_uart_instance = (hal_uart_instance_t)uart_instance;
    g_uart_backend_initialized = true;

    return SHELL_OK;
}

/**
 * \brief           Deinitialize UART backend
 * \return          SHELL_OK on success
 */
shell_status_t
shell_uart_backend_deinit(void) {
    g_uart_backend_initialized = false;
    return SHELL_OK;
}

/**
 * \brief           Check if UART backend is initialized
 * \return          true if initialized, false otherwise
 */
bool
shell_uart_backend_is_initialized(void) {
    return g_uart_backend_initialized;
}

/**
 * \}
 */

/**
 * \}
 */
