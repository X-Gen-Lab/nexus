/**
 * \file            drive_control_test.cpp
 * \brief           Real bounded reference, feedback and common-bus contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/control/drive.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <thread>
#include <vector>

namespace {
nx_motor_config_t profile() {
    nx_motor_config_t c = {};
    c.pole_pairs = 2;
    c.clock_domain = 7;
    c.clock_epoch = 9;
    c.nominal_period_us = 50;
    c.maximum_gap_us = 100;
    c.maximum_angle_age_us = 20;
    c.maximum_prediction_us = 100;
    c.kp_d = c.kp_q = 1;
    c.ki_d = c.ki_q = 100;
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
nx_motor_sample_t observation(uint64_t cycle = 10) {
    nx_motor_sample_t s = {};
    s.clock_domain = 7;
    s.clock_epoch = 9;
    s.sequence = cycle;
    s.sample_time_us = 1000 + (cycle - 10) * 50;
    s.angle_time_us = s.observed_at_us = s.sample_time_us;
    s.apply_time_us = s.sample_time_us + 50;
    s.bus_voltage_v = 24;
    s.valid = true;
    return s;
}
nx_drive_reference_t reference(uint32_t generation = 1) {
    nx_drive_reference_t r = {};
    r.clock_domain = 7;
    r.epoch = 9;
    r.generation = generation;
    r.parameters_revision = 3;
    r.command_sequence = 100 + generation;
    r.received_at = 900;
    r.current = {0, 2, 10000};
    return r;
}
class Drive : public testing::Test {
  protected:
    nx_drive_reference_t slots[2] = {};
    nx_drive_references_t references = {};
    nx_drive_t drive = {};
    void SetUp() override {
        ASSERT_EQ(nx_drive_references_initialize(&references, slots, 2, 7, 9),
                  NX_SUCCESS);
        const auto c = profile();
        ASSERT_EQ(nx_drive_initialize(&drive, &c, &references, 3), NX_SUCCESS);
    }
};

TEST_F(Drive, InitializationRejectsAliasesAndNonPowerOfTwoWithoutMutation) {
    auto before = references;
    EXPECT_EQ(nx_drive_references_initialize(&references, slots, 3, 7, 9),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&references, &before, sizeof(before)), 0);
    EXPECT_EQ(nx_drive_references_initialize(
                  &references,
                  reinterpret_cast<nx_drive_reference_t*>(&references), 1, 7,
                  9),
              NX_ERROR_INVALID);
}

TEST_F(Drive, ReferenceAdmissionIsDistinctFromFastCycleAdoption) {
    const auto r = reference();
    ASSERT_EQ(nx_drive_reference_publish(&references, &r), NX_SUCCESS);
    EXPECT_FALSE(drive.have_reference);
    nx_drive_output_t output = {};
    const auto s = observation();
    ASSERT_EQ(nx_drive_step(&drive, &s, &output), NX_SUCCESS);
    EXPECT_EQ(output.reference_generation, 1U);
    EXPECT_EQ(output.command_sequence, 101U);
    EXPECT_EQ(output.adopted_cycle, 10U);
    EXPECT_EQ(output.adopted_at, s.observed_at_us);
    EXPECT_FLOAT_EQ(output.accepted_iq_a, 2);
    EXPECT_NEAR(output.stationary_voltage.beta, 2.01F, 1e-6F);
    const auto next = observation(11);
    ASSERT_EQ(nx_drive_step(&drive, &next, &output), NX_SUCCESS);
    EXPECT_EQ(output.adopted_cycle, 10U);
    EXPECT_EQ(output.reference_generation, 1U);
}

TEST_F(Drive, MissingFirstReferenceDoesNotPoisonOrChangeOutput) {
    nx_drive_output_t output = {};
    output.reference_generation = 91;
    const auto s = observation();
    EXPECT_EQ(nx_drive_step(&drive, &s, &output), NX_ERROR_EMPTY);
    EXPECT_EQ(output.reference_generation, 91U);
    EXPECT_EQ(drive.fault, NX_SUCCESS);
    const auto r = reference();
    ASSERT_EQ(nx_drive_reference_publish(&references, &r), NX_SUCCESS);
    EXPECT_EQ(nx_drive_step(&drive, &s, &output), NX_SUCCESS);
}

TEST_F(Drive, FullReferenceQueueRejectsWithoutRefreshingReceipt) {
    auto first = reference(1), second = reference(2), third = reference(3);
    ASSERT_EQ(nx_drive_reference_publish(&references, &first), NX_SUCCESS);
    ASSERT_EQ(nx_drive_reference_publish(&references, &second), NX_SUCCESS);
    EXPECT_EQ(nx_drive_reference_publish(&references, &third), NX_ERROR_BUSY);
    nx_drive_output_t output = {};
    auto s = observation();
    ASSERT_EQ(nx_drive_step(&drive, &s, &output), NX_SUCCESS);
    EXPECT_EQ(output.reference_generation, 1U);
    EXPECT_EQ(drive.active.received_at, 900U);
    EXPECT_EQ(nx_drive_reference_publish(&references, &third), NX_SUCCESS);
    s = observation(11);
    ASSERT_EQ(nx_drive_step(&drive, &s, &output), NX_SUCCESS);
    EXPECT_EQ(output.reference_generation, 2U);
}

TEST_F(Drive, OldGenerationAndWrongEpochNeverEnterQueue) {
    auto r = reference();
    r.epoch = 8;
    EXPECT_EQ(nx_drive_reference_publish(&references, &r), NX_ERROR_INVALID);
    r = reference();
    ASSERT_EQ(nx_drive_reference_publish(&references, &r), NX_SUCCESS);
    EXPECT_EQ(nx_drive_reference_publish(&references, &r), NX_ERROR_STATE);
    r.generation = 0;
    EXPECT_EQ(nx_drive_reference_publish(&references, &r), NX_ERROR_INVALID);
}

TEST_F(Drive, ExpiryAtPlannedApplyClosesFastAdmission) {
    auto r = reference();
    r.current.valid_until_us = 1050;
    ASSERT_EQ(nx_drive_reference_publish(&references, &r), NX_SUCCESS);
    const auto s = observation();
    nx_drive_output_t output = {};
    output.reference_generation = 91;
    EXPECT_EQ(nx_drive_step(&drive, &s, &output), NX_ERROR_TIMEOUT);
    EXPECT_EQ(output.reference_generation, 91U);
    EXPECT_EQ(drive.motor.fault, NX_ERROR_TIMEOUT);
}

TEST_F(Drive, RevisionMismatchAndFutureReceiptDoNotAdopt) {
    auto r = reference();
    r.parameters_revision = 4;
    ASSERT_EQ(nx_drive_reference_publish(&references, &r), NX_SUCCESS);
    const auto s = observation();
    nx_drive_output_t output = {};
    EXPECT_EQ(nx_drive_step(&drive, &s, &output), NX_ERROR_STATE);
    EXPECT_FALSE(drive.have_reference);
}

TEST_F(Drive, FutureReceiptNeverBecomesCurrentAuthority) {
    auto r = reference();
    r.received_at = 1001;
    ASSERT_EQ(nx_drive_reference_publish(&references, &r), NX_SUCCESS);
    const auto s = observation();
    nx_drive_output_t output = {};
    EXPECT_EQ(nx_drive_step(&drive, &s, &output), NX_ERROR_STATE);
    EXPECT_FALSE(drive.have_reference);
}

TEST_F(Drive, RingIndexWrapKeepsBoundedFifoOrder) {
    references.produced = references.consumed = UINT32_MAX;
    const auto first = reference(1), second = reference(2);
    ASSERT_EQ(nx_drive_reference_publish(&references, &first), NX_SUCCESS);
    ASSERT_EQ(nx_drive_reference_publish(&references, &second), NX_SUCCESS);
    nx_drive_output_t output = {};
    auto s = observation();
    ASSERT_EQ(nx_drive_step(&drive, &s, &output), NX_SUCCESS);
    EXPECT_EQ(output.reference_generation, 1U);
    s = observation(11);
    ASSERT_EQ(nx_drive_step(&drive, &s, &output), NX_SUCCESS);
    EXPECT_EQ(output.reference_generation, 2U);
}

TEST_F(Drive, IndependentInhibitBypassesFullQueueAndRetainsFirstReason) {
    const auto first = reference(1), second = reference(2);
    ASSERT_EQ(nx_drive_reference_publish(&references, &first), NX_SUCCESS);
    ASSERT_EQ(nx_drive_reference_publish(&references, &second), NX_SUCCESS);
    ASSERT_EQ(nx_drive_inhibit(&references, NX_ERROR_IO), NX_SUCCESS);
    ASSERT_EQ(nx_drive_inhibit(&references, NX_ERROR_CANCELLED), NX_SUCCESS);
    const auto s = observation();
    nx_drive_output_t output = {};
    EXPECT_EQ(nx_drive_step(&drive, &s, &output), NX_ERROR_IO);
    EXPECT_EQ(drive.fault, NX_ERROR_IO);
    EXPECT_EQ(references.consumed, 0U);
}

TEST_F(Drive, CurrentControllerTrackingIsOnceAndInstanceLocal) {
    const auto r = reference();
    ASSERT_EQ(nx_drive_reference_publish(&references, &r), NX_SUCCESS);
    const auto s = observation();
    nx_drive_output_t output = {};
    ASSERT_EQ(nx_drive_step(&drive, &s, &output), NX_SUCCESS);
    const nx_motor_dq_t final = {0, 1};
    EXPECT_EQ(nx_drive_track_voltage(&drive, 10, final), NX_SUCCESS);
    EXPECT_EQ(nx_drive_track_voltage(&drive, 10, final), NX_ERROR_STATE);
    EXPECT_LT(drive.motor.integral_q_v, 0);
    nx_drive_t other = {};
    nx_drive_reference_t other_slots[1] = {};
    nx_drive_references_t other_references = {};
    ASSERT_EQ(
        nx_drive_references_initialize(&other_references, other_slots, 1, 7, 9),
        NX_SUCCESS);
    const auto c = profile();
    ASSERT_EQ(nx_drive_initialize(&other, &c, &other_references, 3),
              NX_SUCCESS);
    EXPECT_FLOAT_EQ(other.motor.integral_q_v, 0);
}

TEST_F(Drive, TaskAxisDecisionReachesFastCurrentControllerWithOriginalReceipt) {
    nx_axis_parameters_t parameters = {};
    parameters.revision = 20;
    parameters.velocity_kp = 1;
    parameters.position_kp = 1;
    parameters.current_limit_a = 5;
    parameters.velocity_limit_rad_s = 10;
    parameters.acceleration_rad_s2 = 100;
    parameters.minimum_position_rad = -100;
    parameters.maximum_position_rad = 100;
    const nx_axis_config_t config = {7, 9, 1000, 1000,
                                     NX_AXIS_INHIBIT_AFTER_DWELL};
    nx_axis_t axis = {};
    ASSERT_EQ(nx_axis_initialize(&axis, &config, &parameters), NX_SUCCESS);
    nx_axis_feedback_t feedback = {7, 9, 1, 950, 1000, 0, 0, true};
    ASSERT_EQ(nx_axis_enable(&axis, &feedback, 1000), NX_SUCCESS);
    nx_axis_command_t command = {};
    command.mode = NX_AXIS_CURRENT;
    command.clock_domain = 7;
    command.epoch = 9;
    command.sequence = 1;
    command.received_at = 1000;
    command.valid_until = 5000;
    command.current_a = 2;
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    feedback.sequence = 2;
    feedback.interval_start = 1000;
    feedback.sampled_at = 1050;
    const nx_axis_limits_t limits = {-5, 5};
    nx_axis_output_t decision = {};
    ASSERT_EQ(
        nx_axis_step(&axis, &feedback, &limits, 1050, 0.00005F, &decision),
        NX_SUCCESS);
    auto current = reference();
    current.current.iq_a = decision.current_a;
    current.current.valid_until_us = command.valid_until;
    current.received_at = command.received_at;
    current.command_sequence = decision.command_sequence;
    ASSERT_EQ(nx_drive_reference_publish(&references, &current), NX_SUCCESS);
    EXPECT_FALSE(drive.motor.have_sample);
    const auto s = observation(11);
    nx_drive_output_t output = {};
    ASSERT_EQ(nx_drive_step(&drive, &s, &output), NX_SUCCESS);
    EXPECT_FLOAT_EQ(output.accepted_iq_a, 2);
    EXPECT_EQ(output.command_sequence, 1U);
    EXPECT_EQ(drive.active.received_at, command.received_at);
    EXPECT_EQ(drive.parameters_revision, 3U);
    EXPECT_EQ(axis.parameters.revision, 20U);
}

TEST(DriveFeedback, DecimationRetainsRealIntervalsAndConsecutiveFastCoverage) {
    nx_drive_feedback_sampler_t sampler = {};
    ASSERT_EQ(nx_drive_feedback_sampler_initialize(&sampler, 7, 9, 2, 100),
              NX_SUCCESS);
    nx_drive_feedback_t out = {};
    out.mechanical.sequence = 91;
    nx_drive_output_t adopted = {};
    adopted.reference_generation = 3;
    adopted.command_sequence = 42;
    adopted.accepted_iq_a = 1.5F;
    auto s = observation();
    adopted.voltage.sequence = s.sequence;
    EXPECT_EQ(nx_drive_feedback_sample(&sampler, &s, 0, &adopted, &out),
              NX_ERROR_BUSY);
    EXPECT_EQ(out.mechanical.sequence, 91U);
    s = observation(11);
    adopted.voltage.sequence = s.sequence;
    EXPECT_EQ(nx_drive_feedback_sample(&sampler, &s, 0.001F, &adopted, &out),
              NX_ERROR_BUSY);
    s = observation(12);
    adopted.voltage.sequence = s.sequence;
    ASSERT_EQ(nx_drive_feedback_sample(&sampler, &s, 0.002F, &adopted, &out),
              NX_SUCCESS);
    EXPECT_EQ(out.mechanical.sequence, 1U);
    EXPECT_EQ(out.fast_first_cycle, 10U);
    EXPECT_EQ(out.fast_last_cycle, 12U);
    EXPECT_EQ(out.mechanical.interval_start, 1000U);
    EXPECT_EQ(out.mechanical.sampled_at, 1100U);
    EXPECT_NEAR(out.mechanical.velocity_rad_s, 20, 1e-5F);
    EXPECT_EQ(out.reference_generation, 3U);
    EXPECT_EQ(out.command_sequence, 42U);
    EXPECT_FLOAT_EQ(out.accepted_iq_a, 1.5F);
}

TEST(DriveFeedback, MissingFastCycleLatchesFaultInsteadOfRenumbering) {
    nx_drive_feedback_sampler_t sampler = {};
    ASSERT_EQ(nx_drive_feedback_sampler_initialize(&sampler, 7, 9, 2, 100),
              NX_SUCCESS);
    nx_drive_feedback_t out = {};
    nx_drive_output_t adopted = {};
    adopted.reference_generation = 1;
    auto s = observation();
    adopted.voltage.sequence = s.sequence;
    ASSERT_EQ(nx_drive_feedback_sample(&sampler, &s, 0, &adopted, &out),
              NX_ERROR_BUSY);
    s = observation(12);
    adopted.voltage.sequence = s.sequence;
    EXPECT_EQ(nx_drive_feedback_sample(&sampler, &s, 0, &adopted, &out),
              NX_ERROR_STATE);
    EXPECT_EQ(sampler.sequence, 0U);
}

TEST(DriveFeedback, FutureAdoptionCannotQualifyAControlObservation) {
    nx_drive_feedback_sampler_t sampler = {};
    ASSERT_EQ(nx_drive_feedback_sampler_initialize(&sampler, 7, 9, 1, 100),
              NX_SUCCESS);
    const auto s = observation();
    nx_drive_output_t adopted = {};
    adopted.reference_generation = 1;
    adopted.voltage.sequence = s.sequence;
    adopted.adopted_cycle = s.sequence + 1;
    nx_drive_feedback_t out = {};
    EXPECT_EQ(nx_drive_feedback_sample(&sampler, &s, 0, &adopted, &out),
              NX_ERROR_INVALID);
    EXPECT_FALSE(sampler.anchored);
}

nx_drive_feedback_t feedback(uint32_t sequence) {
    nx_drive_feedback_t f = {};
    f.mechanical = {7,
                    9,
                    sequence,
                    sequence * 100U,
                    sequence * 100U + 100U,
                    static_cast<float>(sequence),
                    10000,
                    true};
    f.fast_first_cycle = sequence * 2U;
    f.fast_last_cycle = sequence * 2U + 2U;
    f.reference_generation = sequence;
    f.accepted_iq_a = static_cast<float>(sequence);
    return f;
}

TEST(DriveFeedback, OverflowCannotBeRepairedByReadingOldControlRecords) {
    nx_drive_feedback_t slots[1] = {};
    nx_drive_feedback_queue_t queue = {};
    ASSERT_EQ(nx_drive_feedback_initialize(&queue, slots, 1, 7, 9), NX_SUCCESS);
    const auto first = feedback(1), second = feedback(2);
    ASSERT_EQ(nx_drive_feedback_publish(&queue, &first), NX_SUCCESS);
    EXPECT_EQ(nx_drive_feedback_publish(&queue, &second), NX_ERROR_OVERFLOW);
    nx_drive_feedback_t out = {};
    EXPECT_EQ(nx_drive_feedback_take(&queue, &out), NX_ERROR_OVERFLOW);
    EXPECT_EQ(out.mechanical.sequence, 0U);
}

TEST(DriveFeedback, AdjacentIntervalsCannotHideFastCoverageLoss) {
    nx_drive_feedback_t slots[2] = {};
    nx_drive_feedback_queue_t queue = {};
    ASSERT_EQ(nx_drive_feedback_initialize(&queue, slots, 2, 7, 9), NX_SUCCESS);
    const auto first = feedback(1);
    ASSERT_EQ(nx_drive_feedback_publish(&queue, &first), NX_SUCCESS);
    auto second = feedback(2);
    second.fast_first_cycle += 1;
    EXPECT_EQ(nx_drive_feedback_publish(&queue, &second), NX_ERROR_STATE);
}

TEST(DriveFeedback, ConsecutiveControlRecordsRoundTripWithoutPayloadTearing) {
    constexpr uint32_t count = 20000;
    std::vector<nx_drive_feedback_t> slots(32768);
    nx_drive_feedback_queue_t queue = {};
    ASSERT_EQ(nx_drive_feedback_initialize(&queue, slots.data(), 32768, 7, 9),
              NX_SUCCESS);
    std::atomic<bool> failed{false};
    std::thread producer([&] {
        for (uint32_t i = 1; i <= count; ++i) {
            const auto f = feedback(i);
            if (nx_drive_feedback_publish(&queue, &f) != NX_SUCCESS) {
                failed.store(true);
                return;
            }
        }
    });
    for (uint32_t i = 1; i <= count && !failed.load();) {
        nx_drive_feedback_t out = {};
        const auto result = nx_drive_feedback_take(&queue, &out);
        if (result == NX_ERROR_EMPTY) {
            std::this_thread::yield();
            continue;
        }
        const float expected_iq = static_cast<float>(i);
        /* This handoff preserves float bits; ULP tolerance could hide tearing.
         */
        if (result != NX_SUCCESS || out.mechanical.sequence != i ||
            out.reference_generation != i ||
            std::memcmp(&out.accepted_iq_a, &expected_iq,
                        sizeof(expected_iq)) != 0 ||
            out.fast_last_cycle != i * 2U + 2U) {
            failed.store(true);
            break;
        }
        ++i;
    }
    producer.join();
    EXPECT_FALSE(failed.load());
}

TEST(DriveBus, MotoringAndRegenerationHaveSeparateConservativeBudgets) {
    const nx_drive_bus_budget_t budget = {100, 1000, 100, 30};
    const float requested[] = {80, 120, -60};
    float scales[3] = {};
    ASSERT_EQ(nx_drive_bus_limit(&budget, requested, 3, 200, scales),
              NX_SUCCESS);
    EXPECT_FLOAT_EQ(scales[0], 0.5F);
    EXPECT_FLOAT_EQ(scales[1], 0.5F);
    EXPECT_FLOAT_EQ(scales[2], 0.5F);
}

TEST(DriveBus, StaleOrNonfiniteInputsLeaveAllScalesUnchanged) {
    nx_drive_bus_budget_t budget = {100, 1000, 100, 0};
    float requested[] = {10, -1};
    float scales[] = {7, 8};
    EXPECT_EQ(nx_drive_bus_limit(&budget, requested, 2, 1000, scales),
              NX_ERROR_TIMEOUT);
    EXPECT_FLOAT_EQ(scales[0], 7);
    requested[1] = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(nx_drive_bus_limit(&budget, requested, 2, 200, scales),
              NX_ERROR_INVALID);
    EXPECT_FLOAT_EQ(scales[1], 8);
}
} /* namespace */
