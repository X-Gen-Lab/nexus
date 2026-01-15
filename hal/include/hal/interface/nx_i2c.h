/**
 * \file            nx_i2c.h
 * \brief           I2C device interface definition
 * \author          Nexus Team
 */

#ifndef NX_I2C_H
#define NX_I2C_H

#include "hal/interface/nx_diagnostic.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           I2C speed enumeration
 */
typedef enum nx_i2c_speed_e {
    NX_I2C_SPEED_STANDARD = 0, /**< Standard mode: 100 kHz */
    NX_I2C_SPEED_FAST,         /**< Fast mode: 400 kHz */
    NX_I2C_SPEED_FAST_PLUS,    /**< Fast mode plus: 1 MHz */
} nx_i2c_speed_t;

/**
 * \brief           I2C configuration structure
 */
typedef struct nx_i2c_config_s {
    nx_i2c_speed_t speed; /**< I2C speed */
    uint16_t own_addr;    /**< Own address (slave mode) */
    bool addr_10bit;      /**< 10-bit address mode flag */
} nx_i2c_config_t;

/**
 * \brief           I2C statistics structure
 */
typedef struct nx_i2c_stats_s {
    bool busy;                /**< Busy flag */
    uint32_t tx_count;        /**< Total bytes transmitted */
    uint32_t rx_count;        /**< Total bytes received */
    uint32_t nack_count;      /**< NACK count */
    uint32_t bus_error_count; /**< Bus error count */
} nx_i2c_stats_t;

/**
 * \brief           I2C bus interface
 */
typedef struct nx_i2c_s nx_i2c_t;
struct nx_i2c_s {
    /* Master transfer */
    nx_status_t (*master_transmit)(nx_i2c_t* self, uint16_t addr,
                                   const uint8_t* data, size_t len,
                                   uint32_t timeout_ms);
    nx_status_t (*master_receive)(nx_i2c_t* self, uint16_t addr, uint8_t* data,
                                  size_t len, uint32_t timeout_ms);

    /* Memory read/write */
    nx_status_t (*mem_write)(nx_i2c_t* self, uint16_t addr, uint16_t mem_addr,
                             uint8_t mem_addr_size, const uint8_t* data,
                             size_t len, uint32_t timeout_ms);
    nx_status_t (*mem_read)(nx_i2c_t* self, uint16_t addr, uint16_t mem_addr,
                            uint8_t mem_addr_size, uint8_t* data, size_t len,
                            uint32_t timeout_ms);

    /* Device detection */
    nx_status_t (*probe)(nx_i2c_t* self, uint16_t addr, uint32_t timeout_ms);
    nx_status_t (*scan)(nx_i2c_t* self, uint8_t* addr_list, size_t max,
                        size_t* found);

    /* Runtime configuration */
    nx_status_t (*set_speed)(nx_i2c_t* self, nx_i2c_speed_t speed);
    nx_status_t (*get_config)(nx_i2c_t* self, nx_i2c_config_t* cfg);
    nx_status_t (*set_config)(nx_i2c_t* self, const nx_i2c_config_t* cfg);

    /* Base interfaces */
    nx_lifecycle_t* (*get_lifecycle)(nx_i2c_t* self);
    nx_power_t* (*get_power)(nx_i2c_t* self);
    nx_diagnostic_t* (*get_diagnostic)(nx_i2c_t* self);

    /* Diagnostics */
    nx_status_t (*get_stats)(nx_i2c_t* self, nx_i2c_stats_t* stats);
};

#ifdef __cplusplus
}
#endif

#endif /* NX_I2C_H */
