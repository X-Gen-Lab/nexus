/**
 * \file            main.c
 * \brief           Disconnected exact F103RB TIM1/ADC1 software window.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static volatile bool run_disconnected_fixture;
static nx_pwm_adc_request_t request;
static uint16_t sample;
static const uint32_t duty = 256U;

static bool one_window(const nx_pwm_adc_t* port) {
    const nx_pwm_group_frame_t frame = {&duty, 1U, 512U};
    nx_time_us_t deadline;
    nx_request_initialize(&request.base);
    if (nx_deadline_finite_after(nx_time_now_us(), 1000U, &deadline) !=
            NX_SUCCESS ||
        nx_pwm_adc_prepare(&request, &sample, 1U, &frame, deadline) !=
            NX_SUCCESS ||
        nx_pwm_adc_submit(port, &request) != NX_SUCCESS)
        return false;
    nx_result_t serviced = NX_ERROR_BUSY;
    for (unsigned poll = 0U; poll < 4096U && serviced == NX_ERROR_BUSY;
         ++poll) {
        serviced = nx_pwm_adc_service(port);
    }
    nx_result_t result;
    size_t count;
    return serviced == NX_SUCCESS &&
           nx_request_result(&request.base, &result, &count) == NX_SUCCESS &&
           result == NX_SUCCESS && count == 1U && sample <= 0x0fffU;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_pwm_adc_t* port = nx_factory_pwm_adc_ref(NX_PWM_ADC_REF_WINDOW);
    nx_pwm_adc_state_t state;
    const bool valid = nx_pwm_adc_state(port, &state) == NX_SUCCESS &&
                       state.format.group == NX_ADC_INJECTED &&
                       state.max_samples == 1U &&
                       (!run_disconnected_fixture || one_window(port));
    const nx_result_t stopped = nx_platform_stop();
    return valid && stopped == NX_SUCCESS ? 0 : 2;
}
