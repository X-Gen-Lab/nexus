/**
 * \file            hal_i2c.h
 * \brief           HAL I2C Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef HAL_I2C_H
#define HAL_I2C_H

#include "hal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        HAL_I2C I2C Hardware Abstraction
 * \brief           I2C interface for hardware abstraction
 * \{
 */

/**
 * \brief           I2C instance enumeration
 */
typedef enum {
    HAL_I2C_0 = 0, /**< I2C instance 0 */
    HAL_I2C_1,     /**< I2C instance 1 */
    HAL_I2C_2,     /**< I2C instance 2 */
    HAL_I2C_MAX    /**< Maximum I2C count */
} hal_i2c_instance_t;

/**
 * \brief           I2C speed mode
 */
typedef enum {
    HAL_I2C_SPEED_STANDARD = 0, /**< Standard mode (100 kHz) */
    HAL_I2C_SPEED_FAST,         /**< Fast mode (400 kHz) */
    HAL_I2C_SPEED_FAST_PLUS     /**< Fast mode plus (1 MHz) */
} hal_i2c_speed_t;

/**
 * \brief           I2C address mode
 */
typedef enum {
    HAL_I2C_ADDR_7BIT = 0, /**< 7-bit addressing */
    HAL_I2C_ADDR_10BIT     /**< 10-bit addressing */
} hal_i2c_addr_mode_t;

/**
 * \brief           I2C configuration structure
 */
typedef struct {
    hal_i2c_speed_t speed;         /**< I2C speed mode */
    hal_i2c_addr_mode_t addr_mode; /**< Address mode */
    uint16_t own_addr;             /**< Own address (for slave mode) */
} hal_i2c_config_t;

/**
 * \brief           I2C event callback
 * \param[in]       instance: I2C instance
 * \param[in]       event: Event type
 * \param[in]       context: User context
 */
typedef void (*hal_i2c_callback_t)(hal_i2c_instance_t instance, uint32_t event,
                                   void* context);

/**
 * \brief           Initialize I2C
 * \param[in]       instance: I2C instance
 * \param[in]       config: Pointer to configuration structure
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_i2c_init(hal_i2c_instance_t instance,
                          const hal_i2c_config_t* config);

/**
 * \brief           Deinitialize I2C
 * \param[in]       instance: I2C instance
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_i2c_deinit(hal_i2c_instance_t instance);

/**
 * \brief           I2C master transmit (blocking)
 * \param[in]       instance: I2C instance
 * \param[in]       dev_addr: Device address (7-bit or 10-bit)
 * \param[in]       data: Pointer to data buffer
 * \param[in]       len: Number of bytes to transmit
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_i2c_master_transmit(hal_i2c_instance_t instance,
                                     uint16_t dev_addr, const uint8_t* data,
                                     size_t len, uint32_t timeout_ms);

/**
 * \brief           I2C master receive (blocking)
 * \param[in]       instance: I2C instance
 * \param[in]       dev_addr: Device address (7-bit or 10-bit)
 * \param[out]      data: Pointer to receive buffer
 * \param[in]       len: Number of bytes to receive
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_i2c_master_receive(hal_i2c_instance_t instance,
                                    uint16_t dev_addr, uint8_t* data,
                                    size_t len, uint32_t timeout_ms);

/**
 * \brief           I2C memory write (blocking)
 * \param[in]       instance: I2C instance
 * \param[in]       dev_addr: Device address
 * \param[in]       mem_addr: Memory address
 * \param[in]       mem_addr_size: Memory address size (1 or 2 bytes)
 * \param[in]       data: Pointer to data buffer
 * \param[in]       len: Number of bytes to write
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_i2c_mem_write(hal_i2c_instance_t instance, uint16_t dev_addr,
                               uint16_t mem_addr, uint8_t mem_addr_size,
                               const uint8_t* data, size_t len,
                               uint32_t timeout_ms);

/**
 * \brief           I2C memory read (blocking)
 * \param[in]       instance: I2C instance
 * \param[in]       dev_addr: Device address
 * \param[in]       mem_addr: Memory address
 * \param[in]       mem_addr_size: Memory address size (1 or 2 bytes)
 * \param[out]      data: Pointer to receive buffer
 * \param[in]       len: Number of bytes to read
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_i2c_mem_read(hal_i2c_instance_t instance, uint16_t dev_addr,
                              uint16_t mem_addr, uint8_t mem_addr_size,
                              uint8_t* data, size_t len, uint32_t timeout_ms);

/**
 * \brief           Check if device is ready
 * \param[in]       instance: I2C instance
 * \param[in]       dev_addr: Device address
 * \param[in]       retries: Number of retries
 * \param[in]       timeout_ms: Timeout per retry in milliseconds
 * \return          HAL_OK if device responds, error code otherwise
 */
hal_status_t hal_i2c_is_device_ready(hal_i2c_instance_t instance,
                                     uint16_t dev_addr, uint8_t retries,
                                     uint32_t timeout_ms);

/**
 * \brief           Register event callback
 * \param[in]       instance: I2C instance
 * \param[in]       callback: Callback function
 * \param[in]       context: User context
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_i2c_set_callback(hal_i2c_instance_t instance,
                                  hal_i2c_callback_t callback, void* context);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C_H */
