/**
 * \file            main.c
 * \brief           Exercise the typed F103 TIM1/TIM8 complementary PWM
 * contract. \author          Nexus Team \version         1.0.0 \date 2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static volatile bool run_logic_probe;

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_advanced_pwm_t* pair =
        nx_factory_advanced_pwm_ref(NX_ADVANCED_PWM_REF_PAIR);
    nx_advanced_pwm_state_t state;
    bool valid = nx_advanced_pwm_state(pair, &state) == NX_SUCCESS &&
                 !state.running && state.tick_hz == 8000000U &&
                 state.deadtime_ticks == 8U && state.deadtime_hz == 8000000U;
    if (valid && run_logic_probe) {
        valid = nx_advanced_pwm_clear_fault(pair) == NX_SUCCESS &&
                nx_advanced_pwm_set_duty(pair, state.period_ticks / 2U) ==
                    NX_SUCCESS &&
                nx_advanced_pwm_start(pair) == NX_SUCCESS;
    }
    const nx_result_t stopped = nx_advanced_pwm_stop(pair);
    const nx_result_t closed = nx_platform_stop();
    return valid && stopped == NX_SUCCESS && closed == NX_SUCCESS ? 0 : 2;
}
