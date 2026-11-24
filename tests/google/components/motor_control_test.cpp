/**
 * \file            motor_control_test.cpp
 * \brief           Real C sensored control, timing and calibration contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/control/motor.h"
#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>

namespace {
constexpr float pi = 3.14159265358979323846F;

nx_motor_config_t config() {
    nx_motor_config_t c = {};
    c.pole_pairs = 2;
    c.clock_domain = 1;
    c.clock_epoch = 4;
    c.nominal_period_us = 50;
    c.maximum_gap_us = 100;
    c.maximum_angle_age_us = 20;
    c.maximum_prediction_us = 100;
    c.kp_d = 1;
    c.ki_d = 100;
    c.kp_q = 1;
    c.ki_q = 100;
    c.current_limit_a = 10;
    c.phase_current_limit_a = 20;
    c.current_sum_tolerance_a = 0.1F;
    c.integral_limit_v = 24;
    c.minimum_bus_voltage_v = 6;
    c.maximum_bus_voltage_v = 60;
    c.maximum_modulation = 0.9F;
    c.minimum_duty = 0.05F;
    c.anti_windup_per_s = 1000;
    return c;
}

nx_motor_sample_t sample() {
    nx_motor_sample_t s = {};
    s.bus_voltage_v = 24;
    s.sample_time_us = 1000;
    s.angle_time_us = 1000;
    s.observed_at_us = 1000;
    s.apply_time_us = 1050;
    s.clock_domain = 1;
    s.clock_epoch = 4;
    s.valid = true;
    return s;
}

nx_motor_reference_t reference() {
    nx_motor_reference_t r = {};
    r.iq_a = 2;
    r.valid_until_us = 100000;
    return r;
}

TEST(MotorMath, AmplitudeInvariantClarkeAndParkHaveExplicitSigns) {
    const float currents[] = {0, -1.7320508075688772F, 1.7320508075688772F};
    nx_motor_ab_t ab = {};
    ASSERT_EQ(nx_motor_clarke(currents, &ab), NX_SUCCESS);
    EXPECT_FLOAT_EQ(ab.alpha, 0);
    EXPECT_NEAR(ab.beta, -2, 1e-6F);
    nx_motor_dq_t dq = {};
    ASSERT_EQ(nx_motor_park(ab, pi / 2, &dq), NX_SUCCESS);
    EXPECT_NEAR(dq.d, -2, 1e-6F);
    EXPECT_NEAR(dq.q, 0, 1e-6F);
    nx_motor_ab_t roundtrip = {};
    ASSERT_EQ(nx_motor_inverse_park(dq, pi / 2, &roundtrip), NX_SUCCESS);
    EXPECT_NEAR(roundtrip.alpha, ab.alpha, 1e-6F);
    EXPECT_NEAR(roundtrip.beta, ab.beta, 1e-6F);
}

TEST(MotorMath, NonfiniteTransformNeverChangesOutput) {
    nx_motor_dq_t out = {7, 8};
    const nx_motor_ab_t ab = {std::numeric_limits<float>::infinity(), 2};
    EXPECT_EQ(nx_motor_park(ab, 0, &out), NX_ERROR_INVALID);
    EXPECT_FLOAT_EQ(out.d, 7);
    EXPECT_FLOAT_EQ(out.q, 8);
}

TEST(Motor, RejectedConfigurationDoesNotModifyState) {
    nx_motor_t motor = {};
    const nx_motor_t before = motor;
    auto c = config();
    c.minimum_duty = 0.5F;
    EXPECT_EQ(nx_motor_initialize(&motor, &c), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&motor, &before, sizeof(motor)), 0);
    c = config();
    c.maximum_gap_us = c.nominal_period_us - 1;
    EXPECT_EQ(nx_motor_initialize(&motor, &c), NX_ERROR_INVALID);
}

TEST(Motor, RealFocProducesCompletePlannedDutiesWithoutHardware) {
    nx_motor_t motor = {};
    const auto c = config();
    ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
    auto s = sample();
    auto r = reference();
    nx_motor_output_t out = {};
    ASSERT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out.measured_current.d, 0);
    EXPECT_FLOAT_EQ(out.measured_current.q, 0);
    EXPECT_NEAR(out.requested_voltage.q, 2.01F, 1e-6F);
    EXPECT_GT(out.duty[1], out.duty[0]);
    EXPECT_GT(out.duty[0], out.duty[2]);
    EXPECT_FALSE(out.limited);
    for (const float duty : out.duty) {
        EXPECT_GE(duty, c.minimum_duty);
        EXPECT_LE(duty, 1 - c.minimum_duty);
    }
}

TEST(Motor, SamplingAndPlannedOutputUseDifferentPredictedAngles) {
    nx_motor_t motor = {};
    auto c = config();
    c.pole_pairs = 1;
    ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
    auto s = sample();
    s.mechanical_angle_rad = 0.5F;
    s.mechanical_velocity_rad_s = 2000;
    auto r = reference();
    nx_motor_output_t out = {};
    ASSERT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
    EXPECT_NEAR(out.sample_electrical_angle_rad, 0.5F, 1e-6F);
    EXPECT_NEAR(out.output_electrical_angle_rad, 0.6F, 1e-6F);
}

TEST(Motor, AngleAcquiredAfterSamplingCanProjectBackFromKnownObservation) {
    nx_motor_t motor = {};
    auto c = config();
    c.pole_pairs = 1;
    ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
    auto s = sample();
    s.angle_time_us = 1020;
    s.observed_at_us = 1020;
    s.mechanical_angle_rad = 0.54F;
    s.mechanical_velocity_rad_s = 2000;
    const auto r = reference();
    nx_motor_output_t out = {};
    ASSERT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
    EXPECT_NEAR(out.sample_electrical_angle_rad, 0.5F, 1e-6F);
    EXPECT_NEAR(out.output_electrical_angle_rad, 0.6F, 1e-6F);
}

TEST(Motor, ObservationCannotPrecedeSamplingOrFollowThePlannedOutput) {
    for (int which = 0; which < 2; ++which) {
        nx_motor_t motor = {};
        const auto c = config();
        ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
        auto s = sample();
        s.observed_at_us =
            which == 0 ? s.sample_time_us - 1 : s.apply_time_us + 1;
        const auto r = reference();
        nx_motor_output_t out = {};
        EXPECT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_ERROR_STATE);
        EXPECT_FALSE(motor.have_sample);
    }
}

TEST(Motor, IncreasingSampleTimestampCannotHideRegressingObservationClock) {
    nx_motor_t motor = {};
    const auto c = config();
    ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
    auto s = sample();
    s.apply_time_us = 1100;
    s.observed_at_us = 1060;
    const auto r = reference();
    nx_motor_output_t out = {};
    ASSERT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
    s.sequence = 1;
    s.sample_time_us = 1050;
    s.angle_time_us = 1050;
    s.observed_at_us = 1050;
    EXPECT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_ERROR_STATE);
    EXPECT_EQ(motor.last_sequence, 0);
}

TEST(Motor, VectorLimitKeepsDirectionAndFeedsBackToIntegrators) {
    nx_motor_t motor = {};
    auto c = config();
    c.kp_d = 10;
    c.kp_q = 10;
    ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
    const auto s = sample();
    auto r = reference();
    r.id_a = 3;
    r.iq_a = 4;
    nx_motor_output_t out = {};
    ASSERT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
    ASSERT_TRUE(out.limited);
    EXPECT_NEAR(out.planned_voltage.d / out.planned_voltage.q, 0.75F, 1e-6F);
    const float bound = s.bus_voltage_v * c.maximum_modulation *
                        (1 - 2 * c.minimum_duty) / std::sqrt(3.0F);
    EXPECT_NEAR(std::hypot(out.planned_voltage.d, out.planned_voltage.q), bound,
                1e-5F);
    EXPECT_LT(motor.integral_d_v, 0);
    EXPECT_LT(motor.integral_q_v, 0);
}

TEST(Motor, ActualQuantizedVoltageTrackingRequiresCurrentCycleAndOnlyOnce) {
    nx_motor_t motor = {};
    const auto c = config();
    ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
    auto s = sample();
    const auto r = reference();
    nx_motor_output_t out = {};
    ASSERT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
    const auto before = motor;
    EXPECT_EQ(nx_motor_track_voltage(&motor, 9, out.planned_voltage),
              NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&motor, &before, sizeof(motor)), 0);
    nx_motor_dq_t quantized = out.planned_voltage;
    quantized.q -= 0.2F;
    ASSERT_EQ(nx_motor_track_voltage(&motor, s.sequence, quantized),
              NX_SUCCESS);
    EXPECT_NEAR(motor.integral_q_v, before.integral_q_v - 0.01F, 1e-6F);
    EXPECT_EQ(nx_motor_track_voltage(&motor, s.sequence, quantized),
              NX_ERROR_STATE);
}

TEST(Motor, RepeatedSampleLatchesFaultWithoutChangingOutputOrIntegral) {
    nx_motor_t motor = {};
    const auto c = config();
    ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
    const auto s = sample();
    const auto r = reference();
    nx_motor_output_t out = {};
    ASSERT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
    const auto before = out;
    const float integral = motor.integral_q_v;
    EXPECT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_ERROR_STATE);
    EXPECT_EQ(motor.fault, NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&out, &before, sizeof(out)), 0);
    EXPECT_FLOAT_EQ(motor.integral_q_v, integral);
}

TEST(Motor, DomainEpochAndAngleFreshnessAreCheckedBeforeComputing) {
    for (int which = 0; which < 4; ++which) {
        nx_motor_t motor = {};
        const auto c = config();
        ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
        auto s = sample();
        if (which == 0) {
            ++s.clock_epoch;
        } else if (which == 1) {
            ++s.clock_domain;
        } else if (which == 2) {
            s.angle_time_us -= 21;
        } else {
            s.angle_time_us = s.sample_time_us + 1;
        }
        const auto r = reference();
        nx_motor_output_t out = {};
        EXPECT_NE(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
        EXPECT_NE(motor.fault, NX_SUCCESS);
        EXPECT_FALSE(motor.have_sample);
    }
}

TEST(Motor, ExpiryAtPlannedApplyTimeClosesAdmission) {
    nx_motor_t motor = {};
    const auto c = config();
    ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
    const auto s = sample();
    auto r = reference();
    r.valid_until_us = s.apply_time_us;
    nx_motor_output_t out = {};
    EXPECT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_ERROR_TIMEOUT);
    EXPECT_EQ(motor.fault, NX_ERROR_TIMEOUT);
}

TEST(Motor, BadMeasuredCurrentAndBusVoltageNeverPublishOutput) {
    for (int which = 0; which < 5; ++which) {
        nx_motor_t motor = {};
        const auto c = config();
        ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
        auto s = sample();
        if (which == 0) {
            s.phase_current_a[0] = 21;
            s.phase_current_a[1] = -21;
        } else if (which == 1) {
            s.phase_current_a[0] = 1;
        } else if (which == 2) {
            s.bus_voltage_v = 0;
        } else if (which == 3) {
            s.bus_voltage_v = 61;
        } else {
            s.phase_current_a[0] = std::numeric_limits<float>::quiet_NaN();
        }
        const auto r = reference();
        nx_motor_output_t out = {};
        EXPECT_NE(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
        EXPECT_NE(motor.fault, NX_SUCCESS);
        EXPECT_FALSE(motor.have_sample);
    }
}

TEST(Motor, KnownStorageAliasesRejectBeforeMutation) {
    nx_motor_t motor = {};
    const auto c = config();
    ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
    const auto before = motor;
    const auto s = sample();
    const auto r = reference();
    auto* out = reinterpret_cast<nx_motor_output_t*>(&motor);
    EXPECT_EQ(nx_motor_step(&motor, &s, &r, out), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&motor, &before, sizeof(motor)), 0);
}

TEST(Motor, OutputCommitFailureCanLatchWithoutAutomaticRestart) {
    nx_motor_t motor = {};
    const auto c = config();
    ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
    auto s = sample();
    const auto r = reference();
    nx_motor_output_t out = {};
    ASSERT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
    ASSERT_EQ(nx_motor_latch_fault(&motor, NX_ERROR_IO), NX_SUCCESS);
    s.sample_time_us += 50;
    s.angle_time_us += 50;
    s.observed_at_us += 50;
    s.apply_time_us += 50;
    ++s.sequence;
    EXPECT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_ERROR_STATE);
    EXPECT_EQ(motor.fault, NX_ERROR_IO);
    ASSERT_EQ(nx_motor_clear_fault(&motor), NX_SUCCESS);
    EXPECT_FALSE(motor.have_sample);
    EXPECT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
}

TEST(Motor, IndependentInstancesRetainIndependentIntegratorHistory) {
    nx_motor_t first = {};
    nx_motor_t second = {};
    const auto c = config();
    ASSERT_EQ(nx_motor_initialize(&first, &c), NX_SUCCESS);
    ASSERT_EQ(nx_motor_initialize(&second, &c), NX_SUCCESS);
    auto s = sample();
    auto r = reference();
    nx_motor_output_t out = {};
    ASSERT_EQ(nx_motor_step(&first, &s, &r, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(second.integral_q_v, 0);
    r.iq_a = -2;
    ASSERT_EQ(nx_motor_step(&second, &s, &r, &out), NX_SUCCESS);
    EXPECT_NEAR(first.integral_q_v, -second.integral_q_v, 1e-7F);
}

TEST(MotorCalibration, BoundedOffsetCandidateIsIndependentOfHardwareAndState) {
    nx_motor_offset_t calibration = {};
    ASSERT_EQ(nx_motor_offset_begin(&calibration, 3, 4), NX_SUCCESS);
    const float raw[3][3] = {{100, 200, 300}, {102, 202, 302}, {104, 204, 304}};
    float offsets[] = {7, 8, 9};
    EXPECT_EQ(nx_motor_offset_finish(&calibration, offsets), NX_ERROR_BUSY);
    for (const auto& scan : raw) {
        ASSERT_EQ(nx_motor_offset_add(&calibration, scan), NX_SUCCESS);
    }
    ASSERT_EQ(nx_motor_offset_finish(&calibration, offsets), NX_SUCCESS);
    EXPECT_FLOAT_EQ(offsets[0], 102);
    EXPECT_FLOAT_EQ(offsets[1], 202);
    EXPECT_FLOAT_EQ(offsets[2], 302);
    EXPECT_EQ(nx_motor_offset_add(&calibration, raw[0]), NX_ERROR_STATE);
}

TEST(MotorCalibration, RejectedNoisySampleCannotBeSilentlySkipped) {
    nx_motor_offset_t calibration = {};
    ASSERT_EQ(nx_motor_offset_begin(&calibration, 3, 1), NX_SUCCESS);
    const float first[] = {100, 200, 300};
    const float noisy[] = {110, 200, 300};
    ASSERT_EQ(nx_motor_offset_add(&calibration, first), NX_SUCCESS);
    EXPECT_EQ(nx_motor_offset_add(&calibration, noisy), NX_ERROR_STATE);
    EXPECT_EQ(nx_motor_offset_add(&calibration, first), NX_ERROR_STATE);
    float offsets[] = {7, 8, 9};
    EXPECT_EQ(nx_motor_offset_finish(&calibration, offsets), NX_ERROR_STATE);
    EXPECT_FLOAT_EQ(offsets[0], 7);
}

TEST(Motor, FullWidthEpochAndSequenceExhaustionPreserveTheirIdentity) {
    nx_motor_t motor = {};
    auto c = config();
    c.clock_epoch = (UINT64_C(1) << 40) + 4;
    ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
    auto s = sample();
    s.clock_epoch = c.clock_epoch;
    s.sequence = UINT64_MAX;
    const auto r = reference();
    nx_motor_output_t out = {};
    ASSERT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
    s.sample_time_us += 50;
    s.angle_time_us += 50;
    s.observed_at_us += 50;
    s.apply_time_us += 50;
    s.sequence = 0;
    EXPECT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_ERROR_STATE);
    EXPECT_EQ(motor.last_sequence, UINT64_MAX);
    EXPECT_EQ(motor.config.clock_epoch, c.clock_epoch);
}

TEST(Motor, ExcessGapAndOutputPredictionNeverAdvanceControlHistory) {
    for (int which = 0; which < 2; ++which) {
        nx_motor_t motor = {};
        const auto c = config();
        ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
        auto s = sample();
        const auto r = reference();
        nx_motor_output_t out = {};
        ASSERT_EQ(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
        const auto before = out;
        s.sample_time_us += 101;
        s.angle_time_us = s.sample_time_us;
        s.observed_at_us = s.sample_time_us;
        s.apply_time_us = s.sample_time_us + (which == 0 ? 50U : 101U);
        ++s.sequence;
        EXPECT_NE(nx_motor_step(&motor, &s, &r, &out), NX_SUCCESS);
        EXPECT_EQ(std::memcmp(&out, &before, sizeof(out)), 0);
        EXPECT_EQ(motor.last_sequence, 0);
    }
}

/* Independent SI surface-PMSM plant. Only quantized observations reach the
 * controller; double truth and the delayed actuator remain in this fixture.
 * It is a selected averaged electrical model, not a switching or HIL model. */
struct SurfacePmsm {
    double alpha_a = 0;
    double beta_a = 0;
    double mechanical_rad = 0;
    double mechanical_rad_s = 0;
    float duty[3] = {0.5F, 0.5F, 0.5F};
    double previous_encoder_rad = 0;

    nx_motor_sample_t observe(uint64_t sequence, float bus) {
        auto result = sample();
        result.sequence = sequence;
        result.sample_time_us += sequence * 50;
        result.angle_time_us = result.sample_time_us;
        result.observed_at_us = result.sample_time_us;
        result.apply_time_us = result.sample_time_us + 50;
        result.bus_voltage_v = bus;
        const double phase[3] = {
            alpha_a, -0.5 * alpha_a + std::sqrt(3.0) * 0.5 * beta_a,
            -0.5 * alpha_a - std::sqrt(3.0) * 0.5 * beta_a};
        for (size_t i = 0; i < 3; ++i) {
            result.phase_current_a[i] =
                static_cast<float>(std::round(phase[i] * 1000) / 1000);
        }
        const double encoder = std::round(mechanical_rad * 1e6) / 1e6;
        result.mechanical_angle_rad = static_cast<float>(encoder);
        result.mechanical_velocity_rad_s =
            static_cast<float>((encoder - previous_encoder_rad) / 0.00005);
        previous_encoder_rad = encoder;
        return result;
    }

    void advance(float bus) {
        constexpr double resistance = 2;
        constexpr double inductance = 0.002;
        constexpr double flux = 0.02;
        constexpr double pairs = 2;
        constexpr double inertia = 0.1;
        constexpr double step = 0.000005;
        const double mean =
            (static_cast<double>(duty[0]) + duty[1] + duty[2]) / 3;
        const double va = bus * (duty[0] - mean);
        const double vb = bus * (duty[1] - mean);
        const double vbeta = (va + 2 * vb) / std::sqrt(3.0);
        for (int i = 0; i < 10; ++i) {
            const double angle = pairs * mechanical_rad;
            const double electrical_speed = pairs * mechanical_rad_s;
            const double iq =
                std::cos(angle) * beta_a - std::sin(angle) * alpha_a;
            const double da = (va - resistance * alpha_a +
                               flux * electrical_speed * std::sin(angle)) /
                              inductance;
            const double db = (vbeta - resistance * beta_a -
                               flux * electrical_speed * std::cos(angle)) /
                              inductance;
            const double acceleration =
                (1.5 * pairs * flux * iq - 0.01 * mechanical_rad_s) / inertia;
            alpha_a += step * da;
            beta_a += step * db;
            mechanical_rad_s += step * acceleration;
            mechanical_rad += step * mechanical_rad_s;
        }
    }
};

TEST(MotorPlant, QuantizedDelayedPmsmTracksCurrentAndReportsBusLimitation) {
    nx_motor_t motor = {};
    auto c = config();
    c.kp_d = 2;
    c.kp_q = 2;
    c.ki_d = 1000;
    c.ki_q = 1000;
    ASSERT_EQ(nx_motor_initialize(&motor, &c), NX_SUCCESS);
    SurfacePmsm plant;
    auto r = reference();
    r.iq_a = 4;
    r.valid_until_us = 1000000;
    float first_max_error = 0;
    float low_bus_mean = 0;
    bool limited = false;
    nx_motor_output_t out = {};
    for (uint64_t cycle = 0; cycle < 2000; ++cycle) {
        const float bus = cycle < 1000 ? 24.0F : 12.0F;
        const auto observation = plant.observe(cycle, bus);
        ASSERT_EQ(nx_motor_step(&motor, &observation, &r, &out), NX_SUCCESS);
        if (cycle >= 800 && cycle < 1000) {
            first_max_error =
                fmaxf(first_max_error, fabsf(out.measured_current.q - r.iq_a));
        }
        if (cycle >= 1800) {
            low_bus_mean += out.measured_current.q / 200;
            limited = limited || out.limited;
        }
        /* One complete old duty frame acts before the new plan is adopted.
         * The controller receives neither plant truth nor future measurements.
         */
        plant.advance(bus);
        for (size_t i = 0; i < 3; ++i) {
            plant.duty[i] = out.duty[i];
        }
    }
    EXPECT_LT(first_max_error, 0.03F);
    EXPECT_TRUE(limited);
    EXPECT_GT(low_bus_mean, 2.5F);
    EXPECT_LT(low_bus_mean, 3.0F);
    EXPECT_GT(plant.mechanical_rad_s, 0);
    auto bad = plant.observe(2000, 12);
    bad.valid = false;
    const auto last = out;
    EXPECT_EQ(nx_motor_step(&motor, &bad, &r, &out), NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&out, &last, sizeof(out)), 0);
    EXPECT_EQ(motor.fault, NX_ERROR_STATE);
}
} /* namespace */
