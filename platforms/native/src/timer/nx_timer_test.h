/**
 * \file            nx_timer_test.h
 * \brief           Timer test support functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_TIMER_TEST_H
#define NX_TIMER_TEST_H

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
 * \brief           Get Timer state (for testing)
 * \param[in]       index: Timer instance index
 * \param[out]      initialized: Initialization flag
 * \param[out]      suspended: Suspend flag
 * \param[out]      running: Running flag
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_timer_native_get_state(uint8_t index, bool* initialized,
                                      bool* suspended, bool* running);

/**
 * \brief           Get Timer counter value (for testing)
 * \param[in]       index: Timer instance index
 * \param[out]      counter: Counter value
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_timer_native_get_counter(uint8_t index, uint32_t* counter);

/**
 * \brief           Reset Timer instance (for testing)
 * \param[in]       index: Timer instance index
 * \return          NX_OK on success, error code otherwise
 * \note            This function is for testing purposes only
 */
nx_status_t nx_timer_native_reset(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* NX_TIMER_TEST_H */
