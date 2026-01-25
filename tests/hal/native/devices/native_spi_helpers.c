/**
 * \file            native_spi_helpers.c
 * \brief           Native SPI test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_spi_helpers.h"
#include "hal/nx_factory.h"

/* Include platform-specific types */
#include "../../../../platforms/native/src/spi/nx_spi_types.h"

#include <string.h>

/*---------------------------------------------------------------------------*/
/* Constants                                                                 */
/*---------------------------------------------------------------------------*/

#define NX_SPI_MAX_INSTANCES 4

/*---------------------------------------------------------------------------*/
/* Internal Helper                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get SPI implementation structure
 * \details         Retrieves the implementation structure from the factory.
 */
static nx_spi_impl_t* get_spi_impl(uint8_t instance) {
    /* Validate parameters */
    if (instance >= NX_SPI_MAX_INSTANCES) {
        return NULL;
    }

    /* Get SPI instance from factory */
    nx_spi_bus_t* spi = nx_factory_spi(instance);
    if (spi == NULL) {
        return NULL;
    }

    return (nx_spi_impl_t*)spi;
}

/*---------------------------------------------------------------------------*/
/* Buffer Helper Functions                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Write data to circular buffer
 * \details         Writes data to the circular buffer, handling wrap-around.
 */
static size_t buffer_write(nx_spi_buffer_t* buf, const uint8_t* data,
                           size_t len) {
    if (buf == NULL || buf->data == NULL || data == NULL) {
        return 0;
    }

    size_t written = 0;
    size_t available = buf->size - buf->count;

    /* Limit write to available space */
    if (len > available) {
        len = available;
    }

    while (written < len) {
        buf->data[buf->head] = data[written];
        buf->head = (buf->head + 1) % buf->size;
        buf->count++;
        written++;
    }

    return written;
}

/**
 * \brief           Read data from circular buffer
 * \details         Reads data from the circular buffer, handling wrap-around.
 */
static size_t buffer_read(nx_spi_buffer_t* buf, uint8_t* data, size_t len) {
    if (buf == NULL || buf->data == NULL || data == NULL) {
        return 0;
    }

    size_t read = 0;

    /* Limit read to available data */
    if (len > buf->count) {
        len = buf->count;
    }

    while (read < len) {
        data[read] = buf->data[buf->tail];
        buf->tail = (buf->tail + 1) % buf->size;
        buf->count--;
        read++;
    }

    return read;
}

/*---------------------------------------------------------------------------*/
/* Test Helper Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get SPI device state
 */
nx_status_t native_spi_get_state(uint8_t instance, native_spi_state_t* state) {
    /* Validate parameters */
    if (state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Get implementation */
    nx_spi_impl_t* impl = get_spi_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Copy state information */
    state->initialized = impl->state->initialized;
    state->suspended = impl->state->suspended;
    state->busy = impl->state->busy;
    state->max_speed = impl->state->config.max_speed;
    state->mosi_pin = impl->state->config.mosi_pin;
    state->miso_pin = impl->state->config.miso_pin;
    state->sck_pin = impl->state->config.sck_pin;
    state->current_cs_pin = impl->state->current_device.config.cs_pin;
    state->current_speed = impl->state->current_device.config.speed;
    state->current_mode = impl->state->current_device.config.mode;
    state->current_bit_order = impl->state->current_device.config.bit_order;
    state->tx_count = impl->state->stats.tx_count;
    state->rx_count = impl->state->stats.rx_count;
    state->error_count = impl->state->stats.error_count;
    state->tx_buf_count = impl->state->tx_buf.count;
    state->rx_buf_count = impl->state->rx_buf.count;

    return NX_OK;
}

/**
 * \brief           Inject receive data
 * \details         Injects data into the RX buffer, simulating hardware
 *                  reception. Updates statistics to track received data.
 *                  For testing purposes, this updates rx_count immediately
 *                  rather than waiting for actual read operations.
 */
nx_status_t native_spi_inject_rx_data(uint8_t instance, const uint8_t* data,
                                      size_t len) {
    /* Validate parameters */
    if (data == NULL || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Get implementation */
    nx_spi_impl_t* impl = get_spi_impl(instance);
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
        impl->state->stats.error_count++;
        return NX_ERR_NO_MEMORY;
    }

    /* Update statistics after successful write */
    impl->state->stats.rx_count += (uint32_t)written;

    return NX_OK;
}

/**
 * \brief           Get transmitted data
 * \details         Retrieves data from the TX buffer, simulating what would
 *                  be sent to the hardware. This allows tests to verify
 *                  what data was transmitted.
 */
nx_status_t native_spi_get_tx_data(uint8_t instance, uint8_t* data,
                                   size_t* len) {
    /* Validate parameters */
    if (data == NULL || len == NULL || *len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Get implementation */
    nx_spi_impl_t* impl = get_spi_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Read data from TX buffer */
    size_t read = buffer_read(&impl->state->tx_buf, data, *len);
    *len = read;

    return NX_OK;
}

/**
 * \brief           Reset specific SPI instance
 * \details         Resets the SPI instance to its initial state, clearing
 *                  all configuration, state, and statistics.
 */
nx_status_t native_spi_reset(uint8_t instance) {
    /* Get implementation */
    nx_spi_impl_t* impl = get_spi_impl(instance);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Reset state */
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->busy = false;

    /* Reset device handle */
    memset(&impl->state->current_device, 0, sizeof(nx_spi_device_handle_t));

    /* Reset statistics */
    memset(&impl->state->stats, 0, sizeof(nx_spi_stats_t));

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

    return NX_OK;
}

/**
 * \brief           Reset all SPI instances
 * \details         Iterates through all possible SPI instances and resets
 *                  each one to its initial state.
 */
void native_spi_reset_all(void) {
    for (uint8_t instance = 0; instance < NX_SPI_MAX_INSTANCES; instance++) {
        native_spi_reset(instance);
    }
}
