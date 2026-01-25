/**
 * \file            native_uart_helpers.c
 * \brief           Native UART test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_uart_helpers.h"
#include "hal/nx_factory.h"

/* Include platform-specific types */
#include "../../../../platforms/native/src/uart/nx_uart_types.h"

#include <string.h>

/*---------------------------------------------------------------------------*/
/* Constants                                                                 */
/*---------------------------------------------------------------------------*/

#define NX_UART_MAX_INSTANCES 8

/*---------------------------------------------------------------------------*/
/* Internal Helper                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get UART implementation structure
 * \details         Retrieves the implementation structure from the factory.
 */
static nx_uart_impl_t* get_uart_impl(uint8_t instance) {
    /* Validate parameters */
    if (instance >= NX_UART_MAX_INSTANCES) {
        return NULL;
    }

    /* Get UART instance from factory */
    nx_uart_t* uart = nx_factory_uart(instance);
    if (uart == NULL) {
        return NULL;
    }

    return (nx_uart_impl_t*)uart;
}

/*---------------------------------------------------------------------------*/
/* Circular Buffer Helpers                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get available space in circular buffer
 */
static size_t buffer_available_space(const nx_uart_buffer_t* buf) {
    if (buf == NULL || buf->data == NULL) {
        return 0;
    }
    return buf->size - buf->count;
}

/**
 * \brief           Write data to circular buffer
 */
static size_t buffer_write(nx_uart_buffer_t* buf, const uint8_t* data,
                           size_t len) {
    if (buf == NULL || buf->data == NULL || data == NULL || len == 0) {
        return 0;
    }

    size_t space = buffer_available_space(buf);
    size_t to_write = (len < space) ? len : space;
    size_t written = 0;

    while (written < to_write) {
        buf->data[buf->head] = data[written];
        buf->head = (buf->head + 1) % buf->size;
        buf->count++;
        written++;
    }

    return written;
}

/**
 * \brief           Read data from circular buffer
 */
static size_t buffer_read(nx_uart_buffer_t* buf, uint8_t* data, size_t len) {
    if (buf == NULL || buf->data == NULL || data == NULL || len == 0) {
        return 0;
    }

    size_t available = buf->count;
    size_t to_read = (len < available) ? len : available;
    size_t read_count = 0;

    while (read_count < to_read) {
        data[read_count] = buf->data[buf->tail];
        buf->tail = (buf->tail + 1) % buf->size;
        buf->count--;
        read_count++;
    }

    return read_count;
}

/*---------------------------------------------------------------------------*/
/* Test Helper Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get UART device state
 */
nx_status_t native_uart_get_state(uint8_t instance,
                                  native_uart_state_t* state) {
    /* Validate parameters */
    if (state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Get implementation */
    nx_uart_impl_t* impl = get_uart_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Copy state information */
    state->initialized = impl->state->initialized;
    state->suspended = impl->state->suspended;
    state->baudrate = impl->state->config.baudrate;
    state->word_length = impl->state->config.word_length;
    state->stop_bits = impl->state->config.stop_bits;
    state->parity = impl->state->config.parity;
    state->flow_control = impl->state->config.flow_control;
    state->tx_busy = impl->state->tx_busy;
    state->rx_busy = impl->state->stats.rx_busy;
    state->tx_count = impl->state->stats.tx_count;
    state->rx_count = impl->state->stats.rx_count;
    state->tx_errors = impl->state->stats.tx_errors;
    state->rx_errors = impl->state->stats.rx_errors;
    state->overrun_errors = impl->state->stats.overrun_errors;
    state->framing_errors = impl->state->stats.framing_errors;
    state->tx_buf_count = impl->state->tx_buf.count;
    state->rx_buf_count = impl->state->rx_buf.count;

    return NX_OK;
}

/**
 * \brief           Inject receive data
 * \details         Simulates data arriving from hardware by writing to the
 *                  RX buffer. This makes the data available for reading
 *                  through the normal UART receive functions.
 */
nx_status_t native_uart_inject_rx_data(uint8_t instance, const uint8_t* data,
                                       size_t len) {
    /* Validate parameters */
    if (data == NULL || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Get implementation */
    nx_uart_impl_t* impl = get_uart_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if initialized */
    if (!impl->state->initialized) {
        return NX_ERR_INVALID_STATE;
    }

    /* Check if buffer is allocated */
    if (impl->state->rx_buf.data == NULL) {
        return NX_ERR_INVALID_STATE;
    }

    /* Write data to RX buffer using the actual implementation */
    size_t written = buffer_write(&impl->state->rx_buf, data, len);
    if (written < len) {
        /* Buffer overflow - some data was lost */
        impl->state->stats.overrun_errors++;
        return NX_ERR_NO_MEMORY;
    }

    /* Update RX count */
    impl->state->stats.rx_count += (uint32_t)written;

    return NX_OK;
}

/**
 * \brief           Get transmitted data
 * \details         Captures data from the TX buffer, simulating what would
 *                  be sent to the hardware. This allows tests to verify
 *                  what data was transmitted.
 */
nx_status_t native_uart_get_tx_data(uint8_t instance, uint8_t* data,
                                    size_t* len) {
    /* Validate parameters */
    if (data == NULL || len == NULL || *len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Get implementation */
    nx_uart_impl_t* impl = get_uart_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if initialized */
    if (!impl->state->initialized) {
        return NX_ERR_INVALID_STATE;
    }

    /* Check if buffer is allocated */
    if (impl->state->tx_buf.data == NULL) {
        *len = 0;
        return NX_ERR_INVALID_STATE;
    }

    /* Read data from TX buffer */
    size_t read_count = buffer_read(&impl->state->tx_buf, data, *len);
    *len = read_count;

    return NX_OK;
}

/**
 * \brief           Reset specific UART instance
 * \details         Resets the UART instance to its initial state, clearing
 *                  all configuration, state, buffers, and statistics.
 */
nx_status_t native_uart_reset(uint8_t instance) {
    /* Get implementation */
    nx_uart_impl_t* impl = get_uart_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Reset state flags first */
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->tx_busy = false;

    /* Reset buffers */
    if (impl->state->tx_buf.data != NULL) {
        impl->state->tx_buf.head = 0;
        impl->state->tx_buf.tail = 0;
        impl->state->tx_buf.count = 0;
        memset(impl->state->tx_buf.data, 0, impl->state->tx_buf.size);
    }

    if (impl->state->rx_buf.data != NULL) {
        impl->state->rx_buf.head = 0;
        impl->state->rx_buf.tail = 0;
        impl->state->rx_buf.count = 0;
        memset(impl->state->rx_buf.data, 0, impl->state->rx_buf.size);
    }

    /* Reset statistics */
    memset(&impl->state->stats, 0, sizeof(nx_uart_stats_t));

    return NX_OK;
}

/**
 * \brief           Reset all UART instances
 * \details         Iterates through all possible UART instances and resets
 *                  each one to its initial state.
 */
void native_uart_reset_all(void) {
    for (uint8_t instance = 0; instance < NX_UART_MAX_INSTANCES; instance++) {
        native_uart_reset(instance);
    }
}
