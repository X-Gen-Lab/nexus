/**
 * \file            nx_dac_helpers.h
 * \brief           DAC helper functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_DAC_HELPERS_H
#define NX_DAC_HELPERS_H

#include "nx_dac_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get DAC implementation from base interface
 * \param[in]       self: DAC interface pointer
 * \return          DAC implementation pointer
 */
static inline nx_dac_impl_t* dac_get_impl(nx_dac_t* self) {
    return self ? (nx_dac_impl_t*)self : NULL;
}

/**
 * \brief           Get channel implementation from channel interface
 * \param[in]       self: Channel interface pointer
 * \return          Channel implementation pointer
 */
static inline nx_dac_channel_impl_t*
dac_get_channel_impl(nx_dac_channel_t* self) {
    return self ? (nx_dac_channel_impl_t*)self : NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* NX_DAC_HELPERS_H */
