/**
 * \file            nx_spi_helpers.c
 * \brief           SPI helper functions implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements SPI helper functions for buffer management
 *                  and state operations.
 */

#include "nx_spi_helpers.h"
#include "hal/nx_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Helper Function Implementations                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Write data to circular buffer
 * \note            Frequently called functions moved to inline in header
 * \note            Optimized to use memcpy for contiguous blocks
 */
size_t spi_buffer_write(nx_spi_buffer_t* buf, const uint8_t* data, size_t len) {
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
size_t spi_buffer_read(nx_spi_buffer_t* buf, uint8_t* data, size_t len) {
    if (!buf || !data || len == 0) {
        return 0;
    }

    size_t to_read = (len < buf->count) ? len : buf->count;
    size_t read = 0;

    /* Read in two chunks if wrapping around */
    while (read < to_read) {
        size_t chunk_size = buf->size - buf->tail;
        if (chunk_size > (to_read - read)) {
            chunk_size = to_read - read;
        }

        /* Use memcpy for efficient block copy */
        memcpy(&data[read], &buf->data[buf->tail], chunk_size);
        buf->tail = (buf->tail + chunk_size) % buf->size;
        buf->count -= chunk_size;
        read += chunk_size;
    }

    return read;
}

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Inject data into RX buffer for testing
 */
size_t spi_inject_rx_data(nx_spi_state_t* state, const uint8_t* data,
                          size_t len) {
    if (!state || !data) {
        return 0;
    }
    return spi_buffer_write(&state->rx_buf, data, len);
}

/**
 * \brief           Get TX buffer data for testing
 */
size_t spi_get_tx_data(nx_spi_state_t* state, uint8_t* data, size_t len) {
    if (!state || !data) {
        return 0;
    }
    return spi_buffer_read(&state->tx_buf, data, len);
}

/**
 * \brief           Reset SPI state for testing
 */
void spi_reset_state(nx_spi_state_t* state) {
    if (!state) {
        return;
    }

    /* Clear buffers */
    spi_buffer_clear(&state->tx_buf);
    spi_buffer_clear(&state->rx_buf);

    /* Reset statistics */
    memset(&state->stats, 0, sizeof(nx_spi_stats_t));

    /* Reset flags */
    state->busy = false;
    state->current_device.in_use = false;
}
