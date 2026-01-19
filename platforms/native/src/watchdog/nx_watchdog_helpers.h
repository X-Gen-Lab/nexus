/**
 * \file            nx_watchdog_helpers.h
 * \brief           Watchdog helper functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_WATCHDOG_HELPERS_H
#define NX_WATCHDOG_HELPERS_H

#include "hal/interface/nx_watchdog.h"
#include "hal/nx_status.h"
#include "nx_watchdog_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Watchdog implementation from base interface
 * \param[in]       self: Watchdog interface pointer
 * \return          Implementation pointer or NULL
 * \note            Optimized as inline for performance
 */
static inline nx_watchdog_impl_t* watchdog_get_impl(nx_watchdog_t* self) {
    return self ? (nx_watchdog_impl_t*)self : NULL;
}

/**
 * \brief           Get current system time in milliseconds
 * \return          Current time in milliseconds
 */
uint64_t watchdog_get_system_time_ms(void);

/**
 * \brief           Check if watchdog has timed out
 * \param[in]       state: Watchdog state pointer
 * \return          true if timed out, false otherwise
 */
bool watchdog_check_timeout(nx_watchdog_state_t* state);

/**
 * \brief           Reset watchdog state for testing
 * \param[in]       state: Watchdog state pointer
 * \note            This function is for testing purposes only
 */
void watchdog_reset_state(nx_watchdog_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* NX_WATCHDOG_HELPERS_H */
