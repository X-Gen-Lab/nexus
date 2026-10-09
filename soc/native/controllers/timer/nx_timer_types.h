/**
 * \file            nx_timer_types.h
 * \brief           Timer type definitions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_TIMER_TYPES_H
#define NX_TIMER_TYPES_H

#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/interface/nx_timer.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"
#include <stdbool.h>
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
 * \brief           Timer platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct nx_timer_platform_config_s {
    uint8_t timer_index;   /**< Timer instance index */
    uint32_t frequency;    /**< Timer frequency in Hz */
    uint8_t channel_count; /**< Number of PWM channels */
} nx_timer_platform_config_t;

/*---------------------------------------------------------------------------*/
/* Timer Configuration Structure                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Timer runtime configuration structure
 */
typedef struct nx_timer_config_s {
    uint32_t frequency;    /**< Timer frequency in Hz */
    uint16_t prescaler;    /**< Prescaler value */
    uint32_t period;       /**< Period value */
    uint8_t channel_count; /**< Number of PWM channels */
} nx_timer_config_t;

/*---------------------------------------------------------------------------*/
/* Timer State Structure                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Timer state structure
 *
 * Contains runtime state.
 */
typedef struct nx_timer_state_s {
    uint8_t index;                /**< Instance index */
    nx_timer_config_t config;     /**< Configuration */
    uint32_t counter;             /**< Current counter value */
    bool running;                 /**< Running flag */
    bool initialized;             /**< Initialization flag */
    bool suspended;               /**< Suspend flag */
    nx_timer_callback_t callback; /**< Timer callback */
    void* user_data;              /**< User context pointer */
} nx_timer_state_t;

/*---------------------------------------------------------------------------*/
/* Timer Implementation Structure                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Timer implementation structure
 *
 * Contains all interfaces and state pointer.
 */
typedef struct nx_timer_impl_s {
    nx_timer_base_t base;     /**< Base timer interface */
    nx_lifecycle_t lifecycle; /**< Lifecycle interface */
    nx_power_t power;         /**< Power interface */
    nx_timer_state_t* state;  /**< State pointer */
    nx_device_t* device;      /**< Device descriptor */
} nx_timer_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_TIMER_TYPES_H */
