/**
 * \file            main.c
 * \brief           Independent public motor and axis computation consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/control/axis.h"
#include "nexus/control/motor.h"
#include "nexus_bindings.h"

#if __has_include("stm32f4xx_hal.h") || __has_include("stm32f4xx.h")
#error Vendor implementation headers reached the public control consumer
#endif

/** \brief Execute a synthetic current cycle without touching a power stage. */
static int motor_cycle(void) {
    const nx_motor_config_t config = {
        .pole_pairs = 7U,
        .clock_domain = 1U,
        .clock_epoch = 1U,
        .nominal_period_us = 50U,
        .maximum_gap_us = 60U,
        .maximum_angle_age_us = 100U,
        .maximum_prediction_us = 100U,
        .kp_d = 0.2F,
        .ki_d = 100.0F,
        .kp_q = 0.2F,
        .ki_q = 100.0F,
        .current_limit_a = 10.0F,
        .phase_current_limit_a = 20.0F,
        .current_sum_tolerance_a = 0.1F,
        .integral_limit_v = 24.0F,
        .minimum_bus_voltage_v = 12.0F,
        .maximum_bus_voltage_v = 48.0F,
        .maximum_modulation = 0.8F,
        .minimum_duty = 0.05F,
        .anti_windup_per_s = 100.0F,
    };
    const nx_motor_sample_t sample = {
        .bus_voltage_v = 24.0F,
        .sample_time_us = 1000U,
        .observed_at_us = 1000U,
        .angle_time_us = 1000U,
        .apply_time_us = 1050U,
        .sequence = 1U,
        .clock_domain = 1U,
        .clock_epoch = 1U,
        .valid = true,
    };
    const nx_motor_reference_t target = {.iq_a = 1.0F, .valid_until_us = 2000U};
    nx_motor_t motor = {0};
    nx_motor_output_t output;
    if (nx_motor_initialize(&motor, &config) != NX_SUCCESS ||
        nx_motor_step(&motor, &sample, &target, &output) != NX_SUCCESS ||
        output.sequence != 1U || output.planned_voltage.q <= 0.0F) {
        return 1;
    }
    for (size_t i = 0U; i < 3U; ++i) {
        if (!(output.duty[i] >= 0.05F && output.duty[i] <= 0.95F)) {
            return 2;
        }
    }
    return 0;
}

/** \brief Adopt one bounded velocity target through the real axis kernel. */
static int axis_cycle(void) {
    const nx_axis_config_t config = {.clock_domain = 1U,
                                     .epoch = 1U,
                                     .maximum_step_us = 1000U,
                                     .maximum_feedback_age_us = 2000U,
                                     .stop_policy =
                                         NX_AXIS_INHIBIT_AFTER_DWELL};
    const nx_axis_parameters_t parameters = {
        .revision = 1U,
        .velocity_kp = 0.5F,
        .velocity_ki = 1.0F,
        .tracking_rate_per_s = 100.0F,
        .integral_limit_a = 5.0F,
        .position_kp = 2.0F,
        .current_limit_a = 10.0F,
        .velocity_limit_rad_s = 10.0F,
        .acceleration_rad_s2 = 1.0F,
        .minimum_position_rad = -10.0F,
        .maximum_position_rad = 10.0F,
        .position_tolerance_rad = 0.01F,
        .velocity_tolerance_rad_s = 0.01F,
        .settling_us = 10000U,
    };
    nx_axis_feedback_t feedback = {.clock_domain = 1U,
                                   .epoch = 1U,
                                   .sequence = 1U,
                                   .sampled_at = 1000U,
                                   .valid = true};
    const nx_axis_command_t command = {.mode = NX_AXIS_VELOCITY,
                                       .clock_domain = 1U,
                                       .epoch = 1U,
                                       .sequence = 1U,
                                       .received_at = 1000U,
                                       .valid_until = 5000U,
                                       .velocity_rad_s = 1.0F};
    const nx_axis_limits_t limits = {.minimum_current_a = -10.0F,
                                     .maximum_current_a = 10.0F};
    nx_axis_t axis = {0};
    nx_axis_output_t output;
    if (nx_axis_initialize(&axis, &config, &parameters) != NX_SUCCESS ||
        nx_axis_enable(&axis, &feedback, 1000U) != NX_SUCCESS ||
        nx_axis_accept(&axis, &command, 1000U) != NX_SUCCESS) {
        return 3;
    }
    feedback.interval_start = 1000U;
    feedback.sampled_at = 2000U;
    feedback.sequence = 2U;
    if (nx_axis_step(&axis, &feedback, &limits, 2000U, 0.001F, &output) !=
            NX_SUCCESS ||
        output.command_sequence != 1U || output.adopted_at != 2000U ||
        output.current_a <= 0.0F) {
        return 4;
    }
    return 0;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) {
        return 10;
    }
    int result = motor_cycle();
    if (result == 0) {
        result = axis_cycle();
    }
    const nx_result_t stopped = nx_platform_stop();
    return result != 0 ? result : stopped == NX_SUCCESS ? 0 : 20;
}
