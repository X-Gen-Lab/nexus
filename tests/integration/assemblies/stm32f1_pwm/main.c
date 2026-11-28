/**
 * \file            main.c
 * \brief           Link exact fixed PWM providers through public interfaces.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Verify one immutable timebase and its staged duty. */
static bool exercise(const nx_pwm_port_t* port, uint32_t period) {
    nx_pwm_state_t state;
    return nx_pwm_port_start(port) == NX_SUCCESS &&
           nx_pwm_port_set(port, period, period / 2U) == NX_SUCCESS &&
           nx_pwm_port_state(port, &state) == NX_SUCCESS &&
           state.tick_hz == 1000000U && state.period_ticks == period &&
           state.duty_ticks == period / 2U && state.running;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_pwm_port_t* const primary =
        nx_factory_pwm_ref(NX_PWM_REF_WAVEFORM);
    bool valid = exercise(primary, 1000U);
#if defined(NX_PWM_REF_WAVEFORM16)
    const nx_pwm_port_t* const second =
        nx_factory_pwm_ref(NX_PWM_REF_WAVEFORM16);
    const nx_pwm_port_t* const third =
        nx_factory_pwm_ref(NX_PWM_REF_WAVEFORM17);
    valid = exercise(second, 800U) && valid;
    valid = exercise(third, 600U) && valid;
    valid = nx_pwm_port_stop(third) == NX_SUCCESS && valid;
    valid = nx_pwm_port_stop(second) == NX_SUCCESS && valid;
#endif
    const nx_result_t stopped = nx_pwm_port_stop(primary);
    const nx_result_t closed = nx_platform_stop();
    return valid && stopped == NX_SUCCESS && closed == NX_SUCCESS ? 0 : 2;
}
