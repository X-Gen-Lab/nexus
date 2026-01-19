/**
 * \file            nx_dac_types.h
 * \brief           DAC type definitions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_DAC_TYPES_H
#define NX_DAC_TYPES_H

#include "hal/interface/nx_dac.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
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
 * \brief           DAC platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct nx_dac_platform_config_s {
    uint8_t dac_index;     /**< DAC instance index */
    uint8_t channel_count; /**< Number of DAC channels */
    uint32_t resolution;   /**< DAC resolution in bits */
    uint32_t vref_mv;      /**< Reference voltage in millivolts */
} nx_dac_platform_config_t;

/*---------------------------------------------------------------------------*/
/* DAC Channel Implementation Structure                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           DAC channel implementation structure
 */
typedef struct {
    nx_dac_channel_t base;    /**< Base channel interface */
    uint8_t channel_index;    /**< Channel index */
    uint32_t current_value;   /**< Current output value */
    uint32_t vref_mv;         /**< Reference voltage in mV */
    uint32_t resolution_bits; /**< Resolution in bits */
} nx_dac_channel_impl_t;

/*---------------------------------------------------------------------------*/
/* DAC Configuration Structure                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           DAC runtime configuration structure
 */
typedef struct nx_dac_config_s {
    uint8_t channel_count; /**< Number of DAC channels */
    uint32_t resolution;   /**< DAC resolution in bits */
    uint32_t vref_mv;      /**< Reference voltage in millivolts */
} nx_dac_config_t;

/*---------------------------------------------------------------------------*/
/* DAC State Structure                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           DAC state structure
 *
 * Contains runtime state.
 */
typedef struct nx_dac_state_s {
    uint8_t index;          /**< Instance index */
    nx_dac_config_t config; /**< Configuration */
    bool initialized;       /**< Initialization flag */
    bool suspended;         /**< Suspend flag */
    bool clock_enabled;     /**< Clock enable flag */
} nx_dac_state_t;

/*---------------------------------------------------------------------------*/
/* DAC Implementation Structure                                              */
/*---------------------------------------------------------------------------*/

#define NX_DAC_MAX_CHANNELS 4

/**
 * \brief           DAC implementation structure
 *
 * Contains all interfaces and state pointer.
 */
typedef struct nx_dac_impl_s {
    nx_dac_t base;            /**< Base DAC interface */
    nx_lifecycle_t lifecycle; /**< Lifecycle interface */
    nx_power_t power;         /**< Power interface */
    nx_dac_state_t* state;    /**< State pointer */
    nx_device_t* device;      /**< Device descriptor */
    nx_dac_channel_impl_t
        channels[NX_DAC_MAX_CHANNELS]; /**< Channel instances */
} nx_dac_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_DAC_TYPES_H */
