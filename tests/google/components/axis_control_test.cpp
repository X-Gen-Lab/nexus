/**
 * \file            axis_control_test.cpp
 * \brief           Actual rotary-control, command and settling contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/control/axis.h"
#include <cstring>
#include <gtest/gtest.h>
#include <limits>

namespace {
class AxisControl : public testing::Test {
  protected:
    nx_axis_t axis{};
    nx_axis_config_t config{1, 7, 5000, 2000, NX_AXIS_INHIBIT_AFTER_DWELL};
    nx_axis_parameters_t parameters{1,    2,    4,   100,    5,     3,    3, 10,
                                    1000, -100, 100, 0.001F, 0.02F, 2000, 0};
    nx_axis_feedback_t feedback{1, 7, 1, 1000, 1000, 0, 0, true};
    nx_axis_limits_t limits{-3, 3};
    nx_axis_output_t out{};

    void SetUp() override {
        ASSERT_EQ(nx_axis_initialize(&axis, &config, &parameters), NX_SUCCESS);
        ASSERT_EQ(nx_axis_enable(&axis, &feedback, 1000), NX_SUCCESS);
    }

    nx_axis_command_t velocity(float value, uint32_t sequence = 1,
                               nx_time_us_t until = 100000) {
        return {NX_AXIS_VELOCITY,
                1,
                7,
                sequence,
                axis.last_step,
                until,
                0,
                value,
                0};
    }

    nx_result_t tick(nx_time_us_t now, float speed = 0, float position = 0) {
        const float dt = static_cast<float>(now - axis.last_step) * 1e-6F;
        feedback.interval_start = feedback.sampled_at;
        feedback.sampled_at = now;
        feedback.sequence++;
        feedback.velocity_rad_s = speed;
        feedback.position_rad = position;
        return nx_axis_step(&axis, &feedback, &limits, now, dt, &out);
    }
};

TEST_F(AxisControl, AdmissionAndSoftwareAdoptionRemainSeparate) {
    auto command = velocity(2);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    EXPECT_EQ(axis.state, NX_AXIS_READY);
    EXPECT_EQ(axis.output.command_sequence, 0U);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    EXPECT_EQ(out.command_sequence, 1U);
    EXPECT_EQ(out.adopted_at, 2000U);
    EXPECT_EQ(out.state, NX_AXIS_RUNNING);
    EXPECT_FLOAT_EQ(out.requested_velocity_rad_s, 2);
    EXPECT_FLOAT_EQ(out.reference_velocity_rad_s, 1);
    EXPECT_NEAR(out.current_a, 2.004F, 1e-6F);
}

TEST_F(AxisControl, RepeatsAndOldEpochNeverRenewTheOriginalReceipt) {
    auto command = velocity(1, 1, 4000);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    command.received_at = 2000;
    command.valid_until = 9000;
    const auto before = axis;
    EXPECT_EQ(nx_axis_accept(&axis, &command, 2000), NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&before, &axis, sizeof(axis)), 0);
    command.sequence = 2;
    command.epoch = 6;
    EXPECT_EQ(nx_axis_accept(&axis, &command, 2000), NX_ERROR_STATE);
    EXPECT_EQ(axis.command.received_at, 1000U);
    EXPECT_EQ(axis.command.valid_until, 4000U);
}

TEST_F(AxisControl, InvalidCommandsHaveNoPartialEffectOrReceiptRefresh) {
    auto command = velocity(1);
    command.received_at = 1001;
    const auto before = axis;
    EXPECT_EQ(nx_axis_accept(&axis, &command, 1000), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&before, &axis, sizeof(axis)), 0);
    command.received_at = 1000;
    command.valid_until = 1000;
    EXPECT_EQ(nx_axis_accept(&axis, &command, 1000), NX_ERROR_TIMEOUT);
    command.valid_until = 2000;
    command.velocity_rad_s = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(nx_axis_accept(&axis, &command, 1000), NX_ERROR_INVALID);
    EXPECT_EQ(axis.accepted_sequence, 0U);
}

TEST_F(AxisControl, TotalCurrentIncludesFeedforwardAndDownstreamLimits) {
    auto command = velocity(2);
    command.current_a = 2;
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    limits = {-0.5F, 0.5F};
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    EXPECT_GT(out.requested_current_a, 0.5F);
    EXPECT_FLOAT_EQ(out.current_a, 0.5F);
    EXPECT_FLOAT_EQ(axis.integral_a, 0);
    ASSERT_EQ(tick(3000), NX_SUCCESS);
    EXPECT_FLOAT_EQ(axis.integral_a, 0);
}

TEST_F(AxisControl, ExpiryAtEqualityControlsStopThenLatchesWithoutRearming) {
    auto command = velocity(2, 1, 3000);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000, 1), NX_SUCCESS);
    ASSERT_EQ(tick(3000, 1), NX_SUCCESS);
    EXPECT_TRUE(out.command_expired);
    EXPECT_EQ(out.state, NX_AXIS_STOPPING);
    EXPECT_LT(out.reference_velocity_rad_s, 1);
    EXPECT_LT(out.current_a, 0);
    command.sequence = 2;
    command.received_at = 3000;
    command.valid_until = 10000;
    EXPECT_EQ(nx_axis_accept(&axis, &command, 3000), NX_ERROR_STATE);
    ASSERT_EQ(tick(4000), NX_SUCCESS);
    ASSERT_EQ(tick(5000), NX_SUCCESS);
    ASSERT_EQ(tick(6000), NX_SUCCESS);
    EXPECT_EQ(out.state, NX_AXIS_FAULT);
    EXPECT_EQ(out.fault, NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_axis_clear_fault(&axis), NX_ERROR_STATE);
    ASSERT_EQ(nx_axis_disable(&axis), NX_SUCCESS);
    EXPECT_EQ(nx_axis_enable(&axis, &feedback, 6000), NX_ERROR_STATE);
    ASSERT_EQ(nx_axis_clear_fault(&axis), NX_SUCCESS);
    EXPECT_EQ(axis.state, NX_AXIS_DISABLED);
    ASSERT_EQ(nx_axis_enable(&axis, &feedback, 6000), NX_SUCCESS);
    EXPECT_EQ(axis.state, NX_AXIS_READY);
}

TEST_F(AxisControl, ExplicitStopRetainsClosedLoopUntilMeasuredDwell) {
    auto command = velocity(2);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000, 1), NX_SUCCESS);
    ASSERT_EQ(nx_axis_request_stop(&axis), NX_SUCCESS);
    ASSERT_EQ(tick(3000, 1), NX_SUCCESS);
    EXPECT_EQ(out.state, NX_AXIS_STOPPING);
    EXPECT_LT(out.current_a, 0);
    ASSERT_EQ(tick(4000), NX_SUCCESS);
    ASSERT_EQ(tick(5000), NX_SUCCESS);
    EXPECT_EQ(out.state, NX_AXIS_STOPPING);
    ASSERT_EQ(tick(6000), NX_SUCCESS);
    EXPECT_EQ(out.state, NX_AXIS_READY);
    EXPECT_EQ(out.fault, NX_SUCCESS);
}

TEST_F(AxisControl, ExpiredPendingCommandIsNeverRecordedAsAdopted) {
    auto command = velocity(2, 1, 1500);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    EXPECT_EQ(out.command_sequence, 0U);
    EXPECT_EQ(out.adopted_at, 0U);
    EXPECT_TRUE(out.command_expired);
    EXPECT_FLOAT_EQ(out.current_a, 0);
}

TEST_F(AxisControl, RepeatedStopRequestDoesNotRestartSettlingDwell) {
    auto command = velocity(1);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    ASSERT_EQ(nx_axis_request_stop(&axis), NX_SUCCESS);
    ASSERT_EQ(tick(3000), NX_SUCCESS);
    ASSERT_EQ(nx_axis_request_stop(&axis), NX_SUCCESS);
    ASSERT_EQ(tick(4000), NX_SUCCESS);
    ASSERT_EQ(nx_axis_request_stop(&axis), NX_SUCCESS);
    ASSERT_EQ(tick(5000), NX_SUCCESS);
    EXPECT_EQ(out.state, NX_AXIS_READY);
}

TEST_F(AxisControl, InvalidFeedbackCannotAdvanceControlOrOverwriteDecision) {
    auto command = velocity(1);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    const float integral = axis.integral_a;
    const auto previous = out;
    feedback.valid = false;
    EXPECT_EQ(tick(3000), NX_ERROR_STATE);
    EXPECT_EQ(axis.state, NX_AXIS_FAULT);
    EXPECT_FLOAT_EQ(axis.integral_a, integral);
    EXPECT_EQ(std::memcmp(&out, &previous, sizeof(out)), 0);
}

TEST_F(AxisControl, ExplicitHoldingKeepsZeroSpeedRegulationAndClosesCommands) {
    config.stop_policy = NX_AXIS_HOLD_ZERO_VELOCITY;
    ASSERT_EQ(nx_axis_disable(&axis), NX_SUCCESS);
    ASSERT_EQ(nx_axis_initialize(&axis, &config, &parameters), NX_SUCCESS);
    ASSERT_EQ(nx_axis_enable(&axis, &feedback, 1000), NX_SUCCESS);
    auto command = velocity(0);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000, -1), NX_SUCCESS);
    ASSERT_GT(axis.integral_a, 0);
    ASSERT_EQ(nx_axis_request_stop(&axis), NX_SUCCESS);
    ASSERT_EQ(tick(3000), NX_SUCCESS);
    ASSERT_EQ(tick(4000), NX_SUCCESS);
    ASSERT_EQ(tick(5000), NX_SUCCESS);
    EXPECT_EQ(out.state, NX_AXIS_HOLDING);
    EXPECT_GT(out.current_a, 0);
    ASSERT_EQ(tick(6000, -0.1F), NX_SUCCESS);
    EXPECT_GT(out.current_a, 0.2F);
    command = velocity(2, 2);
    EXPECT_EQ(nx_axis_accept(&axis, &command, 6000), NX_ERROR_STATE);
    EXPECT_EQ(nx_axis_clear_fault(&axis), NX_ERROR_STATE);
    EXPECT_EQ(nx_axis_request_stop(&axis), NX_SUCCESS);
    ASSERT_EQ(tick(7000), NX_SUCCESS);
    EXPECT_EQ(out.state, NX_AXIS_HOLDING);
    feedback.valid = false;
    EXPECT_EQ(tick(8000), NX_ERROR_STATE);
    EXPECT_EQ(axis.state, NX_AXIS_FAULT);
}

TEST_F(AxisControl, ExpiredCommandCanLatchReasonWhileHoldingRemainsActive) {
    config.stop_policy = NX_AXIS_HOLD_ZERO_VELOCITY;
    ASSERT_EQ(nx_axis_disable(&axis), NX_SUCCESS);
    ASSERT_EQ(nx_axis_initialize(&axis, &config, &parameters), NX_SUCCESS);
    ASSERT_EQ(nx_axis_enable(&axis, &feedback, 1000), NX_SUCCESS);
    auto command = velocity(0, 1, 3000);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    ASSERT_EQ(tick(3000), NX_SUCCESS);
    ASSERT_EQ(tick(4000), NX_SUCCESS);
    ASSERT_EQ(tick(5000), NX_SUCCESS);
    EXPECT_EQ(out.state, NX_AXIS_HOLDING);
    EXPECT_EQ(out.fault, NX_ERROR_TIMEOUT);
    ASSERT_EQ(tick(6000), NX_SUCCESS);
    EXPECT_EQ(out.state, NX_AXIS_HOLDING);
    EXPECT_TRUE(out.command_expired);
    ASSERT_EQ(nx_axis_disable(&axis), NX_SUCCESS);
    ASSERT_EQ(nx_axis_clear_fault(&axis), NX_SUCCESS);
    EXPECT_EQ(axis.state, NX_AXIS_DISABLED);
}

TEST_F(AxisControl, MissedFeedbackSequenceAndWrongDtCloseAdmission) {
    feedback.sequence += 2;
    feedback.interval_start = 1000;
    feedback.sampled_at = 2000;
    EXPECT_EQ(nx_axis_step(&axis, &feedback, &limits, 2000, 0.001F, &out),
              NX_ERROR_STATE);
    EXPECT_EQ(axis.state, NX_AXIS_FAULT);
    nx_axis_t other{};
    ASSERT_EQ(nx_axis_initialize(&other, &config, &parameters), NX_SUCCESS);
    ASSERT_EQ(nx_axis_enable(&other, &feedback, 2000), NX_SUCCESS);
    feedback.sequence++;
    feedback.interval_start = 2000;
    feedback.sampled_at = 3000;
    EXPECT_EQ(nx_axis_step(&other, &feedback, &limits, 3000, 0.002F, &out),
              NX_ERROR_STATE);
}

TEST_F(AxisControl, PositionProfileCompletionIsNotMeasuredArrival) {
    nx_axis_command_t command{NX_AXIS_POSITION, 1,     7, 1, 1000,
                              100000,           0.01F, 0, 0};
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    for (nx_time_us_t t = 2000; t <= 9000; t += 1000) {
        ASSERT_EQ(tick(t), NX_SUCCESS);
    }
    EXPECT_TRUE(out.trajectory_complete);
    EXPECT_FALSE(out.target_reached);
    ASSERT_EQ(tick(10000, 0, 0.01F), NX_SUCCESS);
    ASSERT_EQ(tick(11000, 0, 0.01F), NX_SUCCESS);
    EXPECT_FALSE(out.target_reached);
    ASSERT_EQ(tick(12000, 0, 0.01F), NX_SUCCESS);
    EXPECT_TRUE(out.target_reached);
    ASSERT_EQ(tick(13000, 0.1F, 0.01F), NX_SUCCESS);
    EXPECT_FALSE(out.target_reached);
}

TEST_F(AxisControl, PositionReplanningAndModeChangeRequireTheSelectedState) {
    nx_axis_command_t command{NX_AXIS_POSITION, 1, 7, 1, 1000, 100000, 1, 0, 0};
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    command.sequence = 2;
    command.received_at = 2000;
    EXPECT_EQ(nx_axis_accept(&axis, &command, 2000), NX_ERROR_BUSY);
    auto speed = velocity(1, 2);
    EXPECT_EQ(nx_axis_accept(&axis, &speed, 2000), NX_ERROR_STATE);
}

TEST_F(AxisControl, WholeRevisionSwitchesAtCycleBoundaryAndPreservesOutput) {
    parameters.velocity_ki = 0;
    ASSERT_EQ(nx_axis_disable(&axis), NX_SUCCESS);
    ASSERT_EQ(nx_axis_initialize(&axis, &config, &parameters), NX_SUCCESS);
    ASSERT_EQ(nx_axis_enable(&axis, &feedback, 1000), NX_SUCCESS);
    auto command = velocity(1);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    const float old_output = out.current_a;
    parameters.revision = 2;
    parameters.velocity_kp = 1;
    ASSERT_EQ(nx_axis_stage_parameters(&axis, &parameters), NX_SUCCESS);
    EXPECT_EQ(axis.parameters.revision, 1U);
    ASSERT_EQ(tick(3000), NX_SUCCESS);
    EXPECT_EQ(out.parameters_revision, 2U);
    EXPECT_FLOAT_EQ(out.current_a, old_output);
}

TEST_F(AxisControl, RejectedRevisionPreservesActiveAndPendingParameters) {
    parameters.revision = 2;
    parameters.acceleration_rad_s2 = -1;
    const auto before = axis;
    EXPECT_EQ(nx_axis_stage_parameters(&axis, &parameters), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&before, &axis, sizeof(axis)), 0);
    parameters.acceleration_rad_s2 = 1000;
    ASSERT_EQ(nx_axis_stage_parameters(&axis, &parameters), NX_SUCCESS);
    parameters.revision = 3;
    EXPECT_EQ(nx_axis_stage_parameters(&axis, &parameters), NX_ERROR_BUSY);
}

TEST_F(AxisControl, DownstreamTrackingIsCausalAndCanOnlyRunOncePerDecision) {
    auto command = velocity(1);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    const auto before = axis;
    EXPECT_EQ(nx_axis_track_current(&axis, out.sample_sequence - 1, 0, 0.001F),
              NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&before, &axis, sizeof(axis)), 0);
    ASSERT_EQ(nx_axis_track_current(&axis, out.sample_sequence, 0, 0.001F),
              NX_SUCCESS);
    EXPECT_LT(axis.integral_a, before.integral_a);
    EXPECT_EQ(nx_axis_track_current(&axis, out.sample_sequence, 0, 0.001F),
              NX_ERROR_STATE);
}

TEST_F(AxisControl, PendingParametersCannotDiscardAcceptedCurrentTracking) {
    auto command = velocity(1);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    parameters.revision = 2;
    parameters.velocity_kp = 1;
    ASSERT_EQ(nx_axis_stage_parameters(&axis, &parameters), NX_SUCCESS);
    const auto before = axis;
    EXPECT_EQ(nx_axis_track_current(&axis, out.sample_sequence, 0, 0.001F),
              NX_ERROR_BUSY);
    EXPECT_EQ(std::memcmp(&before, &axis, sizeof(axis)), 0);
}

TEST_F(AxisControl, TrackingRejectsAnIntervalFromAnotherControlCycle) {
    auto command = velocity(1);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    const auto before = axis;
    EXPECT_EQ(nx_axis_track_current(&axis, out.sample_sequence, 0, 0.1F),
              NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&before, &axis, sizeof(axis)), 0);
}

TEST_F(AxisControl, JoinedRestartDoesNotRetainPriorTrackingCycleIdentity) {
    auto command = velocity(1);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    ASSERT_EQ(nx_axis_track_current(&axis, out.sample_sequence, 0, 0.001F),
              NX_SUCCESS);
    ASSERT_EQ(nx_axis_disable(&axis), NX_SUCCESS);
    feedback.sequence = 1;
    feedback.interval_start = 2000;
    ASSERT_EQ(nx_axis_enable(&axis, &feedback, 2000), NX_SUCCESS);
    command = velocity(1, 2);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 2000), NX_SUCCESS);
    ASSERT_EQ(tick(3000), NX_SUCCESS);
    EXPECT_EQ(nx_axis_track_current(&axis, out.sample_sequence, 0, 0.001F),
              NX_SUCCESS);
}

TEST_F(AxisControl, IndependentInstancesAndCurrentModeRetainSeparateState) {
    nx_axis_t second{};
    ASSERT_EQ(nx_axis_initialize(&second, &config, &parameters), NX_SUCCESS);
    ASSERT_EQ(nx_axis_enable(&second, &feedback, 1000), NX_SUCCESS);
    auto speed = velocity(1);
    ASSERT_EQ(nx_axis_accept(&axis, &speed, 1000), NX_SUCCESS);
    nx_axis_command_t current{NX_AXIS_CURRENT, 1, 7, 1, 1000, 100000, 0, 0, 2};
    ASSERT_EQ(nx_axis_accept(&second, &current, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    nx_axis_output_t other{};
    ASSERT_EQ(nx_axis_step(&second, &feedback, &limits, 2000, 0.001F, &other),
              NX_SUCCESS);
    EXPECT_FLOAT_EQ(other.current_a, 2);
    EXPECT_FLOAT_EQ(second.integral_a, 0);
    EXPECT_GT(axis.integral_a, 0);
}

TEST(AxisInitialization, InvalidProfileNeverChangesCallerStorage) {
    nx_axis_t axis{};
    axis.accepted_sequence = 123;
    nx_axis_config_t config{0, 1, 1000, 1000, NX_AXIS_INHIBIT_AFTER_DWELL};
    nx_axis_parameters_t parameters{1,    2,    4,   100,   5,     3,    3, 10,
                                    1000, -100, 100, 0.01F, 0.02F, 2000, 0};
    const auto before = axis;
    EXPECT_EQ(nx_axis_initialize(&axis, &config, &parameters),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&before, &axis, sizeof(axis)), 0);
}

TEST_F(AxisControl, FullWidthClockEpochSurvivesCommandAndFeedbackBinding) {
    constexpr uint64_t epoch = (UINT64_C(1) << 40) + 7;
    config.epoch = static_cast<decltype(config.epoch)>(epoch);
    feedback.epoch = static_cast<decltype(feedback.epoch)>(epoch);
    ASSERT_EQ(nx_axis_disable(&axis), NX_SUCCESS);
    ASSERT_EQ(nx_axis_initialize(&axis, &config, &parameters), NX_SUCCESS);
    ASSERT_EQ(nx_axis_enable(&axis, &feedback, 1000), NX_SUCCESS);
    auto command = velocity(1);
    command.epoch = static_cast<decltype(command.epoch)>(epoch);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    EXPECT_EQ(axis.config.epoch, epoch);
    EXPECT_EQ(axis.feedback.epoch, epoch);
    EXPECT_EQ(axis.command.epoch, epoch);
}

TEST_F(AxisControl, UnknownStopPolicyRejectsBeforeStorageChange) {
    config.stop_policy = NX_AXIS_STOP_POLICY_INVALID;
    const auto before = axis;
    EXPECT_EQ(nx_axis_initialize(&axis, &config, &parameters),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&before, &axis, sizeof(axis)), 0);
}
TEST_F(AxisControl, StoppingUsesExplicitBiasInsteadOfPreviousAccelerationBias) {
    config.stop_policy = NX_AXIS_HOLD_ZERO_VELOCITY;
    parameters.stop_feedforward_a = 1;
    ASSERT_EQ(nx_axis_disable(&axis), NX_SUCCESS);
    ASSERT_EQ(nx_axis_initialize(&axis, &config, &parameters), NX_SUCCESS);
    ASSERT_EQ(nx_axis_enable(&axis, &feedback, 1000), NX_SUCCESS);
    auto command = velocity(0);
    command.current_a = 2;
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out.current_a, 2);
    ASSERT_EQ(nx_axis_request_stop(&axis), NX_SUCCESS);
    ASSERT_EQ(tick(3000), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out.current_a, 1);
    ASSERT_EQ(tick(4000), NX_SUCCESS);
    ASSERT_EQ(tick(5000), NX_SUCCESS);
    EXPECT_EQ(out.state, NX_AXIS_HOLDING);
    EXPECT_FLOAT_EQ(out.current_a, 1);
}

TEST_F(AxisControl, HoldingBiasRespectsTotalDeratingAndConditionalIntegration) {
    config.stop_policy = NX_AXIS_HOLD_ZERO_VELOCITY;
    parameters.stop_feedforward_a = 2;
    ASSERT_EQ(nx_axis_disable(&axis), NX_SUCCESS);
    ASSERT_EQ(nx_axis_initialize(&axis, &config, &parameters), NX_SUCCESS);
    ASSERT_EQ(nx_axis_enable(&axis, &feedback, 1000), NX_SUCCESS);
    auto command = velocity(0);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 1000), NX_SUCCESS);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    ASSERT_EQ(nx_axis_request_stop(&axis), NX_SUCCESS);
    limits = {-0.5F, 0.5F};
    ASSERT_EQ(tick(3000), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out.current_a, 0.5F);
    ASSERT_EQ(tick(4000), NX_SUCCESS);
    ASSERT_EQ(tick(5000), NX_SUCCESS);
    ASSERT_EQ(tick(6000, -0.1F), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out.current_a, 0.5F);
    EXPECT_FLOAT_EQ(axis.integral_a, 0);
    ASSERT_EQ(tick(7000, 0.1F), NX_SUCCESS);
    EXPECT_LT(axis.integral_a, 0);
}

TEST_F(AxisControl, StopBiasChangesOnlyWithTheWholeParameterRevision) {
    parameters.revision = 2;
    parameters.stop_feedforward_a = 1.5F;
    ASSERT_EQ(nx_axis_stage_parameters(&axis, &parameters), NX_SUCCESS);
    EXPECT_FLOAT_EQ(axis.parameters.stop_feedforward_a, 0);
    ASSERT_EQ(tick(2000), NX_SUCCESS);
    EXPECT_EQ(out.parameters_revision, 2U);
    auto command = velocity(0);
    ASSERT_EQ(nx_axis_accept(&axis, &command, 2000), NX_SUCCESS);
    ASSERT_EQ(tick(3000), NX_SUCCESS);
    ASSERT_EQ(nx_axis_request_stop(&axis), NX_SUCCESS);
    ASSERT_EQ(tick(4000), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out.current_a, 1.5F);
}

TEST_F(AxisControl, InvalidStopBiasCannotChangeCurrentOrPendingParameters) {
    parameters.revision = 2;
    parameters.stop_feedforward_a = 4;
    const auto before = axis;
    EXPECT_EQ(nx_axis_stage_parameters(&axis, &parameters), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&before, &axis, sizeof(axis)), 0);
    parameters.stop_feedforward_a = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(nx_axis_initialize(&axis, &config, &parameters),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&before, &axis, sizeof(axis)), 0);
}
} /* namespace */
