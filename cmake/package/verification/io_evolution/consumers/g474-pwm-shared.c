/**
 * \file            g474-pwm-shared.c
 * \brief           Public installed-SDK two-channel fixed PWM linking
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

static nx_pwm_state_t s_left;
static nx_pwm_state_t s_right;

/** \brief Retain the generated owner if complete withdrawal fails. */
static void keep_storage(void) {
    for (;;) {
    }
}

/** \brief Close both lanes and their outer pin and timer leases. */
static int close_platform(int result) {
    if (nx_platform_stop() != NX_SUCCESS) {
        keep_storage();
    }
    return result;
}

/** \brief Linking does not execute PWM or establish electrical timing. */
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return close_platform(1);
    }
    const nx_pwm_port_t* left = nx_factory_pwm(NX_PWM_ID_LEFT);
    const nx_pwm_port_t* right = nx_factory_pwm(NX_PWM_ID_RIGHT);
    if (left == NULL || right == NULL || left == right ||
        nx_pwm_port_state(left, &s_left) != NX_SUCCESS ||
        nx_pwm_port_state(right, &s_right) != NX_SUCCESS) {
        return close_platform(2);
    }
    if (nx_pwm_port_set(left, s_left.period_ticks, s_left.duty_ticks) !=
            NX_SUCCESS ||
        nx_pwm_port_set(right, s_right.period_ticks, s_right.duty_ticks) !=
            NX_SUCCESS ||
        nx_pwm_port_start(left) != NX_SUCCESS ||
        nx_pwm_port_start(right) != NX_SUCCESS ||
        nx_pwm_port_state(left, &s_left) != NX_SUCCESS ||
        nx_pwm_port_state(right, &s_right) != NX_SUCCESS || !s_left.running ||
        !s_right.running) {
        return close_platform(3);
    }
    if (nx_pwm_port_stop(left) != NX_SUCCESS ||
        nx_pwm_port_state(right, &s_right) != NX_SUCCESS || !s_right.running) {
        return close_platform(4);
    }
    if (nx_pwm_port_stop(right) != NX_SUCCESS ||
        nx_pwm_port_state(left, &s_left) != NX_SUCCESS ||
        nx_pwm_port_state(right, &s_right) != NX_SUCCESS || s_left.running ||
        s_right.running) {
        return close_platform(5);
    }
    return close_platform(0);
}
