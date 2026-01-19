/**
 * \file            native_adc_test.h
 * \brief           Native ADC Test Helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_ADC_TEST_H
#define NATIVE_ADC_TEST_H

#include "hal/interface/nx_adc.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Factory Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ADC instance (factory function)
 * \param[in]       adc_index: ADC index (0-2)
 * \return          ADC interface pointer, NULL on failure
 */
nx_adc_t* nx_adc_native_get(uint8_t adc_index);

/**
 * \brief           Get ADC buffer instance (factory function)
 * \param[in]       adc_index: ADC index (0-2)
 * \param[in]       buffer_size: Buffer size in samples
 * \return          ADC buffer interface pointer, NULL on failure
 */
nx_adc_buffer_t* nx_adc_buffer_native_get(uint8_t adc_index,
                                          size_t buffer_size);

/*---------------------------------------------------------------------------*/
/* ADC-Specific Test Helpers                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Set simulated ADC value for testing
 * \param[in]       adc_index: ADC index (0-2)
 * \param[in]       channel: ADC channel (0-15)
 * \param[in]       value: Simulated value
 * \note            This function is for testing purposes only
 */
void nx_adc_native_set_simulated_value(uint8_t adc_index, uint8_t channel,
                                       uint16_t value);

/**
 * \brief           Cleanup ADC buffer instance (for testing)
 * \param[in]       adc_index: ADC index (0-2)
 */
void nx_adc_buffer_native_cleanup(uint8_t adc_index);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_ADC_TEST_H */
