/**
 * \file            nx_adc_helpers.h
 * \brief           ADC helper functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_ADC_HELPERS_H
#define NX_ADC_HELPERS_H

#include "nx_adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ADC implementation from base interface
 * \param[in]       self: ADC interface pointer
 * \return          ADC implementation pointer
 */
static inline nx_adc_impl_t* adc_get_impl(nx_adc_t* self) {
    return self ? (nx_adc_impl_t*)self : NULL;
}

/**
 * \brief           Get ADC buffer implementation from base interface
 * \param[in]       self: ADC buffer interface pointer
 * \return          ADC buffer implementation pointer
 */
static inline nx_adc_buffer_impl_t* adc_buffer_get_impl(nx_adc_buffer_t* self) {
    return self ? (nx_adc_buffer_impl_t*)self : NULL;
}

/**
 * \brief           Get channel implementation from channel interface
 * \param[in]       self: Channel interface pointer
 * \return          Channel implementation pointer
 */
static inline nx_adc_channel_impl_t*
adc_get_channel_impl(nx_adc_channel_t* self) {
    return self ? (nx_adc_channel_impl_t*)self : NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* NX_ADC_HELPERS_H */
