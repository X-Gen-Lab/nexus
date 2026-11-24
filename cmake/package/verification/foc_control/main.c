/**
 * \file            main.c
 * \brief           Independent public FOC composition contract consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/control/current.h"
#include "nexus/control/drive.h"
#include "nexus/control/modulation.h"
#include "nexus_bindings.h"

#if __has_include("stm32f4xx_hal.h") || __has_include("stm32f4xx.h")
#error Vendor implementation headers reached the public FOC consumer
#endif

/** \brief Use public control and measurement APIs on one synthetic SI cycle. */
static int control_cycle(void) {
    const nx_axis_config_t axis_config = {
        .clock_domain = 1U,
        .epoch = 1U,
        .maximum_step_us = 1000U,
        .maximum_feedback_age_us = 2000U,
        .stop_policy = NX_AXIS_INHIBIT_AFTER_DWELL,
    };
    const nx_axis_parameters_t parameters = {
        .revision = 1U,
        .velocity_kp = 0.5F,
        .velocity_ki = 1.0F,
        .tracking_rate_per_s = 100.0F,
        .integral_limit_a = 5.0F,
        .position_kp = 2.0F,
        .current_limit_a = 10.0F,
        .velocity_limit_rad_s = 10.0F,
        .acceleration_rad_s2 = 10.0F,
        .minimum_position_rad = -10.0F,
        .maximum_position_rad = 10.0F,
        .position_tolerance_rad = 0.01F,
        .velocity_tolerance_rad_s = 0.01F,
        .settling_us = 10000U,
    };
    nx_axis_feedback_t feedback = {
        .clock_domain = 1U,
        .epoch = 1U,
        .sequence = 1U,
        .sampled_at = 1000U,
        .valid = true,
    };
    const nx_axis_command_t command = {
        .mode = NX_AXIS_VELOCITY,
        .clock_domain = 1U,
        .epoch = 1U,
        .sequence = 1U,
        .received_at = 1000U,
        .valid_until = 5000U,
        .velocity_rad_s = 1.0F,
    };
    const nx_axis_limits_t limits = {-10.0F, 10.0F};
    nx_axis_t axis = {0};
    nx_axis_output_t outer;
    if (nx_axis_initialize(&axis, &axis_config, &parameters) != NX_SUCCESS ||
        nx_axis_enable(&axis, &feedback, 1000U) != NX_SUCCESS ||
        nx_axis_accept(&axis, &command, 1000U) != NX_SUCCESS) {
        return 1;
    }
    feedback.interval_start = 1000U;
    feedback.sampled_at = 2000U;
    feedback.sequence = 2U;
    if (nx_axis_step(&axis, &feedback, &limits, 2000U, 0.001F, &outer) !=
            NX_SUCCESS ||
        outer.command_sequence != 1U || outer.current_a <= 0.0F) {
        return 2;
    }

    const nx_motor_config_t motor = {
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
    nx_drive_references_t references = {0};
    nx_drive_reference_t slots[2];
    nx_drive_t drive = {0};
    const nx_drive_reference_t reference = {
        .current = {0.0F, outer.current_a, 5000U},
        .clock_domain = 1U,
        .epoch = 1U,
        .generation = 1U,
        .parameters_revision = 1U,
        .command_sequence = outer.command_sequence,
        .received_at = command.received_at,
    };
    if (nx_drive_references_initialize(&references, slots, 2U, 1U, 1U) !=
            NX_SUCCESS ||
        nx_drive_initialize(&drive, &motor, &references, 1U) != NX_SUCCESS ||
        nx_drive_reference_publish(&references, &reference) != NX_SUCCESS) {
        return 3;
    }

    const nx_current_calibration_t calibration = {
        .identity = 1U,
        .revision = 1U,
        .zero_code = {2048.0F, 2048.0F, 2048.0F},
        .a_per_code = {0.01F, 0.01F, 0.01F},
        .direction = {1, 1, 1},
        .maximum_code = 4095U,
        .maximum_skew_ticks = 2U,
        .maximum_current_a = 10.0F,
        .maximum_sum_a = 0.1F,
    };
    const nx_current_capture_t capture = {
        .calibration_identity = 1U,
        .calibration_revision = 1U,
        .code = {2048U, 2048U, 2048U},
        .aperture_tick = {800U, 800U, 800U},
        .measured_mask = NX_CURRENT_ALL_PHASES,
        .valid = true,
    };
    nx_phase_current_t currents;
    if (nx_current_convert(&calibration, &capture, &currents) != NX_SUCCESS) {
        return 4;
    }
    nx_motor_sample_t sample = {
        .bus_voltage_v = 24.0F,
        .sample_time_us = 2000U,
        .angle_time_us = 2000U,
        .observed_at_us = 2000U,
        .apply_time_us = 2050U,
        .sequence = 1U,
        .clock_domain = 1U,
        .clock_epoch = 1U,
        .valid = true,
    };
    for (size_t i = 0U; i < 3U; ++i) {
        sample.phase_current_a[i] = currents.phase_a[i];
    }
    nx_drive_output_t output;
    if (nx_drive_step(&drive, &sample, &output) != NX_SUCCESS ||
        output.reference_generation != 1U ||
        output.command_sequence != outer.command_sequence) {
        return 5;
    }
    const nx_modulation_config_t carrier = {
        .topology = NX_MODULATION_THREE_SHUNT,
        .alignment = NX_MODULATION_EDGE_ALIGNED,
        .period_ticks = 1000U,
        .minimum_duty_ticks = 50U,
        .maximum_duty_ticks = 700U,
        .deadtime_ticks = 1U,
        .settling_ticks = 20U,
        .aperture_ticks = 4U,
        .conversion_ticks = 10U,
        .irq_ticks = 5U,
        .compute_ticks = 20U,
        .commit_before_ticks = 990U,
        .sensor_mask = NX_CURRENT_ALL_PHASES,
        .samples_per_trigger = 3U,
    };
    nx_modulation_output_t planned;
    nx_motor_dq_t quantized;
    if (nx_modulation_phase_plan(&carrier, output.stationary_voltage, 24.0F,
                                 &planned) != NX_SUCCESS ||
        nx_motor_park(planned.voltage,
                      output.voltage.output_electrical_angle_rad,
                      &quantized) != NX_SUCCESS ||
        nx_drive_track_voltage(&drive, sample.sequence, quantized) !=
            NX_SUCCESS ||
        planned.sample_count == 0U) {
        return 6;
    }
    return nx_drive_inhibit(&references, NX_ERROR_TIMEOUT) == NX_SUCCESS ? 0
                                                                         : 7;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) {
        return 10;
    }
    const int result = control_cycle();
    const nx_result_t stopped = nx_platform_stop();
    return result != 0 ? result : stopped == NX_SUCCESS ? 0 : 20;
}
