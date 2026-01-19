/**
 * \file            nx_adc_test.h
 * \brief           ADC test support functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_ADC_TEST_H
#define NX_ADC_TEST_H

#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Test Support Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Set simulated ADC value (for testing)
 * \param[in]       index: ADC instance index
 * \param[in]       channel: ADC channel
 * \param[in]       value: Simulated value
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_adc_native_set_value(uint8_t index, uint8_t channel,
                                    uint16_t value);

/**
 * \brief           Get ADC state (for testing)
 * \param[in]       index: ADC instance index
 * \param[out]      initialized: Initialization flag
 * \param[out]      suspended: Suspend flag
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_adc_native_get_state(uint8_t index, bool* initialized,
                                    bool* suspended);

/**
 * \brief           Reset ADC instance (for testing)
 * \param[in]       index: ADC instance index
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_adc_native_reset(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* NX_ADC_TEST_H */
