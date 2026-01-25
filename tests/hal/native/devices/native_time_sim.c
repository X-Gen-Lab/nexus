/**
 * \file            native_time_sim.c
 * \brief           Native platform time simulation for testing
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include <stdint.h>

/*---------------------------------------------------------------------------*/
/* Time Simulation State                                                     */
/*---------------------------------------------------------------------------*/

static uint64_t g_simulated_time_ms = 0;

/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get current simulated time in milliseconds
 * \return          Current simulated time
 */
uint64_t nx_get_time_ms(void) {
    return g_simulated_time_ms;
}

/**
 * \brief           Advance simulated time
 * \param[in]       ms: Milliseconds to advance
 */
void nx_advance_time_ms(uint32_t ms) {
    g_simulated_time_ms += ms;
}

/**
 * \brief           Reset simulated time to zero
 */
void nx_reset_time(void) {
    g_simulated_time_ms = 0;
}
