/**
 * \file            hal_spi_native.c
 * \brief           Native SPI Implementation (Simulation)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This implementation simulates SPI communication for testing purposes
 * on the native platform. It provides loopback functionality and
 * tracks CS state for verification.
 */

#include "hal/hal_spi.h"
#include "native_platform.h"
#include <string.h>

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

#define SPI_BUFFER_SIZE     256

/**
 * \brief           SPI instance state
 */
typedef struct {
    bool                initialized;
    hal_spi_config_t    config;
    hal_spi_callback_t  callback;
    void*               context;
    bool                cs_active;      /**< CS pin state (true = asserted/low) */
    uint8_t             tx_buffer[SPI_BUFFER_SIZE];  /**< Last transmitted data */
    uint8_t             rx_buffer[SPI_BUFFER_SIZE];  /**< Simulated RX data */
    size_t              last_transfer_len;           /**< Length of last transfer */
} spi_state_t;

static spi_state_t spi_instances[HAL_SPI_MAX];

/*===========================================================================*/
/* Public functions - Test helpers                                            */
/*===========================================================================*/

void native_spi_reset_all(void)
{
    memset(spi_instances, 0, sizeof(spi_instances));
}

native_spi_state_t* native_spi_get_state(int instance)
{
    if (instance < 0 || instance >= HAL_SPI_MAX) {
        return NULL;
    }
    return (native_spi_state_t*)&spi_instances[instance];
}

bool native_spi_is_initialized(int instance)
{
    if (instance < 0 || instance >= HAL_SPI_MAX) {
        return false;
    }
    return spi_instances[instance].initialized;
}

bool native_spi_get_cs_state(int instance)
{
    if (instance < 0 || instance >= HAL_SPI_MAX) {
        return false;
    }
    return spi_instances[instance].cs_active;
}

hal_spi_mode_t native_spi_get_mode(int instance)
{
    if (instance < 0 || instance >= HAL_SPI_MAX) {
        return HAL_SPI_MODE_0;
    }
    return spi_instances[instance].config.mode;
}

size_t native_spi_get_last_transfer_len(int instance)
{
    if (instance < 0 || instance >= HAL_SPI_MAX) {
        return 0;
    }
    return spi_instances[instance].last_transfer_len;
}

bool native_spi_inject_rx_data(int instance, const uint8_t* data, size_t len)
{
    if (instance < 0 || instance >= HAL_SPI_MAX || data == NULL) {
        return false;
    }
    if (len > SPI_BUFFER_SIZE) {
        return false;
    }
    memcpy(spi_instances[instance].rx_buffer, data, len);
    return true;
}

size_t native_spi_get_tx_data(int instance, uint8_t* data, size_t max_len)
{
    if (instance < 0 || instance >= HAL_SPI_MAX || data == NULL) {
        return 0;
    }
    size_t len = spi_instances[instance].last_transfer_len;
    if (len > max_len) {
        len = max_len;
    }
    memcpy(data, spi_instances[instance].tx_buffer, len);
    return len;
}

/*===========================================================================*/
/* Public functions - HAL API                                                 */
/*===========================================================================*/

hal_status_t hal_spi_init(hal_spi_instance_t instance,
                          const hal_spi_config_t* config)
{
    if (instance >= HAL_SPI_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    /* Validate SPI mode */
    if (config->mode > HAL_SPI_MODE_3) {
        return HAL_ERROR_INVALID_PARAM;
    }

    spi_state_t* spi = &spi_instances[instance];
    memcpy(&spi->config, config, sizeof(hal_spi_config_t));
    spi->initialized = true;
    spi->callback = NULL;
    spi->context = NULL;
    spi->cs_active = false;
    spi->last_transfer_len = 0;
    memset(spi->tx_buffer, 0, SPI_BUFFER_SIZE);
    memset(spi->rx_buffer, 0xFF, SPI_BUFFER_SIZE);  /* Default RX is 0xFF */

    return HAL_OK;
}

hal_status_t hal_spi_deinit(hal_spi_instance_t instance)
{
    if (instance >= HAL_SPI_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    spi_state_t* spi = &spi_instances[instance];
    spi->initialized = false;
    spi->cs_active = false;
    spi->callback = NULL;
    spi->context = NULL;
    spi->last_transfer_len = 0;

    return HAL_OK;
}

hal_status_t hal_spi_transmit(hal_spi_instance_t instance,
                              const uint8_t* tx_data,
                              size_t len,
                              uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (instance >= HAL_SPI_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (tx_data == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (len == 0 || len > SPI_BUFFER_SIZE) {
        return HAL_ERROR_INVALID_PARAM;
    }

    spi_state_t* spi = &spi_instances[instance];
    if (!spi->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Store transmitted data for testing */
    memcpy(spi->tx_buffer, tx_data, len);
    spi->last_transfer_len = len;

    /* Invoke callback if registered */
    if (spi->callback != NULL) {
        spi->callback(instance, spi->context);
    }

    return HAL_OK;
}

hal_status_t hal_spi_receive(hal_spi_instance_t instance,
                             uint8_t* rx_data,
                             size_t len,
                             uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (instance >= HAL_SPI_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (rx_data == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (len == 0 || len > SPI_BUFFER_SIZE) {
        return HAL_ERROR_INVALID_PARAM;
    }

    spi_state_t* spi = &spi_instances[instance];
    if (!spi->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Return data from RX buffer (pre-injected or default 0xFF) */
    memcpy(rx_data, spi->rx_buffer, len);
    spi->last_transfer_len = len;

    /* Invoke callback if registered */
    if (spi->callback != NULL) {
        spi->callback(instance, spi->context);
    }

    return HAL_OK;
}

hal_status_t hal_spi_transfer(hal_spi_instance_t instance,
                              const uint8_t* tx_data,
                              uint8_t* rx_data,
                              size_t len,
                              uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (instance >= HAL_SPI_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (len == 0 || len > SPI_BUFFER_SIZE) {
        return HAL_ERROR_INVALID_PARAM;
    }

    spi_state_t* spi = &spi_instances[instance];
    if (!spi->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Store transmitted data */
    if (tx_data != NULL) {
        memcpy(spi->tx_buffer, tx_data, len);
    }
    spi->last_transfer_len = len;

    /* Full-duplex simulation: loopback TX to RX */
    if (rx_data != NULL) {
        if (tx_data != NULL) {
            /* Loopback mode: RX receives what TX sends */
            memcpy(rx_data, tx_data, len);
        } else {
            /* No TX data: return pre-injected RX data or 0xFF */
            memcpy(rx_data, spi->rx_buffer, len);
        }
    }

    /* Invoke callback if registered */
    if (spi->callback != NULL) {
        spi->callback(instance, spi->context);
    }

    return HAL_OK;
}

hal_status_t hal_spi_cs_control(hal_spi_instance_t instance, bool active)
{
    if (instance >= HAL_SPI_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    spi_state_t* spi = &spi_instances[instance];
    if (!spi->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Track CS state: active=true means CS is asserted (low) */
    spi->cs_active = active;

    return HAL_OK;
}

hal_status_t hal_spi_set_callback(hal_spi_instance_t instance,
                                  hal_spi_callback_t callback,
                                  void* context)
{
    if (instance >= HAL_SPI_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    spi_state_t* spi = &spi_instances[instance];
    spi->callback = callback;
    spi->context = context;

    return HAL_OK;
}
