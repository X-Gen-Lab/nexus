/**
 * \file            common_attitude_test.cpp
 * \brief           Explicit six-axis attitude and calibration contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/attitude.h"
#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>

namespace {
TEST(CommonAttitude, VectorCalibrationAndQuaternionBodyToWorldRotation) {
    const nx_vec3f_t raw = {3, 5, 7};
    const nx_vec3_calibration_t calibration = {{1, 1, 1}, {2, 3, -1}};
    nx_vec3f_t value = {};
    ASSERT_EQ(nx_vec3_calibrate(&calibration, raw, &value), NX_SUCCESS);
    EXPECT_FLOAT_EQ(value.x, 4);
    EXPECT_FLOAT_EQ(value.y, 12);
    EXPECT_FLOAT_EQ(value.z, -6);
    const nx_quatf_t rotation = {std::sqrt(0.5F), 0, 0, std::sqrt(0.5F)};
    ASSERT_EQ(nx_quatf_rotate(rotation, {1, 0, 0}, &value), NX_SUCCESS);
    EXPECT_NEAR(value.x, 0, 1e-6F);
    EXPECT_NEAR(value.y, 1, 1e-6F);
    EXPECT_NEAR(value.z, 0, 1e-6F);
}

TEST(CommonAttitude, NormalizationHandlesScaleAndRejectsZeroWithoutWrites) {
    nx_quatf_t value = {};
    const float maximum = std::numeric_limits<float>::max();
    ASSERT_EQ(nx_quatf_normalize({maximum, 0, 0, maximum}, &value), NX_SUCCESS);
    EXPECT_NEAR(value.w, std::sqrt(0.5F), 1e-6F);
    EXPECT_NEAR(value.z, std::sqrt(0.5F), 1e-6F);
    const auto before = value;
    EXPECT_EQ(nx_quatf_normalize({0, 0, 0, 0}, &value), NX_ERROR_INVALID);
    EXPECT_EQ(nx_quatf_normalize(
                  {std::numeric_limits<float>::quiet_NaN(), 0, 0, 1}, &value),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&value, &before, sizeof(value)), 0);
}

TEST(CommonAttitude, GyroIntegratesKnownYawWithoutAbsoluteHeadingClaim) {
    nx_attitude6_t state = {};
    ASSERT_EQ(nx_attitude6_init(&state, {1, 0, 0, 0}), NX_SUCCESS);
    const nx_attitude6_config_t config = {2, 7, 12, 0.25F};
    nx_attitude6_result_t result = {};
    constexpr float half_pi = 1.57079632679489661923F;
    for (int index = 0; index < 100; ++index) {
        ASSERT_EQ(nx_attitude6_step(&state, &config, {0, 0, half_pi},
                                    {0, 0, 9.80665F}, 0.01F, &result),
                  NX_SUCCESS);
    }
    EXPECT_NEAR(result.orientation.w, std::sqrt(0.5F), 1e-5F);
    EXPECT_NEAR(result.orientation.z, std::sqrt(0.5F), 1e-5F);
    EXPECT_TRUE(result.acceleration_used);
}

TEST(CommonAttitude, GravityCorrectsTiltAndInvalidAccelerationUsesGyroOnly) {
    nx_attitude6_t state = {};
    ASSERT_EQ(nx_attitude6_init(&state, {0.9950042F, 0.0998334F, 0, 0}),
              NX_SUCCESS);
    const nx_attitude6_config_t config = {2, 7, 12, 0.25F};
    nx_attitude6_result_t result = {};
    const float before = state.orientation.x;
    ASSERT_EQ(nx_attitude6_step(&state, &config, {0, 0, 0}, {0, 0, 9.80665F},
                                0.01F, &result),
              NX_SUCCESS);
    EXPECT_LT(std::fabs(result.orientation.x), before);
    EXPECT_TRUE(result.acceleration_used);
    ASSERT_EQ(nx_attitude6_step(&state, &config, {0, 0, 0}, {0, 0, 0}, 0.01F,
                                &result),
              NX_SUCCESS);
    EXPECT_FALSE(result.acceleration_used);
}

TEST(CommonAttitude, InvalidTimingAndExcessiveAngularStepPreserveWholeState) {
    nx_attitude6_t state = {};
    ASSERT_EQ(nx_attitude6_init(&state, {1, 0, 0, 0}), NX_SUCCESS);
    const auto before = state;
    const nx_attitude6_config_t config = {2, 7, 12, 0.25F};
    nx_attitude6_result_t result = {};
    const auto old_result = result;
    EXPECT_EQ(
        nx_attitude6_step(&state, &config, {0, 0, 10}, {0, 0, 9}, 1, &result),
        NX_ERROR_STATE);
    EXPECT_EQ(
        nx_attitude6_step(&state, &config, {0, 0, 0}, {0, 0, 9}, 0, &result),
        NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&state, &before, sizeof(state)), 0);
    EXPECT_EQ(std::memcmp(&result, &old_result, sizeof(result)), 0);
}
} /* namespace */
