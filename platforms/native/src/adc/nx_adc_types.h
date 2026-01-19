/**
 * \file            nx_adc_types.h
 * \brief           ADC type definitions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_ADC_TYPES_H
#define NX_ADC_TYPES_H

#include "hal/interface/nx_adc.h"
#include "hal/interface/nx_diagnostic.h"
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
 * \brief           ADC platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct nx_adc_platform_config_s {
    uint8_t adc_index;     /**< ADC instance index */
    uint8_t channel_count; /**< Number of ADC channels */
    uint32_t resolution;   /**< ADC resolution in bits */
} nx_adc_platform_config_t;

/*---------------------------------------------------------------------------*/
/* ADC Channel Implementation Structure                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           ADC channel implementation structure
 */
typedef struct {
    nx_adc_channel_t base;    /**< Base channel interface */
    uint8_t channel_index;    /**< Channel index */
    uint16_t simulated_value; /**< Simulated conversion value */
} nx_adc_channel_impl_t;

/*---------------------------------------------------------------------------*/
/* ADC Configuration Structure                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           ADC runtime configuration structure
 */
typedef struct nx_adc_config_s {
    uint8_t channel_count; /**< Number of ADC channels */
    uint32_t resolution;   /**< ADC resolution in bits */
} nx_adc_config_t;

/*---------------------------------------------------------------------------*/
/* ADC Statistics Structure                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           ADC statistics structure
 */
typedef struct nx_adc_stats_s {
    uint32_t conversion_count; /**< Total conversions */
    uint32_t error_count;      /**< Error count */
} nx_adc_stats_t;

/*---------------------------------------------------------------------------*/
/* ADC State Structure                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           ADC state structure
 *
 * Contains runtime state.
 */
typedef struct nx_adc_state_s {
    uint8_t index;          /**< Instance index */
    nx_adc_config_t config; /**< Configuration */
    nx_adc_stats_t stats;   /**< Statistics */
    bool initialized;       /**< Initialization flag */
    bool suspended;         /**< Suspend flag */
    bool clock_enabled;     /**< Clock enable flag */
} nx_adc_state_t;

/*---------------------------------------------------------------------------*/
/* ADC Implementation Structure                                              */
/*---------------------------------------------------------------------------*/

#define NX_ADC_MAX_CHANNELS 16

/**
 * \brief           ADC implementation structure
 *
 * Contains all interfaces and state pointer.
 */
typedef struct nx_adc_impl_s {
    nx_adc_t base;              /**< Base ADC interface */
    nx_lifecycle_t lifecycle;   /**< Lifecycle interface */
    nx_power_t power;           /**< Power interface */
    nx_diagnostic_t diagnostic; /**< Diagnostic interface */
    nx_adc_state_t* state;      /**< State pointer */
    nx_device_t* device;        /**< Device descriptor */
    nx_adc_channel_impl_t
        channels[NX_ADC_MAX_CHANNELS]; /**< Channel instances */
} nx_adc_impl_t;

/*---------------------------------------------------------------------------*/
/* ADC Buffer Platform Configuration Structure                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           ADC Buffer platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct nx_adc_buffer_platform_config_s {
    uint8_t adc_index;     /**< ADC instance index */
    uint8_t channel_count; /**< Number of ADC channels */
    size_t buffer_size;    /**< Buffer size in samples */
} nx_adc_buffer_platform_config_t;

/*---------------------------------------------------------------------------*/
/* ADC Buffer State Structure                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           ADC buffer state structure
 */
typedef struct nx_adc_buffer_state_s {
    uint8_t index;                     /**< Instance index */
    uint8_t channel_count;             /**< Number of channels */
    bool initialized;                  /**< Initialization flag */
    bool clock_enabled;                /**< Clock enable flag */
    bool sampling_active;              /**< Sampling active flag */
    uint32_t* buffer;                  /**< Sample buffer */
    size_t buffer_size;                /**< Buffer size in samples */
    size_t current_index;              /**< Current buffer index */
    nx_adc_buffer_callback_t callback; /**< Buffer-full callback */
    void* user_data;                   /**< User data for callback */
} nx_adc_buffer_state_t;

/*---------------------------------------------------------------------------*/
/* ADC Buffer Implementation Structure                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           ADC buffer implementation structure
 */
typedef struct nx_adc_buffer_impl_s {
    nx_adc_buffer_t base;         /**< Base ADC buffer interface */
    nx_lifecycle_t lifecycle;     /**< Lifecycle interface */
    nx_power_t power;             /**< Power interface */
    nx_adc_buffer_state_t* state; /**< State pointer */
    nx_device_t* device;          /**< Device descriptor */
} nx_adc_buffer_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* NX_ADC_TYPES_H */
