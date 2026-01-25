/**
 * \file            native_i2c_helpers.c
 * \brief           Native I2C test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_i2c_helpers.h"
#include "hal/nx_factory.h"

/* Include platform-specific types */
#include "../../../../platforms/native/src/i2c/nx_i2c_types.h"

#include <string.h>

/*---------------------------------------------------------------------------*/
/* Constants                                                                 */
/*---------------------------------------------------------------------------*/

#define NX_I2C_MAX_INSTANCES 8

/*---------------------------------------------------------------------------*/
/* Internal Helper                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get I2C implementation structure
 * \details         Retrieves the implementation structure from the factory.
 */
static nx_i2c_impl_t* get_i2c_impl(uint8_t instance) {
    /* Validate parameters */
    if (instance >= NX_I2C_MAX_INSTANCES) {
        return NULL;
    }

    /* Get I2C instance from factory */
    nx_i2c_bus_t* i2c = nx_factory_i2c(instance);
    if (i2c == NULL) {
        return NULL;
    }

    return (nx_i2c_impl_t*)i2c;
}

/*---------------------------------------------------------------------------*/
/* Circular Buffer Helpers                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get available space in circular buffer
 */
static size_t buffer_available_space(const nx_i2c_buffer_t* buf) {
    if (buf == NULL || buf->data == NULL) {
        return 0;
    }
    return buf->size - buf->count;
}

/**
 * \brief           Write data to circular buffer
 */
static size_t buffer_write(nx_i2c_buffer_t* buf, const uint8_t* data,
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
static size_t buffer_read(nx_i2c_buffer_t* buf, uint8_t* data, size_t len) {
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
 * \brief           Get I2C device state
 */
nx_status_t native_i2c_get_state(uint8_t instance, native_i2c_state_t* state) {
    /* Validate parameters */
    if (state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Get implementation */
    nx_i2c_impl_t* impl = get_i2c_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Copy state information */
    state->initialized = impl->state->initialized;
    state->suspended = impl->state->suspended;
    state->busy = impl->state->busy;
    state->speed = impl->state->config.speed;
    state->scl_pin = impl->state->config.scl_pin;
    state->sda_pin = impl->state->config.sda_pin;
    state->current_dev_addr = impl->state->current_device.dev_addr;
    state->tx_count = impl->state->stats.tx_count;
    state->rx_count = impl->state->stats.rx_count;
    state->nack_count = impl->state->stats.nack_count;
    state->bus_error_count = impl->state->stats.bus_error_count;
    state->tx_buf_count = impl->state->tx_buf.count;
    state->rx_buf_count = impl->state->rx_buf.count;

    return NX_OK;
}

/**
 * \brief           Inject receive data
 * \details         Simulates data arriving from hardware by writing to the
 *                  RX buffer. Updates statistics to track received data.
 *                  For testing purposes, this updates rx_count immediately
 *                  rather than waiting for actual read operations.
 */
nx_status_t native_i2c_inject_rx_data(uint8_t instance, const uint8_t* data,
                                      size_t len) {
    /* Validate parameters */
    if (data == NULL || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Get implementation */
    nx_i2c_impl_t* impl = get_i2c_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if initialized */
    if (!impl->state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Check if buffer is allocated */
    if (impl->state->rx_buf.data == NULL) {
        return NX_ERR_INVALID_STATE;
    }

    /* Write data to RX buffer */
    size_t written = buffer_write(&impl->state->rx_buf, data, len);
    if (written < len) {
        /* Buffer overflow - some data was lost */
        impl->state->stats.bus_error_count++;
        return NX_ERR_NO_MEMORY;
    }

    /* Update statistics after successful write */
    impl->state->stats.rx_count += (uint32_t)written;

    return NX_OK;
}

/**
 * \brief           Get transmitted data
 * \details         Captures data from the TX buffer, simulating what would
 *                  be sent to the hardware. This allows tests to verify
 *                  what data was transmitted.
 */
nx_status_t native_i2c_get_tx_data(uint8_t instance, uint8_t* data,
                                   size_t* len) {
    /* Validate parameters */
    if (data == NULL || len == NULL || *len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Get implementation */
    nx_i2c_impl_t* impl = get_i2c_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if initialized */
    if (!impl->state->initialized) {
        return NX_ERR_INVALID_STATE;
    }

    /* Read data from TX buffer */
    size_t read_count = buffer_read(&impl->state->tx_buf, data, *len);
    *len = read_count;

    return NX_OK;
}

/**
 * \brief           Reset specific I2C instance
 * \details         Resets the I2C instance to its initial state, clearing
 *                  all configuration, state, buffers, and statistics.
 */
nx_status_t native_i2c_reset(uint8_t instance) {
    /* Get implementation */
    nx_i2c_impl_t* impl = get_i2c_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Reset state flags */
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->busy = false;

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
    memset(&impl->state->stats, 0, sizeof(nx_i2c_stats_t));

    /* Reset device handle */
    memset(&impl->state->current_device, 0, sizeof(nx_i2c_device_handle_t));

    return NX_OK;
}

/**
 * \brief           Reset all I2C instances
 * \details         Iterates through all possible I2C instances and resets
 *                  each one to its initial state.
 */
void native_i2c_reset_all(void) {
    for (uint8_t instance = 0; instance < NX_I2C_MAX_INSTANCES; instance++) {
        native_i2c_reset(instance);
    }
}
