/**
 * \file            native_timer_helpers.h
 * \brief           Native Timer test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_TIMER_HELPERS_H
#define NATIVE_TIMER_HELPERS_H

#include "hal/interface/nx_timer.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Timer State Structure for Testing                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Timer state structure for testing
 *
 * Contains runtime state information that can be queried by tests.
 */
typedef struct native_timer_state_s {
    bool initialized;        /**< Initialization flag */
    bool suspended;          /**< Suspend flag */
    bool running;            /**< Timer running flag */
    uint32_t frequency;      /**< Timer frequency in Hz */
    uint16_t prescaler;      /**< Prescaler value */
    uint32_t period;         /**< Period value */
    uint32_t counter;        /**< Current counter value */
    uint8_t channel_count;   /**< Number of PWM channels */
    uint32_t overflow_count; /**< Number of overflows */
} native_timer_state_t;

/*---------------------------------------------------------------------------*/
/* Timer Test Helpers                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Timer device state
 * \param[in]       instance: Timer instance ID
 * \param[out]      state: State structure to fill
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_timer_get_state(uint8_t instance,
                                   native_timer_state_t* state);

/**
 * \brief           Advance timer time (simulate time passage)
 * \param[in]       instance: Timer instance ID
 * \param[in]       ticks: Number of ticks to advance
 * \return          NX_OK on success, error code otherwise
 * \note            This simulates the passage of time by advancing the
 *                  timer counter. If the counter reaches the period value,
 *                  it will overflow and trigger the callback if configured.
 */
nx_status_t native_timer_advance_time(uint8_t instance, uint32_t ticks);

/**
 * \brief           Reset specific Timer instance to initial state
 * \param[in]       instance: Timer instance ID
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_timer_reset(uint8_t instance);

/**
 * \brief           Reset all Timer instances to initial state
 */
void native_timer_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_TIMER_HELPERS_H */
