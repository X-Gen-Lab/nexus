/**
 * \file            controller_main.c
 * \brief           Link-only timer caller with permanently retained storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

static nx_timer_capture_event_t s_ring[4];
static nx_timer_capture_event_t s_event;
static nx_timer_capture_state_t s_capture;
static nx_advanced_pwm_state_t s_power;

int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary == NX_SUCCESS) {
        if (NX_TIMER_CAPTURE_ID_COUNT != 0U) {
            const nx_timer_capture_t* capture = nx_factory_timer_capture(0U);
            (void)nx_timer_capture_start(capture, s_ring, 4U);
            (void)nx_timer_capture_state(capture, &s_capture);
            (void)nx_timer_capture_read(capture, &s_event);
            (void)nx_timer_capture_stop(capture);
        }
        if (NX_ADVANCED_PWM_ID_COUNT != 0U) {
            const nx_advanced_pwm_t* power = nx_factory_advanced_pwm(0U);
            (void)nx_advanced_pwm_set_duty(power, 250U);
            (void)nx_advanced_pwm_start(power);
            (void)nx_advanced_pwm_state(power, &s_power);
            (void)nx_advanced_pwm_stop(power);
            (void)nx_advanced_pwm_clear_fault(power);
        }
    }
    (void)nx_platform_stop();
    /* Link evidence never claims waveforms or an inspected output stage. The
     * exact ring and snapshots remain static if actual cleanup is refused. */
    for (;;) {
    }
}
