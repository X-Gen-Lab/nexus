/**
 * \file            shell_mock_backend.c
 * \brief           Shell Mock backend implementation for testing
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Implements a mock backend for Shell I/O operations used in testing.
 * Supports input injection and output capture.
 *
 * Requirements: 8.1
 */

#include "shell/shell_backend.h"
#include <string.h>

/**
 * \addtogroup      SHELL_MOCK_BACKEND
 * \{
 */

/*---------------------------------------------------------------------------*/
/* Constants                                                                 */
/*---------------------------------------------------------------------------*/

/** Maximum input buffer size */
#define MOCK_INPUT_BUFFER_SIZE      1024

/** Maximum output buffer size */
#define MOCK_OUTPUT_BUFFER_SIZE     4096

/*---------------------------------------------------------------------------*/
/* Private Data                                                              */
/*---------------------------------------------------------------------------*/

/** Input buffer for injected data */
static uint8_t g_input_buffer[MOCK_INPUT_BUFFER_SIZE];

/** Current input buffer length */
static size_t g_input_length = 0;

/** Current read position in input buffer */
static size_t g_input_read_pos = 0;

/** Output buffer for captured data */
static uint8_t g_output_buffer[MOCK_OUTPUT_BUFFER_SIZE];

/** Current output buffer length */
static size_t g_output_length = 0;

/** Backend initialization flag */
static bool g_mock_backend_initialized = false;

/*---------------------------------------------------------------------------*/
/* Private Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Non-blocking read from mock input buffer
 * \param[out]      data: Buffer to store read data
 * \param[in]       max_len: Maximum number of bytes to read
 * \return          Number of bytes actually read, 0 if no data available
 */
static int
mock_backend_read(uint8_t* data, int max_len) {
    if (!g_mock_backend_initialized) {
        return 0;
    }

    if (data == NULL || max_len <= 0) {
        return 0;
    }

    int count = 0;

    while (count < max_len && g_input_read_pos < g_input_length) {
        data[count++] = g_input_buffer[g_input_read_pos++];
    }

    return count;
}

/**
 * \brief           Write to mock output buffer
 * \param[in]       data: Data buffer to write
 * \param[in]       len: Number of bytes to write
 * \return          Number of bytes actually written
 */
static int
mock_backend_write(const uint8_t* data, int len) {
    if (!g_mock_backend_initialized) {
        return 0;
    }

    if (data == NULL || len <= 0) {
        return 0;
    }

    int count = 0;

    while (count < len && g_output_length < MOCK_OUTPUT_BUFFER_SIZE) {
        g_output_buffer[g_output_length++] = data[count++];
    }

    return count;
}

/*---------------------------------------------------------------------------*/
/* Public Data                                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Mock backend instance
 */
const shell_backend_t shell_mock_backend = {
    .read = mock_backend_read,
    .write = mock_backend_write
};

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

shell_status_t
shell_mock_backend_init(void) {
    g_input_length = 0;
    g_input_read_pos = 0;
    g_output_length = 0;
    g_mock_backend_initialized = true;

    return SHELL_OK;
}

shell_status_t
shell_mock_backend_deinit(void) {
    g_mock_backend_initialized = false;
    g_input_length = 0;
    g_input_read_pos = 0;
    g_output_length = 0;

    return SHELL_OK;
}

void
shell_mock_backend_reset(void) {
    g_input_length = 0;
    g_input_read_pos = 0;
    g_output_length = 0;
}

int
shell_mock_backend_inject_input(const uint8_t* data, size_t len) {
    if (data == NULL || len == 0) {
        return 0;
    }

    /* Reset read position when injecting new data */
    g_input_read_pos = 0;
    g_input_length = 0;

    size_t count = 0;
    while (count < len && g_input_length < MOCK_INPUT_BUFFER_SIZE) {
        g_input_buffer[g_input_length++] = data[count++];
    }

    return (int)count;
}

int
shell_mock_backend_inject_string(const char* str) {
    if (str == NULL) {
        return 0;
    }

    return shell_mock_backend_inject_input((const uint8_t*)str, strlen(str));
}

int
shell_mock_backend_get_output(uint8_t* data, size_t max_len) {
    if (data == NULL || max_len == 0) {
        return 0;
    }

    size_t copy_len = g_output_length;
    if (copy_len > max_len) {
        copy_len = max_len;
    }

    memcpy(data, g_output_buffer, copy_len);
    return (int)copy_len;
}

int
shell_mock_backend_get_output_string(char* str, size_t max_len) {
    if (str == NULL || max_len == 0) {
        return 0;
    }

    size_t copy_len = g_output_length;
    if (copy_len >= max_len) {
        copy_len = max_len - 1;
    }

    memcpy(str, g_output_buffer, copy_len);
    str[copy_len] = '\0';

    return (int)copy_len;
}

size_t
shell_mock_backend_get_output_length(void) {
    return g_output_length;
}

void
shell_mock_backend_clear_output(void) {
    g_output_length = 0;
}

size_t
shell_mock_backend_get_remaining_input(void) {
    if (g_input_read_pos >= g_input_length) {
        return 0;
    }
    return g_input_length - g_input_read_pos;
}

bool
shell_mock_backend_is_initialized(void) {
    return g_mock_backend_initialized;
}

/**
 * \}
 */
