/**
 * \file            nx_sdio_types.h
 * \brief           SDIO type definitions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_SDIO_TYPES_H
#define NX_SDIO_TYPES_H

#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/interface/nx_sdio.h"
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
/* SDIO Constants                                                            */
/*---------------------------------------------------------------------------*/

#define NX_SDIO_BLOCK_SIZE 512  /**< Standard SD block size */
#define NX_SDIO_NUM_BLOCKS 1024 /**< Number of blocks (512KB total) */

/*---------------------------------------------------------------------------*/
/* Platform Configuration Structure                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SDIO platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct nx_sdio_platform_config_s {
    uint8_t sdio_index;   /**< SDIO instance index */
    uint8_t bus_width;    /**< Bus width (1, 4, or 8 bits) */
    uint32_t clock_speed; /**< Clock speed in Hz */
    uint32_t block_size;  /**< Block size in bytes */
    uint32_t num_blocks;  /**< Number of blocks */
    bool card_present;    /**< Initial card present state */
} nx_sdio_platform_config_t;

/*---------------------------------------------------------------------------*/
/* SDIO Configuration Structure                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SDIO runtime configuration structure
 */
typedef struct nx_sdio_config_s {
    uint32_t clock_speed; /**< Clock speed in Hz */
    uint8_t bus_width;    /**< Bus width (1, 4, or 8) */
} nx_sdio_config_t;

/*---------------------------------------------------------------------------*/
/* SDIO Block Structure                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SDIO block structure
 */
typedef struct nx_sdio_block_s {
    uint8_t data[NX_SDIO_BLOCK_SIZE]; /**< Block data */
} nx_sdio_block_t;

/*---------------------------------------------------------------------------*/
/* SDIO Statistics Structure                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SDIO statistics structure
 */
typedef struct nx_sdio_stats_s {
    uint32_t read_count;  /**< Number of read operations */
    uint32_t write_count; /**< Number of write operations */
    uint32_t erase_count; /**< Number of erase operations */
    uint32_t error_count; /**< Number of errors */
} nx_sdio_stats_t;

/*---------------------------------------------------------------------------*/
/* SDIO State Structure                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SDIO state structure
 *
 * Contains runtime state and statistics.
 */
typedef struct nx_sdio_state_s {
    uint8_t index;           /**< SDIO instance index */
    nx_sdio_config_t config; /**< Configuration */
    nx_sdio_stats_t stats;   /**< Statistics */
    bool card_present;       /**< Card present flag */
    nx_sdio_block_t* blocks; /**< Block storage array */
    bool initialized;        /**< Initialization flag */
    bool suspended;          /**< Suspend flag */
} nx_sdio_state_t;

/*---------------------------------------------------------------------------*/
/* SDIO Implementation Structure                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SDIO implementation structure
 *
 * Contains SDIO interface and state pointer.
 */
typedef struct nx_sdio_impl_s {
    nx_sdio_t base;           /**< Base SDIO interface */
    nx_lifecycle_t lifecycle; /**< Lifecycle interface */
    nx_power_t power;         /**< Power interface */
    nx_sdio_state_t* state;   /**< State pointer */
    nx_device_t* device;      /**< Device descriptor */
} nx_sdio_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_SDIO_TYPES_H */
