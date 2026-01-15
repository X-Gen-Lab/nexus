/**
 * \file            nx_spi.h
 * \brief           SPI device interface definition
 * \author          Nexus Team
 */

#ifndef NX_SPI_H
#define NX_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal/interface/nx_diagnostic.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

/**
 * \brief           SPI mode enumeration
 */
typedef enum nx_spi_mode_e {
    NX_SPI_MODE_0 = 0, /**< CPOL=0, CPHA=0 */
    NX_SPI_MODE_1,     /**< CPOL=0, CPHA=1 */
    NX_SPI_MODE_2,     /**< CPOL=1, CPHA=0 */
    NX_SPI_MODE_3,     /**< CPOL=1, CPHA=1 */
} nx_spi_mode_t;

/**
 * \brief           SPI configuration structure
 */
typedef struct nx_spi_config_s {
    uint32_t clock_hz;    /**< Clock frequency in Hz */
    nx_spi_mode_t mode;   /**< SPI mode */
    uint8_t bits;         /**< Data bits: 8 or 16 */
    bool msb_first;       /**< MSB first flag */
    uint32_t cs_delay_us; /**< CS delay in microseconds */
} nx_spi_config_t;

/**
 * \brief           SPI statistics structure
 */
typedef struct nx_spi_stats_s {
    bool busy;            /**< Busy flag */
    uint32_t tx_count;    /**< Total bytes transmitted */
    uint32_t rx_count;    /**< Total bytes received */
    uint32_t error_count; /**< Error count */
} nx_spi_stats_t;

/**
 * \brief           SPI bus interface
 */
typedef struct nx_spi_s nx_spi_t;
struct nx_spi_s {
    /* Synchronous transfer */
    nx_status_t (*transfer)(nx_spi_t* self, const uint8_t* tx, uint8_t* rx,
                            size_t len, uint32_t timeout_ms);
    nx_status_t (*transmit)(nx_spi_t* self, const uint8_t* tx, size_t len,
                            uint32_t timeout_ms);
    nx_status_t (*receive)(nx_spi_t* self, uint8_t* rx, size_t len,
                           uint32_t timeout_ms);

    /* CS control */
    nx_status_t (*cs_select)(nx_spi_t* self);
    nx_status_t (*cs_deselect)(nx_spi_t* self);

    /* Bus lock */
    nx_status_t (*lock)(nx_spi_t* self, uint32_t timeout_ms);
    nx_status_t (*unlock)(nx_spi_t* self);

    /* Runtime configuration */
    nx_status_t (*set_clock)(nx_spi_t* self, uint32_t clock_hz);
    nx_status_t (*set_mode)(nx_spi_t* self, nx_spi_mode_t mode);
    nx_status_t (*get_config)(nx_spi_t* self, nx_spi_config_t* cfg);
    nx_status_t (*set_config)(nx_spi_t* self, const nx_spi_config_t* cfg);

    /* Base interfaces */
    nx_lifecycle_t* (*get_lifecycle)(nx_spi_t* self);
    nx_power_t* (*get_power)(nx_spi_t* self);
    nx_diagnostic_t* (*get_diagnostic)(nx_spi_t* self);

    /* Diagnostics */
    nx_status_t (*get_stats)(nx_spi_t* self, nx_spi_stats_t* stats);
};

#ifdef __cplusplus
}
#endif

#endif /* NX_SPI_H */
