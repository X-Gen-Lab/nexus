/**
 * \file            nx_timer_helpers.h
 * \brief           Timer helper functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_TIMER_HELPERS_H
#define NX_TIMER_HELPERS_H

#include "nx_timer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get timer implementation from base interface
 * \param[in]       base: Base timer interface pointer
 * \return          Timer implementation pointer or NULL
 */
static inline nx_timer_impl_t* timer_get_impl(nx_timer_base_t* base) {
    return base ? NX_CONTAINER_OF(base, nx_timer_impl_t, base) : NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* NX_TIMER_HELPERS_H */
