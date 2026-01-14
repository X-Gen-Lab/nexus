/**
 * \file            shell_backend.h
 * \brief           Shell backend interface definitions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This file defines the backend interface for Shell I/O operations.
 * Backends provide the actual input/output channel (e.g., UART).
 */

#ifndef SHELL_BACKEND_H
#define SHELL_BACKEND_H

#include "shell_def.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        SHELL_BACKEND Shell Backend Interface
 * \brief           Backend abstraction for shell I/O
 * \{
 */

/**
 * \brief           Shell backend interface structure
 */
typedef struct {
    /**
     * \brief       Non-blocking read function
     * \param[out]  data: Buffer to store read data
     * \param[in]   max_len: Maximum number of bytes to read
     * \return      Number of bytes actually read, 0 if no data available
     */
    int (*read)(uint8_t* data, int max_len);

    /**
     * \brief       Blocking write function
     * \param[in]   data: Data buffer to write
     * \param[in]   len: Number of bytes to write
     * \return      Number of bytes actually written
     */
    int (*write)(const uint8_t* data, int len);
} shell_backend_t;

/**
 * \brief           Set the Shell backend
 * \param[in]       backend: Pointer to backend interface structure
 * \return          SHELL_OK on success, error code otherwise
 */
shell_status_t shell_set_backend(const shell_backend_t* backend);

/**
 * \brief           Get the current Shell backend
 * \return          Pointer to current backend, or NULL if not set
 */
const shell_backend_t* shell_get_backend(void);

/**
 * \brief           Printf-style output to Shell
 * \param[in]       format: Printf-style format string
 * \param[in]       ...: Format arguments
 * \return          Number of characters written, or negative on error
 */
int shell_printf(const char* format, ...);

/**
 * \brief           Write raw data to Shell backend
 * \param[in]       data: Data buffer to write
 * \param[in]       len: Number of bytes to write
 * \return          Number of bytes written
 */
int shell_write(const uint8_t* data, int len);

/**
 * \brief           Write a single character to Shell backend
 * \param[in]       c: Character to write
 * \return          1 on success, 0 on failure
 */
int shell_putchar(char c);

/**
 * \brief           Write a string to Shell backend
 * \param[in]       str: Null-terminated string to write
 * \return          Number of characters written
 */
int shell_puts(const char* str);

/**
 * \}
 */

/**
 * \defgroup        SHELL_UART_BACKEND UART Backend
 * \brief           UART backend implementation
 * \{
 */

/**
 * \brief           UART backend instance
 */
extern const shell_backend_t shell_uart_backend;

/**
 * \brief           Initialize UART backend
 * \param[in]       uart_instance: UART instance number to use
 * \return          SHELL_OK on success, error code otherwise
 */
shell_status_t shell_uart_backend_init(int uart_instance);

/**
 * \brief           Deinitialize UART backend
 * \return          SHELL_OK on success
 */
shell_status_t shell_uart_backend_deinit(void);

/**
 * \brief           Check if UART backend is initialized
 * \return          true if initialized, false otherwise
 */
bool shell_uart_backend_is_initialized(void);

/**
 * \}
 */

/**
 * \defgroup        SHELL_MOCK_BACKEND Mock Backend (Testing)
 * \brief           Mock backend for testing
 * \{
 */

/**
 * \brief           Mock backend instance
 */
extern const shell_backend_t shell_mock_backend;

/**
 * \brief           Initialize mock backend
 * \return          SHELL_OK on success
 */
shell_status_t shell_mock_backend_init(void);

/**
 * \brief           Deinitialize mock backend
 * \return          SHELL_OK on success
 */
shell_status_t shell_mock_backend_deinit(void);

/**
 * \brief           Reset mock backend buffers
 */
void shell_mock_backend_reset(void);

/**
 * \brief           Inject input data into mock backend
 * \param[in]       data: Data to inject
 * \param[in]       len: Length of data
 * \return          Number of bytes injected
 */
int shell_mock_backend_inject_input(const uint8_t* data, size_t len);

/**
 * \brief           Inject input string into mock backend
 * \param[in]       str: Null-terminated string to inject
 * \return          Number of bytes injected
 */
int shell_mock_backend_inject_string(const char* str);

/**
 * \brief           Get captured output data
 * \param[out]      data: Buffer to store output data
 * \param[in]       max_len: Maximum buffer size
 * \return          Number of bytes copied
 */
int shell_mock_backend_get_output(uint8_t* data, size_t max_len);

/**
 * \brief           Get captured output as string
 * \param[out]      str: Buffer to store output string
 * \param[in]       max_len: Maximum buffer size (including null terminator)
 * \return          Number of characters copied (excluding null terminator)
 */
int shell_mock_backend_get_output_string(char* str, size_t max_len);

/**
 * \brief           Get current output length
 * \return          Number of bytes in output buffer
 */
size_t shell_mock_backend_get_output_length(void);

/**
 * \brief           Clear output buffer
 */
void shell_mock_backend_clear_output(void);

/**
 * \brief           Get remaining input length
 * \return          Number of bytes remaining in input buffer
 */
size_t shell_mock_backend_get_remaining_input(void);

/**
 * \brief           Check if mock backend is initialized
 * \return          true if initialized, false otherwise
 */
bool shell_mock_backend_is_initialized(void);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* SHELL_BACKEND_H */
