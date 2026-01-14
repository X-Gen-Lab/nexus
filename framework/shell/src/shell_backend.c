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
 * \addtogroup      SHELL_BACKEND
 * \{
 */

/*---------------------------------------------------------------------------*/
/* Private Data                                                              */
/*---------------------------------------------------------------------------*/

/** Current backend pointer */
static const shell_backend_t* g_current_backend = NULL;

/** Printf buffer size */
#define SHELL_PRINTF_BUFFER_SIZE 256

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

shell_status_t shell_set_backend(const shell_backend_t* backend) {
    g_current_backend = backend;
    return SHELL_OK;
}

const shell_backend_t* shell_get_backend(void) {
    return g_current_backend;
}

int shell_printf(const char* format, ...) {
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

int shell_write(const uint8_t* data, int len) {
    if (g_current_backend == NULL || g_current_backend->write == NULL) {
        return 0;
    }

    if (data == NULL || len <= 0) {
        return 0;
    }

    return g_current_backend->write(data, len);
}

int shell_putchar(char c) {
    if (g_current_backend == NULL || g_current_backend->write == NULL) {
        return 0;
    }

    uint8_t byte = (uint8_t)c;
    return g_current_backend->write(&byte, 1);
}

int shell_puts(const char* str) {
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
