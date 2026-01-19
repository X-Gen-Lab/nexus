/**
 * \file            nx_i2c_types.h
 * \brief           I2C type definitions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_I2C_TYPES_H
#define NX_I2C_TYPES_H

#include "hal/base/nx_comm.h"
#include "hal/interface/nx_diagnostic.h"
#include "hal/interface/nx_i2c.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Forward declare device type */
typedef struct nx_device_s nx_device_t;

/*---------------------------------------------------------------------------*/
/* Platform Configuration Structure                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           I2C platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct nx_i2c_platform_config_s {
    uint8_t i2c_index;  /**< I2C instance index */
    uint32_t speed;     /**< I2C speed (Hz) */
    uint8_t scl_pin;    /**< SCL pin number */
    uint8_t sda_pin;    /**< SDA pin number */
    size_t tx_buf_size; /**< TX buffer size */
    size_t rx_buf_size; /**< RX buffer size */
} nx_i2c_platform_config_t;

/*---------------------------------------------------------------------------*/
/* Circular Buffer Structure                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Circular buffer structure
 *
 * Used for TX and RX buffering.
 */
typedef struct nx_i2c_buffer_s {
    uint8_t* data; /**< Buffer data pointer */
    size_t size;   /**< Buffer size */
    size_t head;   /**< Write position */
    size_t tail;   /**< Read position */
    size_t count;  /**< Number of bytes in buffer */
} nx_i2c_buffer_t;

/*---------------------------------------------------------------------------*/
/* I2C Configuration Structure                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           I2C runtime configuration structure
 */
typedef struct nx_i2c_config_s {
    uint32_t speed;     /**< I2C speed (Hz) */
    uint8_t scl_pin;    /**< SCL pin number */
    uint8_t sda_pin;    /**< SDA pin number */
    bool dma_tx_enable; /**< DMA TX enable flag */
    bool dma_rx_enable; /**< DMA RX enable flag */
    size_t tx_buf_size; /**< TX buffer size */
    size_t rx_buf_size; /**< RX buffer size */
} nx_i2c_config_t;

/*---------------------------------------------------------------------------*/
/* I2C Device Handle Structure                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           I2C device handle structure
 *
 * Stores device-specific configuration for handle acquisition pattern.
 */
typedef struct nx_i2c_device_handle_s {
    uint8_t dev_addr;            /**< Device address */
    nx_comm_callback_t callback; /**< Callback for async operations */
    void* user_data;             /**< User data for callback */
    bool in_use;                 /**< Handle in use flag */
} nx_i2c_device_handle_t;

/*---------------------------------------------------------------------------*/
/* I2C State Structure                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           I2C state structure
 *
 * Contains runtime state and statistics.
 */
typedef struct nx_i2c_state_s {
    uint8_t index;                         /**< Instance index */
    nx_i2c_config_t config;                /**< Configuration */
    nx_i2c_stats_t stats;                  /**< Statistics */
    nx_i2c_buffer_t tx_buf;                /**< TX buffer */
    nx_i2c_buffer_t rx_buf;                /**< RX buffer */
    nx_i2c_device_handle_t current_device; /**< Current device handle */
    bool initialized;                      /**< Initialization flag */
    bool suspended;                        /**< Suspend flag */
    bool busy;                             /**< Busy flag */
} nx_i2c_state_t;

/*---------------------------------------------------------------------------*/
/* I2C Implementation Structure                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           I2C implementation structure
 *
 * Contains all interfaces and state pointer.
 */
typedef struct nx_i2c_impl_s {
    nx_i2c_bus_t base;            /**< Base I2C bus interface */
    nx_tx_async_t tx_async;       /**< TX async interface */
    nx_tx_rx_async_t tx_rx_async; /**< TX/RX async interface */
    nx_tx_sync_t tx_sync;         /**< TX sync interface */
    nx_tx_rx_sync_t tx_rx_sync;   /**< TX/RX sync interface */
    nx_lifecycle_t lifecycle;     /**< Lifecycle interface */
    nx_power_t power;             /**< Power interface */
    nx_diagnostic_t diagnostic;   /**< Diagnostic interface */
    nx_i2c_state_t* state;        /**< State pointer */
    nx_device_t* device;          /**< Device descriptor */
} nx_i2c_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_I2C_TYPES_H */
