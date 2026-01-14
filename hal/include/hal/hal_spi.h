/**
 * \file            hal_spi.h
 * \brief           HAL SPI Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef HAL_SPI_H
#define HAL_SPI_H

#include "hal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        HAL_SPI SPI Hardware Abstraction
 * \brief           SPI interface for hardware abstraction
 * \{
 */

/**
 * \brief           SPI instance enumeration
 */
typedef enum {
    HAL_SPI_0 = 0, /**< SPI instance 0 */
    HAL_SPI_1,     /**< SPI instance 1 */
    HAL_SPI_2,     /**< SPI instance 2 */
    HAL_SPI_MAX    /**< Maximum SPI count */
} hal_spi_instance_t;

/**
 * \brief           SPI mode (CPOL/CPHA)
 */
typedef enum {
    HAL_SPI_MODE_0 = 0, /**< CPOL=0, CPHA=0 */
    HAL_SPI_MODE_1,     /**< CPOL=0, CPHA=1 */
    HAL_SPI_MODE_2,     /**< CPOL=1, CPHA=0 */
    HAL_SPI_MODE_3      /**< CPOL=1, CPHA=1 */
} hal_spi_mode_t;

/**
 * \brief           SPI bit order
 */
typedef enum {
    HAL_SPI_MSB_FIRST = 0, /**< MSB first */
    HAL_SPI_LSB_FIRST      /**< LSB first */
} hal_spi_bit_order_t;

/**
 * \brief           SPI data width
 */
typedef enum {
    HAL_SPI_DATA_8BIT = 0, /**< 8-bit data */
    HAL_SPI_DATA_16BIT     /**< 16-bit data */
} hal_spi_data_width_t;

/**
 * \brief           SPI role
 */
typedef enum {
    HAL_SPI_ROLE_MASTER = 0, /**< Master mode */
    HAL_SPI_ROLE_SLAVE       /**< Slave mode */
} hal_spi_role_t;

/**
 * \brief           SPI configuration structure
 */
typedef struct {
    uint32_t clock_hz;               /**< Clock frequency in Hz */
    hal_spi_mode_t mode;             /**< SPI mode (CPOL/CPHA) */
    hal_spi_bit_order_t bit_order;   /**< Bit order */
    hal_spi_data_width_t data_width; /**< Data width */
    hal_spi_role_t role;             /**< Master or slave */
} hal_spi_config_t;

/**
 * \brief           SPI transfer complete callback
 * \param[in]       instance: SPI instance
 * \param[in]       context: User context
 */
typedef void (*hal_spi_callback_t)(hal_spi_instance_t instance, void* context);

/**
 * \brief           Initialize SPI
 * \param[in]       instance: SPI instance
 * \param[in]       config: Pointer to configuration structure
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_spi_init(hal_spi_instance_t instance,
                          const hal_spi_config_t* config);

/**
 * \brief           Deinitialize SPI
 * \param[in]       instance: SPI instance
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_spi_deinit(hal_spi_instance_t instance);

/**
 * \brief           SPI transmit (blocking)
 * \param[in]       instance: SPI instance
 * \param[in]       tx_data: Pointer to transmit data
 * \param[in]       len: Number of bytes to transmit
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_spi_transmit(hal_spi_instance_t instance,
                              const uint8_t* tx_data, size_t len,
                              uint32_t timeout_ms);

/**
 * \brief           SPI receive (blocking)
 * \param[in]       instance: SPI instance
 * \param[out]      rx_data: Pointer to receive buffer
 * \param[in]       len: Number of bytes to receive
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_spi_receive(hal_spi_instance_t instance, uint8_t* rx_data,
                             size_t len, uint32_t timeout_ms);

/**
 * \brief           SPI transmit and receive (blocking)
 * \param[in]       instance: SPI instance
 * \param[in]       tx_data: Pointer to transmit data
 * \param[out]      rx_data: Pointer to receive buffer
 * \param[in]       len: Number of bytes to transfer
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_spi_transfer(hal_spi_instance_t instance,
                              const uint8_t* tx_data, uint8_t* rx_data,
                              size_t len, uint32_t timeout_ms);

/**
 * \brief           Set CS pin state (for software CS control)
 * \param[in]       instance: SPI instance
 * \param[in]       active: true to assert CS (low), false to deassert
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_spi_cs_control(hal_spi_instance_t instance, bool active);

/**
 * \brief           Register transfer complete callback
 * \param[in]       instance: SPI instance
 * \param[in]       callback: Callback function
 * \param[in]       context: User context
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_spi_set_callback(hal_spi_instance_t instance,
                                  hal_spi_callback_t callback, void* context);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* HAL_SPI_H */
