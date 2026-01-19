/**
 * \file            nx_dac_test.h
 * \brief           DAC test support functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_DAC_TEST_H
#define NX_DAC_TEST_H

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
 * \brief           Get DAC output value (for testing)
 * \param[in]       index: DAC instance index
 * \param[in]       channel: DAC channel
 * \param[out]      value: Output value
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_dac_native_get_value(uint8_t index, uint8_t channel,
                                    uint16_t* value);

/**
 * \brief           Get DAC state (for testing)
 * \param[in]       index: DAC instance index
 * \param[out]      initialized: Initialization flag
 * \param[out]      suspended: Suspend flag
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_dac_native_get_state(uint8_t index, bool* initialized,
                                    bool* suspended);

/**
 * \brief           Reset DAC instance (for testing)
 * \param[in]       index: DAC instance index
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_dac_native_reset(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* NX_DAC_TEST_H */
