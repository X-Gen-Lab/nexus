/**
 * \file            main.c
 * \brief           Run one finite pulse through the typed F103 platform.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/timer_pulse.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static const uint32_t width = 250U;
static nx_timer_pulse_request_t request;

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_timer_pulse_t* pulse =
        nx_factory_timer_pulse_ref(NX_TIMER_PULSE_REF_WAVEFORM);
    nx_time_us_t deadline;
    nx_request_initialize(&request.base);
    request.width_ticks = &width;
    request.channel_count = 1U;
    if (nx_deadline_finite_after(nx_time_now_us(), 10000U, &deadline) !=
            NX_SUCCESS ||
        nx_request_prepare(&request.base, deadline) != NX_SUCCESS ||
        nx_timer_pulse_port_submit(pulse, &request) != NX_SUCCESS) {
        (void)nx_platform_stop();
        return 2;
    }
    nx_result_t service;
    do {
        service = nx_timer_pulse_port_service(pulse);
    } while (service == NX_ERROR_BUSY);
    size_t periods = 0U;
    nx_result_t result = NX_ERROR_STATE;
    const nx_result_t copied =
        nx_request_result(&request.base, &result, &periods);
    const nx_result_t stopped = nx_platform_stop();
    return copied == NX_SUCCESS && result == NX_SUCCESS && periods == 1U &&
                   stopped == NX_SUCCESS
               ? 0
               : 3;
}
