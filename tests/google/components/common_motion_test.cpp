/**
 * \file            common_motion_test.cpp
 * \brief           Calibration, modulo feedback and planned motion contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/calibration.h"
#include "nexus/algorithms/motion.h"
#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>

namespace {
TEST(CommonCalibration, ExplicitOffsetScaleAndRejectedOutput) {
    nx_calibration_f32_t calibration = {10.0F, 0.25F};
    float out = 91;
    ASSERT_EQ(nx_calibration_apply_f32(&calibration, 18, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 2);
    calibration.scale = -2;
    ASSERT_EQ(nx_calibration_apply_f32(&calibration, 11, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, -2);
    EXPECT_EQ(nx_calibration_apply_f32(
                  &calibration, std::numeric_limits<float>::quiet_NaN(), &out),
              NX_ERROR_INVALID);
    EXPECT_FLOAT_EQ(out, -2);
    calibration = {-std::numeric_limits<float>::max(), 2};
    EXPECT_EQ(nx_calibration_apply_f32(&calibration,
                                       std::numeric_limits<float>::max(), &out),
              NX_ERROR_EXHAUSTED);
    EXPECT_FLOAT_EQ(out, -2);
}

TEST(CommonMotion, AngleRangeBoundaryAndNonfiniteRejection) {
    constexpr float pi = 3.14159265358979323846F;
    float out = 91;
    ASSERT_EQ(nx_angle_wrap_f32(pi, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, -pi);
    ASSERT_EQ(nx_angle_wrap_f32(-pi, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, -pi);
    ASSERT_EQ(nx_angle_wrap_f32(4 * pi + 0.5F, &out), NX_SUCCESS);
    EXPECT_NEAR(out, 0.5F, 1e-6F);
    ASSERT_EQ(nx_angle_wrap_f32(std::numeric_limits<float>::max(), &out),
              NX_SUCCESS);
    EXPECT_GE(out, -pi);
    EXPECT_LT(out, pi);
    const float before = out;
    EXPECT_EQ(nx_angle_wrap_f32(std::numeric_limits<float>::infinity(), &out),
              NX_ERROR_INVALID);
    EXPECT_FLOAT_EQ(out, before);
}

TEST(CommonMotion, EncoderWrapBothDirectionsAndFirstSampleHasNoVelocity) {
    nx_encoder_tracker_t tracker = {};
    nx_encoder_estimate_t sample = {};
    ASSERT_EQ(nx_encoder_tracker_init(&tracker, 65536, 100, 20), NX_SUCCESS);
    ASSERT_EQ(nx_encoder_tracker_step(&tracker, 65530, 0.5F, 0.25F, &sample),
              NX_SUCCESS);
    EXPECT_EQ(sample.position_counts, 20);
    EXPECT_EQ(sample.delta_counts, 0);
    EXPECT_FALSE(sample.velocity_valid);
    ASSERT_EQ(nx_encoder_tracker_step(&tracker, 4, 0.5F, 0.25F, &sample),
              NX_SUCCESS);
    EXPECT_EQ(sample.position_counts, 30);
    EXPECT_EQ(sample.delta_counts, 10);
    EXPECT_FLOAT_EQ(sample.velocity, 20);
    EXPECT_TRUE(sample.velocity_valid);
    ASSERT_EQ(nx_encoder_tracker_step(&tracker, 65530, 0.5F, 0.25F, &sample),
              NX_SUCCESS);
    EXPECT_EQ(sample.position_counts, 20);
    EXPECT_EQ(sample.delta_counts, -10);
    EXPECT_FLOAT_EQ(sample.velocity, -20);
}

TEST(CommonMotion, EncoderFull32BitModulusAndIndependentState) {
    nx_encoder_tracker_t first = {};
    nx_encoder_tracker_t second = {};
    nx_encoder_estimate_t sample = {};
    ASSERT_EQ(nx_encoder_tracker_init(&first, UINT64_C(1) << 32, 20, 0),
              NX_SUCCESS);
    ASSERT_EQ(nx_encoder_tracker_init(&second, 1000, 10, -50), NX_SUCCESS);
    ASSERT_EQ(nx_encoder_tracker_step(&first, UINT32_MAX - 3U, 1, 1, &sample),
              NX_SUCCESS);
    ASSERT_EQ(nx_encoder_tracker_step(&second, 500, 1, 1, &sample), NX_SUCCESS);
    ASSERT_EQ(nx_encoder_tracker_step(&first, 2, 1, 1, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.position_counts, 6);
    EXPECT_EQ(sample.delta_counts, 6);
    EXPECT_EQ(second.position_counts, -50);
    EXPECT_EQ(second.previous, 500U);
}

TEST(CommonMotion, EncoderAmbiguityBoundsAndBadTimingPreserveState) {
    nx_encoder_tracker_t tracker = {};
    nx_encoder_estimate_t sample = {};
    ASSERT_EQ(nx_encoder_tracker_init(&tracker, 100, 10, 0), NX_SUCCESS);
    const auto initialized = tracker;
    EXPECT_EQ(nx_encoder_tracker_init(&tracker, 100, 50, 0), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&tracker, &initialized, sizeof(tracker)), 0);
    ASSERT_EQ(nx_encoder_tracker_step(&tracker, 0, 1, 1, &sample), NX_SUCCESS);
    const auto before = tracker;
    const auto old_sample = sample;
    EXPECT_EQ(nx_encoder_tracker_step(&tracker, 50, 1, 1, &sample),
              NX_ERROR_STATE);
    EXPECT_EQ(nx_encoder_tracker_step(&tracker, 11, 1, 1, &sample),
              NX_ERROR_STATE);
    EXPECT_EQ(nx_encoder_tracker_step(&tracker, 1, 1, 0, &sample),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_encoder_tracker_step(&tracker, 100, 1, 1, &sample),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&tracker, &before, sizeof(tracker)), 0);
    EXPECT_EQ(std::memcmp(&sample, &old_sample, sizeof(sample)), 0);
}

TEST(CommonMotion, EncoderPositionAndVelocityOverflowDoNotAdvanceHistory) {
    nx_encoder_tracker_t tracker = {};
    nx_encoder_estimate_t sample = {};
    ASSERT_EQ(nx_encoder_tracker_init(&tracker, 100, 10, INT64_MAX),
              NX_SUCCESS);
    ASSERT_EQ(nx_encoder_tracker_step(&tracker, 0, 1, 1, &sample), NX_SUCCESS);
    const auto before = tracker;
    const auto old_sample = sample;
    EXPECT_EQ(nx_encoder_tracker_step(&tracker, 1, 1, 1, &sample),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(std::memcmp(&tracker, &before, sizeof(tracker)), 0);
    EXPECT_EQ(std::memcmp(&sample, &old_sample, sizeof(sample)), 0);
    ASSERT_EQ(nx_encoder_tracker_init(&tracker, 100, 10, 0), NX_SUCCESS);
    ASSERT_EQ(nx_encoder_tracker_step(&tracker, 0, 1, 1, &sample), NX_SUCCESS);
    EXPECT_EQ(nx_encoder_tracker_step(&tracker, 2,
                                      std::numeric_limits<float>::max(), 0.5F,
                                      &sample),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(tracker.previous, 0U);
    EXPECT_EQ(tracker.position_counts, 0);
    EXPECT_EQ(nx_encoder_tracker_step(&tracker, 1, 1,
                                      std::numeric_limits<float>::denorm_min(),
                                      &sample),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(tracker.previous, 0U);
    ASSERT_EQ(nx_encoder_tracker_init(&tracker, 100, 10, INT64_MIN),
              NX_SUCCESS);
    ASSERT_EQ(nx_encoder_tracker_step(&tracker, 10, 1, 1, &sample), NX_SUCCESS);
    EXPECT_EQ(nx_encoder_tracker_step(&tracker, 9, 1, 1, &sample),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(tracker.previous, 10U);
    EXPECT_EQ(tracker.position_counts, INT64_MIN);
}

TEST(CommonMotion, TrapezoidHasAnalyticPhasesAndExactEndpoint) {
    nx_trajectory_plan_t plan = {};
    nx_trajectory_sample_t sample = {};
    ASSERT_EQ(nx_trajectory_plan(&plan, 0, 10, 2, 1), NX_SUCCESS);
    EXPECT_FLOAT_EQ(plan.total_s, 7);
    ASSERT_EQ(nx_trajectory_evaluate(&plan, 1, &sample), NX_SUCCESS);
    EXPECT_FLOAT_EQ(sample.position, 0.5F);
    EXPECT_FLOAT_EQ(sample.velocity, 1);
    EXPECT_FLOAT_EQ(sample.acceleration, 1);
    ASSERT_EQ(nx_trajectory_evaluate(&plan, 3, &sample), NX_SUCCESS);
    EXPECT_FLOAT_EQ(sample.position, 4);
    EXPECT_FLOAT_EQ(sample.velocity, 2);
    EXPECT_FLOAT_EQ(sample.acceleration, 0);
    ASSERT_EQ(nx_trajectory_evaluate(&plan, 6, &sample), NX_SUCCESS);
    EXPECT_FLOAT_EQ(sample.position, 9.5F);
    EXPECT_FLOAT_EQ(sample.velocity, 1);
    EXPECT_FLOAT_EQ(sample.acceleration, -1);
    ASSERT_EQ(nx_trajectory_evaluate(&plan, 7, &sample), NX_SUCCESS);
    EXPECT_FLOAT_EQ(sample.position, 10);
    EXPECT_FLOAT_EQ(sample.velocity, 0);
    EXPECT_FLOAT_EQ(sample.acceleration, 0);
    EXPECT_TRUE(sample.complete);
    ASSERT_EQ(nx_trajectory_evaluate(&plan, 1000, &sample), NX_SUCCESS);
    EXPECT_FLOAT_EQ(sample.position, 10);
}

TEST(CommonMotion, ShortMoveIsTriangularAndReverseUsesSignedKinematics) {
    nx_trajectory_plan_t plan = {};
    nx_trajectory_sample_t sample = {};
    ASSERT_EQ(nx_trajectory_plan(&plan, 0, 1, 10, 4), NX_SUCCESS);
    EXPECT_FLOAT_EQ(plan.total_s, 1);
    EXPECT_FLOAT_EQ(plan.cruise_s, 0);
    ASSERT_EQ(nx_trajectory_evaluate(&plan, 0.25F, &sample), NX_SUCCESS);
    EXPECT_FLOAT_EQ(sample.position, 0.125F);
    EXPECT_FLOAT_EQ(sample.velocity, 1);
    ASSERT_EQ(nx_trajectory_evaluate(&plan, 0.75F, &sample), NX_SUCCESS);
    EXPECT_FLOAT_EQ(sample.position, 0.875F);
    EXPECT_FLOAT_EQ(sample.velocity, 1);
    ASSERT_EQ(nx_trajectory_plan(&plan, 3, -1, 2, 2), NX_SUCCESS);
    ASSERT_EQ(nx_trajectory_evaluate(&plan, 0.5F, &sample), NX_SUCCESS);
    EXPECT_FLOAT_EQ(sample.position, 2.75F);
    EXPECT_FLOAT_EQ(sample.velocity, -1);
    EXPECT_FLOAT_EQ(sample.acceleration, -2);
}

TEST(CommonMotion, TrajectoryIsMonotonicAndRespectsVelocityBound) {
    nx_trajectory_plan_t plan = {};
    nx_trajectory_sample_t sample = {};
    ASSERT_EQ(nx_trajectory_plan(&plan, -4, 9, 3, 7), NX_SUCCESS);
    float previous = -4;
    for (int index = 0; index <= 100; ++index) {
        const float elapsed = plan.total_s * (static_cast<float>(index) / 100);
        ASSERT_EQ(nx_trajectory_evaluate(&plan, elapsed, &sample), NX_SUCCESS);
        EXPECT_GE(sample.position, previous);
        EXPECT_LE(sample.position, 9);
        EXPECT_LE(std::fabs(sample.velocity), 3);
        previous = sample.position;
    }
    EXPECT_TRUE(sample.complete);
}

TEST(CommonMotion, ZeroMoveAndInvalidPlanOrTimeDoNotPublishPartialState) {
    nx_trajectory_plan_t plan = {};
    nx_trajectory_sample_t sample = {};
    ASSERT_EQ(nx_trajectory_plan(&plan, 5, 5, 1, 1), NX_SUCCESS);
    ASSERT_EQ(nx_trajectory_evaluate(&plan, 0, &sample), NX_SUCCESS);
    EXPECT_TRUE(sample.complete);
    EXPECT_FLOAT_EQ(sample.position, 5);
    const auto before = plan;
    EXPECT_EQ(nx_trajectory_plan(&plan, 0, 1, 0, 1), NX_ERROR_INVALID);
    EXPECT_EQ(nx_trajectory_plan(&plan, 0, 1, 1,
                                 std::numeric_limits<float>::quiet_NaN()),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&plan, &before, sizeof(plan)), 0);
    const auto old_sample = sample;
    EXPECT_EQ(nx_trajectory_evaluate(&plan, -1, &sample), NX_ERROR_INVALID);
    EXPECT_EQ(nx_trajectory_evaluate(
                  &plan, std::numeric_limits<float>::quiet_NaN(), &sample),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&sample, &old_sample, sizeof(sample)), 0);
}
TEST(CommonMotion, SmallMoveAndUnrepresentablePhasesHaveExplicitResults) {
    nx_trajectory_plan_t plan = {};
    nx_trajectory_sample_t sample = {};
    const float start = 1000;
    const float target = std::nextafter(start, 2000.0F);
    ASSERT_EQ(nx_trajectory_plan(&plan, start, target, 2, 4), NX_SUCCESS);
    ASSERT_EQ(nx_trajectory_evaluate(&plan, plan.total_s / 2, &sample),
              NX_SUCCESS);
    EXPECT_GE(sample.position, start);
    EXPECT_LE(sample.position, target);
    EXPECT_FALSE(sample.complete);
    ASSERT_EQ(nx_trajectory_evaluate(&plan, plan.total_s, &sample), NX_SUCCESS);
    EXPECT_FLOAT_EQ(sample.position, target);
    const auto before = plan;
    EXPECT_EQ(nx_trajectory_plan(&plan, 0, 1, 1e-30F, 1), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(nx_trajectory_plan(&plan, -std::numeric_limits<float>::max(),
                                 std::numeric_limits<float>::max(), 1, 1),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(std::memcmp(&plan, &before, sizeof(plan)), 0);
}
} /* namespace */
