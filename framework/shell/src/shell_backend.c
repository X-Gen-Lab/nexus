/**
 * \file            shell_backend.c
 * \brief           Shell backend abstraction implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Implements the backend abstraction layer for Shell I/O operations.
 *
 * Requirements: 8.1, 8.2, 8.6
 */

#include "shell/shell_backend.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/**
 * \defgroup        SHELL_BACKEND_IMPL Shell Backend Implementation
 * \brief           Backend abstraction implementation
 * \{
 */

/**
 * \name            Private Data
 * \{
 */

/** Current backend pointer */
static const shell_backend_t* g_current_backend = NULL;

/** Printf buffer size */
#define SHELL_PRINTF_BUFFER_SIZE    256

/**
 * \}
 */

/**
 * \name            Public API Implementation
 * \{
 */

/**
 * \brief           Set the Shell backend
 * \param[in]       backend: Pointer to backend interface structure
 * \return          SHELL_OK on success, error code otherwise
 */
shell_status_t
shell_set_backend(const shell_backend_t* backend) {
    g_current_backend = backend;
    return SHELL_OK;
}

/**
 * \brief           Get the current Shell backend
 * \return          Pointer to current backend, or NULL if not set
 */
const shell_backend_t*
shell_get_backend(void) {
    return g_current_backend;
}

/**
 * \brief           Printf-style output to Shell
 * \param[in]       format: Printf-style format string
 * \param[in]       ...: Format arguments
 * \return          Number of characters written, or negative on error
 */
int
shell_printf(const char* format, ...) {
    if (g_current_backend == NULL || g_current_backend->write == NULL) {
        return -1;
    }

    if (format == NULL) {
        return -1;
    }

    char buffer[SHELL_PRINTF_BUFFER_SIZE];
    va_list args;

    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len < 0) {
        return len;
    }

    /* Truncate if necessary */
    if (len >= (int)sizeof(buffer)) {
        len = sizeof(buffer) - 1;
    }

    return g_current_backend->write((const uint8_t*)buffer, len);
}

/**
 * \brief           Write raw data to Shell backend
 * \param[in]       data: Data buffer to write
 * \param[in]       len: Number of bytes to write
 * \return          Number of bytes written
 */
int
shell_write(const uint8_t* data, int len) {
    if (g_current_backend == NULL || g_current_backend->write == NULL) {
        return 0;
    }

    if (data == NULL || len <= 0) {
        return 0;
    }

    return g_current_backend->write(data, len);
}

/**
 * \brief           Write a single character to Shell backend
 * \param[in]       c: Character to write
 * \return          1 on success, 0 on failure
 */
int
shell_putchar(char c) {
    if (g_current_backend == NULL || g_current_backend->write == NULL) {
        return 0;
    }

    uint8_t byte = (uint8_t)c;
    return g_current_backend->write(&byte, 1);
}

/**
 * \brief           Write a string to Shell backend
 * \param[in]       str: Null-terminated string to write
 * \return          Number of characters written
 */
int
shell_puts(const char* str) {
    if (g_current_backend == NULL || g_current_backend->write == NULL) {
        return 0;
    }

    if (str == NULL) {
        return 0;
    }

    int len = (int)strlen(str);
    if (len == 0) {
        return 0;
    }

    return g_current_backend->write((const uint8_t*)str, len);
}

/**
 * \}
 */

/**
 * \}
 */

