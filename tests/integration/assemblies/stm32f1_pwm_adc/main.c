/**
 * \file            main.c
 * \brief           Bounded disconnected F103RB TIM1/ADC1 sampled feedback
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
#if defined(NX_PWM_ADC_REF_THREE)
enum { LEGS = 3U };
#define CYCLE_REF NX_PWM_ADC_REF_THREE
#else
enum { LEGS = 1U };
#define CYCLE_REF NX_PWM_ADC_REF_WINDOW
#endif
static uint32_t duties[LEGS];

enum { FULL_SCALE = 4095U, WINDOW_COUNT = 4U };

static bool one_window(const nx_pwm_adc_t* port, uint32_t trigger) {
    const nx_pwm_group_frame_t frame = {duties, LEGS, trigger};
    nx_time_us_t deadline;
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
           result == NX_SUCCESS && count == 1U && sample <= FULL_SCALE;
}

/** \brief Run a finite raw-sample to duty feedback sequence on the fixture. */
static bool sampled_feedback(const nx_pwm_adc_t* port,
                             const nx_pwm_adc_state_t* state) {
    const uint32_t ceiling = state->trigger_ticks - state->deadtime_ticks - 1U;
    nx_request_initialize(&request.base);
    for (unsigned leg = 0U; leg < LEGS; ++leg)
        duties[leg] = ceiling / (leg + 2U);
    for (unsigned window = 0U; window < WINDOW_COUNT; ++window) {
        if (!one_window(port, state->trigger_ticks))
            return false;
        duties[0] = ceiling * (FULL_SCALE - sample) / FULL_SCALE;
    }
    return true;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_pwm_adc_t* port = nx_factory_pwm_adc_ref(CYCLE_REF);
    nx_pwm_adc_state_t state;
    const bool valid =
        nx_pwm_adc_state(port, &state) == NX_SUCCESS &&
        state.format.group == NX_ADC_INJECTED &&
        state.format.significant_bits == 12U && state.pwm_channels == LEGS &&
        state.max_samples == 1U && state.trigger_ticks > state.deadtime_ticks &&
        (!run_disconnected_fixture || sampled_feedback(port, &state));
    const nx_result_t stopped = nx_platform_stop();
    return valid && stopped == NX_SUCCESS ? 0 : 2;
}
