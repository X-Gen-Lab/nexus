/**
 * \file            nx_watchdog_types.h
 * \brief           Watchdog type definitions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_WATCHDOG_TYPES_H
#define NX_WATCHDOG_TYPES_H

#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/interface/nx_watchdog.h"
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
 * \brief           Watchdog platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct nx_watchdog_platform_config_s {
    uint8_t index;            /**< Watchdog instance index */
    uint32_t default_timeout; /**< Default timeout in milliseconds */
} nx_watchdog_platform_config_t;

/*---------------------------------------------------------------------------*/
/* Watchdog Configuration Structure                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Watchdog runtime configuration structure
 */
typedef struct nx_watchdog_config_s {
    uint32_t timeout_ms; /**< Timeout in milliseconds */
} nx_watchdog_config_t;

/*---------------------------------------------------------------------------*/
/* Watchdog Statistics Structure                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Watchdog statistics structure
 */
typedef struct nx_watchdog_stats_s {
    uint32_t start_count;   /**< Number of start operations */
    uint32_t stop_count;    /**< Number of stop operations */
    uint32_t feed_count;    /**< Number of feed operations */
    uint32_t timeout_count; /**< Number of timeout events */
} nx_watchdog_stats_t;

/*---------------------------------------------------------------------------*/
/* Watchdog State Structure                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Watchdog state structure
 *
 * Contains runtime state and statistics.
 */
typedef struct nx_watchdog_state_s {
    uint8_t index;                   /**< Watchdog instance index */
    nx_watchdog_config_t config;     /**< Configuration */
    nx_watchdog_stats_t stats;       /**< Statistics */
    bool running;                    /**< Running flag */
    uint64_t last_feed_time_ms;      /**< Last feed timestamp */
    nx_watchdog_callback_t callback; /**< Early warning callback */
    void* user_data;                 /**< User data for callback */
    bool initialized;                /**< Initialization flag */
    bool suspended;                  /**< Suspend flag */
} nx_watchdog_state_t;

/*---------------------------------------------------------------------------*/
/* Watchdog Implementation Structure                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Watchdog implementation structure
 *
 * Contains Watchdog interface and state pointer.
 */
typedef struct nx_watchdog_impl_s {
    nx_watchdog_t base;         /**< Base Watchdog interface */
    nx_lifecycle_t lifecycle;   /**< Lifecycle interface */
    nx_power_t power;           /**< Power interface */
    nx_watchdog_state_t* state; /**< State pointer */
    nx_device_t* device;        /**< Device descriptor */
} nx_watchdog_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_WATCHDOG_TYPES_H */
