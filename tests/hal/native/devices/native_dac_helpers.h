/**
 * \file            native_dac_helpers.h
 * \brief           DAC test helper functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NATIVE_DAC_HELPERS_H
#define NATIVE_DAC_HELPERS_H

#include "hal/nx_status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Type Definitions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           DAC state structure for testing
 */
typedef struct {
    bool initialized;      /**< Initialization flag */
    bool suspended;        /**< Suspend flag */
    bool clock_enabled;    /**< Clock enable flag */
    uint8_t channel_count; /**< Number of channels */
    uint32_t resolution;   /**< Resolution in bits */
    uint32_t vref_mv;      /**< Reference voltage in mV */
} native_dac_state_t;

/*---------------------------------------------------------------------------*/
/* Test Helper Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get DAC internal state
 * \param[in]       instance: DAC instance ID
 * \param[out]      state: State structure pointer
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_dac_get_state(uint8_t instance, native_dac_state_t* state);

/**
 * \brief           Get DAC channel output value
 * \param[in]       instance: DAC instance ID
 * \param[in]       channel: Channel index
 * \return          Current output value
 */
uint32_t native_dac_get_output_value(uint8_t instance, uint8_t channel);

/**
 * \brief           Reset DAC instance to initial state
 * \param[in]       instance: DAC instance ID
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_dac_reset(uint8_t instance);

/**
 * \brief           Reset all DAC instances
 */
void native_dac_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_DAC_HELPERS_H */
