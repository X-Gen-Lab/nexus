/**
 * \file            hal_spi_native.c
 * \brief           Native SPI Implementation (Simulation)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/hal_spi.h"
#include <string.h>

/**
 * \brief           SPI instance state
 */
typedef struct {
    bool                initialized;
    hal_spi_config_t    config;
    hal_spi_callback_t  callback;
    void*               context;
} spi_state_t;

static spi_state_t spi_instances[HAL_SPI_MAX];

hal_status_t hal_spi_init(hal_spi_instance_t instance,
                          const hal_spi_config_t* config)
{
    if (instance >= HAL_SPI_MAX || config == NULL) {
        return HAL_ERR_PARAM;
    }

    spi_state_t* spi = &spi_instances[instance];
    memcpy(&spi->config, config, sizeof(hal_spi_config_t));
    spi->initialized = true;
    spi->callback = NULL;
    spi->context = NULL;

    return HAL_OK;
}

hal_status_t hal_spi_deinit(hal_spi_instance_t instance)
{
    if (instance >= HAL_SPI_MAX) {
        return HAL_ERR_PARAM;
    }

    spi_instances[instance].initialized = false;
    return HAL_OK;
}

hal_status_t hal_spi_transmit(hal_spi_instance_t instance,
                              const uint8_t* tx_data,
                              size_t len,
                              uint32_t timeout_ms)
{
    if (instance >= HAL_SPI_MAX || tx_data == NULL || len == 0) {
        return HAL_ERR_PARAM;
    }

    if (!spi_instances[instance].initialized) {
        return HAL_ERR_STATE;
    }

    (void)timeout_ms;
    /* Native simulation: data is "transmitted" */
    return HAL_OK;
}

hal_status_t hal_spi_receive(hal_spi_instance_t instance,
                             uint8_t* rx_data,
                             size_t len,
                             uint32_t timeout_ms)
{
    if (instance >= HAL_SPI_MAX || rx_data == NULL || len == 0) {
        return HAL_ERR_PARAM;
    }

    if (!spi_instances[instance].initialized) {
        return HAL_ERR_STATE;
    }

    (void)timeout_ms;
    /* Native simulation: fill with dummy data */
    memset(rx_data, 0xFF, len);
    return HAL_OK;
}

hal_status_t hal_spi_transfer(hal_spi_instance_t instance,
                              const uint8_t* tx_data,
                              uint8_t* rx_data,
                              size_t len,
                              uint32_t timeout_ms)
{
    if (instance >= HAL_SPI_MAX || len == 0) {
        return HAL_ERR_PARAM;
    }

    if (!spi_instances[instance].initialized) {
        return HAL_ERR_STATE;
    }

    (void)timeout_ms;
    /* Native simulation: loopback */
    if (tx_data != NULL && rx_data != NULL) {
        memcpy(rx_data, tx_data, len);
    } else if (rx_data != NULL) {
        memset(rx_data, 0xFF, len);
    }

    return HAL_OK;
}

hal_status_t hal_spi_cs_control(hal_spi_instance_t instance, bool active)
{
    if (instance >= HAL_SPI_MAX) {
        return HAL_ERR_PARAM;
    }

    if (!spi_instances[instance].initialized) {
        return HAL_ERR_STATE;
    }

    (void)active;
    return HAL_OK;
}

hal_status_t hal_spi_set_callback(hal_spi_instance_t instance,
                                  hal_spi_callback_t callback,
                                  void* context)
{
    if (instance >= HAL_SPI_MAX) {
        return HAL_ERR_PARAM;
    }

    spi_instances[instance].callback = callback;
    spi_instances[instance].context = context;
    return HAL_OK;
}
