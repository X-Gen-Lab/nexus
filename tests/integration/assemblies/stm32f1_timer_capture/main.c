/**
 * \file            main.c
 * \brief           Exercise the F103RB TIM4 typed capture contract.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static nx_timer_capture_event_t events[4];

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_timer_capture_t* input =
        nx_factory_timer_capture_ref(NX_TIMER_CAPTURE_REF_INPUT);
    if (nx_timer_capture_start(input, events, 4U) != NX_SUCCESS) {
        return 2;
    }
    nx_timer_capture_state_t state;
    const bool units = nx_timer_capture_state(input, &state) == NX_SUCCESS &&
                       state.tick_hz == 1000000U && state.counter_bits == 16U;
    nx_result_t stopped = nx_timer_capture_stop(input);
    if (stopped == NX_ERROR_BUSY) {
        nx_timer_capture_event_t event;
        for (size_t i = 0U; i < 4U; ++i) {
            if (nx_timer_capture_read(input, &event) != NX_SUCCESS) {
                break;
            }
        }
        stopped = nx_timer_capture_stop(input);
    }
    return units && stopped == NX_SUCCESS && nx_platform_stop() == NX_SUCCESS
               ? 0
               : 3;
}
