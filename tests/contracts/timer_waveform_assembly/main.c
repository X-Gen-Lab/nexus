/**
 * \file            main.c
 * \brief           ARM static timer face and lifecycle consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    if (nx_platform_start().primary != NX_SUCCESS) {
        return 1;
    }
#if defined(NEXUS_WAVEFORM_GROUP)
    const nx_pwm_group_t* face =
        nx_factory_pwm_group_ref(NX_PWM_GROUP_REF_DRIVE);
#elif defined(NEXUS_WAVEFORM_PULSE)
    const nx_timer_pulse_port_t* face =
        nx_factory_timer_pulse_ref(NX_TIMER_PULSE_REF_DRIVE);
#else
    const nx_timer_sequence_port_t* face =
        nx_factory_timer_sequence_ref(NX_TIMER_SEQUENCE_REF_DRIVE);
#endif
#if defined(NEXUS_WAVEFORM_GROUP)
    nx_pwm_group_state_t state;
    nx_result_t observed = nx_pwm_group_state(face, &state);
#elif defined(NEXUS_WAVEFORM_PULSE)
    nx_timer_pulse_state_t state;
    nx_result_t observed = nx_timer_pulse_port_state(face, &state);
#else
    nx_timer_sequence_state_t state;
    nx_result_t observed = nx_timer_sequence_port_state(face, &state);
#endif
    if (observed != NX_SUCCESS) {
        return 2;
    }
    return nx_platform_stop() == NX_SUCCESS ? 0 : 3;
}
