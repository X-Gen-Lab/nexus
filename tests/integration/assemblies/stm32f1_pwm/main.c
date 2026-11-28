/**
 * \file            main.c
 * \brief           Link the exact TIM3 PWM provider through public interfaces.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_pwm_port_t* pwm = nx_factory_pwm_ref(NX_PWM_REF_WAVEFORM);
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }

    nx_pwm_state_t state;
    const bool updated = nx_pwm_port_start(pwm) == NX_SUCCESS &&
                         nx_pwm_port_set(pwm, 1000U, 500U) == NX_SUCCESS &&
                         nx_pwm_port_state(pwm, &state) == NX_SUCCESS &&
                         state.tick_hz == 1000000U &&
                         state.period_ticks == 1000U &&
                         state.duty_ticks == 500U && state.running;
    const nx_result_t stopped = nx_pwm_port_stop(pwm);
    const nx_result_t closed = nx_platform_stop();
    return updated && stopped == NX_SUCCESS && closed == NX_SUCCESS ? 0 : 2;
}
