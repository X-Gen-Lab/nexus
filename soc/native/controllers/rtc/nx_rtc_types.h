/**
 * \file            nx_rtc_types.h
 * \brief           RTC type definitions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_RTC_TYPES_H
#define NX_RTC_TYPES_H

#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/interface/nx_rtc.h"
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
 * \brief           RTC platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct nx_rtc_platform_config_s {
    uint8_t rtc_index;    /**< RTC instance index */
    bool enable_alarm;    /**< Enable alarm functionality */
    uint32_t alarm_count; /**< Maximum number of alarms */
} nx_rtc_platform_config_t;

/*---------------------------------------------------------------------------*/
/* RTC Configuration Structure                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           RTC runtime configuration structure
 */
typedef struct nx_rtc_config_s {
    bool enable_alarm; /**< Enable alarm functionality */
} nx_rtc_config_t;

/*---------------------------------------------------------------------------*/
/* RTC Alarm Structure                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           RTC alarm structure
 */
typedef struct nx_rtc_alarm_s {
    bool enabled;                     /**< Alarm enabled flag */
    nx_datetime_t alarm_time;         /**< Alarm date/time */
    nx_rtc_alarm_callback_t callback; /**< Alarm callback */
    void* user_data;                  /**< User data for callback */
} nx_rtc_alarm_t;

/*---------------------------------------------------------------------------*/
/* RTC Statistics Structure                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           RTC statistics structure
 */
typedef struct nx_rtc_stats_s {
    uint32_t set_time_count;      /**< Number of set time operations */
    uint32_t get_time_count;      /**< Number of get time operations */
    uint32_t set_alarm_count;     /**< Number of set alarm operations */
    uint32_t alarm_trigger_count; /**< Number of alarm triggers */
} nx_rtc_stats_t;

/*---------------------------------------------------------------------------*/
/* RTC State Structure                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           RTC state structure
 *
 * Contains runtime state and statistics.
 */
typedef struct nx_rtc_state_s {
    uint8_t index;               /**< RTC instance index */
    nx_rtc_config_t config;      /**< Configuration */
    nx_rtc_stats_t stats;        /**< Statistics */
    nx_datetime_t current_time;  /**< Current date/time */
    nx_rtc_alarm_t alarm;        /**< Alarm configuration */
    uint64_t start_timestamp_ms; /**< Start timestamp for simulation */
    bool initialized;            /**< Initialization flag */
    bool suspended;              /**< Suspend flag */
} nx_rtc_state_t;

/*---------------------------------------------------------------------------*/
/* RTC Implementation Structure                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           RTC implementation structure
 *
 * Contains RTC interface and state pointer.
 */
typedef struct nx_rtc_impl_s {
    nx_rtc_t base;            /**< Base RTC interface */
    nx_lifecycle_t lifecycle; /**< Lifecycle interface */
    nx_power_t power;         /**< Power interface */
    nx_rtc_state_t* state;    /**< State pointer */
    nx_device_t* device;      /**< Device descriptor */
} nx_rtc_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_RTC_TYPES_H */
