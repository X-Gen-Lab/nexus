/**
 * \file            nx_uart_helpers.c
 * \brief           UART helper functions implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements UART helper functions for buffer management
 *                  and state operations.
 */

#include "nx_uart_helpers.h"
#include "hal/base/nx_device.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Helper Functions Implementation                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Write data to circular buffer
 * \note            Frequently called functions moved to inline in header
 * \note            Optimized to use memcpy for contiguous blocks
 */
size_t buffer_write(nx_uart_buffer_t* buf, const uint8_t* data, size_t len) {
    if (!buf || !data || len == 0) {
        return 0;
    }

    size_t space = buf->size - buf->count;
    size_t to_write = (len < space) ? len : space;
    size_t written = 0;

    /* Write in two chunks if wrapping around */
    while (written < to_write) {
        size_t chunk_size = buf->size - buf->head;
        if (chunk_size > (to_write - written)) {
            chunk_size = to_write - written;
        }

        /* Use memcpy for efficient block copy */
        memcpy(&buf->data[buf->head], &data[written], chunk_size);
        buf->head = (buf->head + chunk_size) % buf->size;
        buf->count += chunk_size;
        written += chunk_size;
    }

    return written;
}

/**
 * \brief           Read data from circular buffer
 * \note            Optimized to use memcpy for contiguous blocks
 */
size_t buffer_read(nx_uart_buffer_t* buf, uint8_t* data, size_t len) {
    if (!buf || !data || len == 0) {
        return 0;
    }

    size_t to_read = (len < buf->count) ? len : buf->count;
    size_t read_count = 0;

    /* Read in two chunks if wrapping around */
    while (read_count < to_read) {
        size_t chunk_size = buf->size - buf->tail;
        if (chunk_size > (to_read - read_count)) {
            chunk_size = to_read - read_count;
        }

        /* Use memcpy for efficient block copy */
        memcpy(&data[read_count], &buf->data[buf->tail], chunk_size);
        buf->tail = (buf->tail + chunk_size) % buf->size;
        buf->count -= chunk_size;
        read_count += chunk_size;
    }

    return read_count;
}
