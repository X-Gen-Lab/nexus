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
     * \return      Number of bytes actually read, 0 if no data available, negative on I/O error
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

#ifdef __cplusplus
}
#endif

#endif /* SHELL_BACKEND_H */
