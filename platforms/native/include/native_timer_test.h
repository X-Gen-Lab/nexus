/**
 * \file            native_timer_test.h
 * \brief           Native Timer Test Helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_TIMER_TEST_H
#define NATIVE_TIMER_TEST_H

#include "hal/interface/nx_timer.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Factory Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Timer base instance (factory function)
 * \param[in]       timer_index: Timer index (0-13)
 * \return          Timer base interface pointer, NULL on failure
 */
nx_timer_base_t* nx_timer_base_native_get(uint8_t timer_index);

/**
 * \brief           Get Timer PWM instance (factory function)
 * \param[in]       timer_index: Timer index (0-13)
 * \return          Timer PWM interface pointer, NULL on failure
 */
nx_timer_pwm_t* nx_timer_pwm_native_get(uint8_t timer_index);

/**
 * \brief           Get Timer encoder instance (factory function)
 * \param[in]       timer_index: Timer index (0-13)
 * \return          Timer encoder interface pointer, NULL on failure
 */
nx_timer_encoder_t* nx_timer_encoder_native_get(uint8_t timer_index);

/*---------------------------------------------------------------------------*/
/* Timer-Specific Test Helpers                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Simulate encoder count change (for testing)
 * \param[in]       timer_index: Timer index
 * \param[in]       delta: Count change (positive or negative)
 */
void nx_timer_encoder_native_simulate_count(uint8_t timer_index, int64_t delta);

/*---------------------------------------------------------------------------*/
/* PWM Test Helpers                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Check if PWM channel is initialized
 * \param[in]       instance: Timer instance
 * \param[in]       channel: PWM channel
 * \return          true if initialized, false otherwise
 */
bool native_pwm_is_initialized(int instance, int channel);

/**
 * \brief           Check if PWM channel is running
 * \param[in]       instance: Timer instance
 * \param[in]       channel: PWM channel
 * \return          true if running, false otherwise
 */
bool native_pwm_is_running(int instance, int channel);

/**
 * \brief           Get PWM frequency (for testing)
 * \param[in]       instance: Timer instance
 * \param[in]       channel: PWM channel
 * \return          PWM frequency in Hz or 0 if not initialized
 */
uint32_t native_pwm_get_frequency(int instance, int channel);

/**
 * \brief           Get PWM duty cycle (for testing)
 * \param[in]       instance: Timer instance
 * \param[in]       channel: PWM channel
 * \return          Duty cycle (0-10000) or 0 if not initialized
 */
uint16_t native_pwm_get_duty_cycle(int instance, int channel);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_TIMER_TEST_H */
