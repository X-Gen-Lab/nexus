/**
 * \file            nx_crc_types.h
 * \brief           CRC type definitions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_CRC_TYPES_H
#define NX_CRC_TYPES_H

#include "hal/interface/nx_crc.h"
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
/* CRC Algorithm Types                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           CRC algorithm type
 */
typedef enum {
    NX_CRC_ALGO_CRC32 = 0, /**< CRC-32 (IEEE 802.3) */
    NX_CRC_ALGO_CRC16,     /**< CRC-16 (CCITT) */
} nx_crc_algo_t;

/*---------------------------------------------------------------------------*/
/* Platform Configuration Structure                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           CRC platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct nx_crc_platform_config_s {
    uint8_t index;           /**< CRC instance index */
    nx_crc_algo_t algorithm; /**< Default CRC algorithm */
    uint32_t polynomial;     /**< CRC polynomial */
    uint32_t init_value;     /**< Initial CRC value */
    uint32_t final_xor;      /**< Final XOR value */
} nx_crc_platform_config_t;

/*---------------------------------------------------------------------------*/
/* CRC Configuration Structure                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           CRC runtime configuration structure
 */
typedef struct nx_crc_config_s {
    nx_crc_algo_t algorithm; /**< CRC algorithm */
    uint32_t polynomial;     /**< CRC polynomial */
    uint32_t init_value;     /**< Initial CRC value */
    uint32_t final_xor;      /**< Final XOR value */
} nx_crc_config_t;

/*---------------------------------------------------------------------------*/
/* CRC Statistics Structure                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           CRC statistics structure
 */
typedef struct nx_crc_stats_s {
    uint32_t reset_count;     /**< Number of reset operations */
    uint32_t update_count;    /**< Number of update operations */
    uint32_t calculate_count; /**< Number of calculate operations */
    uint32_t bytes_processed; /**< Total bytes processed */
} nx_crc_stats_t;

/*---------------------------------------------------------------------------*/
/* CRC State Structure                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           CRC state structure
 *
 * Contains runtime state and statistics.
 */
typedef struct nx_crc_state_s {
    uint8_t index;          /**< CRC instance index */
    nx_crc_config_t config; /**< Configuration */
    nx_crc_stats_t stats;   /**< Statistics */
    uint32_t current_crc;   /**< Current CRC value */
    bool initialized;       /**< Initialization flag */
    bool suspended;         /**< Suspend flag */
} nx_crc_state_t;

/*---------------------------------------------------------------------------*/
/* CRC Implementation Structure                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           CRC implementation structure
 *
 * Contains CRC interface and state pointer.
 */
typedef struct nx_crc_impl_s {
    nx_crc_t base;            /**< Base CRC interface */
    nx_lifecycle_t lifecycle; /**< Lifecycle interface */
    nx_power_t power;         /**< Power interface */
    nx_crc_state_t* state;    /**< State pointer */
    nx_device_t* device;      /**< Device descriptor */
} nx_crc_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_CRC_TYPES_H */
