/**
 * \file            native_adc_helpers.h
 * \brief           Native ADC test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_ADC_HELPERS_H
#define NATIVE_ADC_HELPERS_H

#include "hal/interface/nx_adc.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* ADC State Structure for Testing                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           ADC state structure for testing
 *
 * Contains runtime state information that can be queried by tests.
 */
typedef struct native_adc_state_s {
    bool initialized;          /**< Initialization flag */
    bool suspended;            /**< Suspend flag */
    bool clock_enabled;        /**< Clock enable flag */
    uint8_t channel_count;     /**< Number of configured channels */
    uint32_t resolution;       /**< ADC resolution in bits */
    uint32_t conversion_count; /**< Total number of conversions */
    uint32_t error_count;      /**< Error count */
} native_adc_state_t;

/*---------------------------------------------------------------------------*/
/* ADC Test Helpers                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ADC device state
 * \param[in]       instance: ADC instance ID
 * \param[out]      state: State structure to fill
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_adc_get_state(uint8_t instance, native_adc_state_t* state);

/**
 * \brief           Set analog input value for a channel
 * \param[in]       instance: ADC instance ID
 * \param[in]       channel: Channel index (0-based)
 * \param[in]       value: Analog value to set (raw ADC value)
 * \return          NX_OK on success, error code otherwise
 * \note            This simulates an analog voltage input to the ADC channel.
 *                  The value should be within the ADC resolution range.
 */
nx_status_t native_adc_set_analog_value(uint8_t instance, uint8_t channel,
                                        uint16_t value);

/**
 * \brief           Reset specific ADC instance to initial state
 * \param[in]       instance: ADC instance ID
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_adc_reset(uint8_t instance);

/**
 * \brief           Reset all ADC instances to initial state
 */
void native_adc_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_ADC_HELPERS_H */
