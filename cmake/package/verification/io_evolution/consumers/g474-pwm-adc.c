/**
 * \file            g474-pwm-adc.c
 * \brief           Public installed-SDK finite PWM/ADC link verification
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

static nx_pwm_adc_request_t s_request;
static uint16_t s_samples[4];
static const uint32_t s_duties[3] = {0U, 0U, 0U};
static nx_pwm_group_frame_t s_frame = {s_duties, 3U, 0U};
static nx_pwm_adc_state_t s_state;

/** \brief Keep all disjoint window storage alive during failed withdrawal. */
static void keep_storage(void) {
    for (;;) {
    }
}

/** \brief Final joined close owns pad, clock and private-state release. */
static int close_platform(int result) {
    if (nx_platform_stop() != NX_SUCCESS) {
        keep_storage();
    }
    return result;
}

/** \brief Link real finite acquisition; no analog or HIL claim is implied. */
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return close_platform(1);
    }
    const nx_pwm_adc_t* window = nx_factory_pwm_adc(NX_PWM_ADC_ID_WINDOW);
    if (window == NULL || nx_pwm_adc_state(window, &s_state) != NX_SUCCESS ||
        s_state.pwm_channels != sizeof(s_duties) / sizeof(s_duties[0])) {
        return close_platform(2);
    }
    s_frame.trigger_ticks = s_state.trigger_ticks;
    nx_request_initialize(&s_request.base);
    if (nx_pwm_adc_prepare(&s_request, s_samples, 4U, &s_frame,
                           nx_deadline_after(nx_time_now_us(), 10000U)) !=
            NX_SUCCESS ||
        nx_pwm_adc_submit(window, &s_request) != NX_SUCCESS) {
        return close_platform(3);
    }
    if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_pwm_adc_cancel(window, &s_request);
    }
    for (unsigned i = 0U;
         i != 64U && nx_request_state(&s_request.base) != NX_REQUEST_SETTLED;
         ++i) {
        (void)nx_pwm_adc_service(window);
    }
    (void)nx_pwm_adc_state(window, &s_state);
    (void)nx_pwm_adc_stop(window);
    if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_platform_stop();
        if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
            keep_storage();
        }
    }
    return close_platform(0);
}
