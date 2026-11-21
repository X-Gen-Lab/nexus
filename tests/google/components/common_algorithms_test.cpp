/**
 * \file            common_algorithms_test.cpp
 * \brief           Real scalar, streaming and control algorithm contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/control.h"
#include "nexus/algorithms/crc.h"
#include "nexus/algorithms/dsp.h"
#include "nexus/algorithms/endian.h"
#include "nexus/algorithms/filter.h"
#include "nexus/algorithms/interpolation.h"
#include "nexus/algorithms/numeric.h"
#include "nexus/algorithms/pid.h"
#include "nexus/algorithms/statistics.h"
#include <array>
#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>

namespace {
TEST(CommonNumeric, FixedPointRoundsSignedTiesAndSaturatesEndpoints) {
    EXPECT_EQ(nx_saturate_i16(INT32_MAX), INT16_MAX);
    EXPECT_EQ(nx_saturate_i16(INT32_MIN), INT16_MIN);
    EXPECT_EQ(nx_q15_multiply(INT16_MIN, INT16_MIN), INT16_MAX);
    EXPECT_EQ(nx_q15_multiply(1, 16384), 1);
    EXPECT_EQ(nx_q15_multiply(-1, 16384), -1);
    EXPECT_EQ(nx_q31_multiply(INT32_MIN, INT32_MIN), INT32_MAX);
    EXPECT_EQ(nx_q31_multiply(1, 1073741824), 1);
    EXPECT_EQ(nx_q31_multiply(-1, 1073741824), -1);
    int16_t value = 17;
    EXPECT_EQ(nx_q15_from_f32(1.0F, &value), NX_SUCCESS);
    EXPECT_EQ(value, INT16_MAX);
    EXPECT_EQ(nx_q15_from_f32(-1.0F, &value), NX_SUCCESS);
    EXPECT_EQ(value, INT16_MIN);
    EXPECT_EQ(nx_q15_from_f32(std::numeric_limits<float>::infinity(), &value),
              NX_ERROR_INVALID);
    EXPECT_EQ(value, INT16_MIN);
    float out = 91.0F;
    EXPECT_EQ(nx_clamp_f32(8.0F, -1.0F, 4.0F, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 4.0F);
    EXPECT_EQ(nx_clamp_f32(2.0F, 4.0F, -1.0F, &out), NX_ERROR_INVALID);
    EXPECT_FLOAT_EQ(out, 4.0F);
}

TEST(CommonCrc, CatalogueVectorsAndChunkedUpdateShareOneDefinition) {
    const char text[] = "123456789";
    const std::array<const nx_crc_model_t*, 3> models = {
        &nx_crc32_iso_hdlc, &nx_crc16_modbus, &nx_crc8_smbus};
    const std::array<uint32_t, 3> expected = {0xCBF43926U, 0x4B37U, 0xF4U};
    for (size_t index = 0; index < models.size(); ++index) {
        const auto* model = models[index];
        uint32_t state = model->initial;
        ASSERT_EQ(nx_crc_update(model, text, 3, &state), NX_SUCCESS);
        ASSERT_EQ(nx_crc_update(model, text + 3, 6, &state), NX_SUCCESS);
        uint32_t out = 0;
        EXPECT_EQ(nx_crc_finish(model, state, &out), NX_SUCCESS);
        EXPECT_EQ(out, expected[index]);
        const auto old = state;
        EXPECT_EQ(nx_crc_update(model, nullptr, 0, &state), NX_SUCCESS);
        EXPECT_EQ(state, old);
        EXPECT_EQ(nx_crc_update(model, nullptr, 1, &state), NX_ERROR_INVALID);
        EXPECT_EQ(state, old);
    }
    auto invalid = nx_crc8_smbus;
    invalid.width = 0;
    uint32_t state = 42;
    EXPECT_EQ(nx_crc_update(&invalid, text, 9, &state), NX_ERROR_INVALID);
    EXPECT_EQ(state, 42U);
}

TEST(CommonEndian, UnalignedBothOrdersAndOverflowDoesNotWrite) {
    std::array<uint8_t, 10> data = {};
    ASSERT_EQ(nx_endian_write(data.data() + 1, 4, false, 0x12345678),
              NX_SUCCESS);
    EXPECT_EQ(data[1], 0x12);
    EXPECT_EQ(data[4], 0x78);
    uint64_t value = 0;
    ASSERT_EQ(nx_endian_read(data.data() + 1, 4, false, &value), NX_SUCCESS);
    EXPECT_EQ(value, 0x12345678U);
    ASSERT_EQ(nx_endian_read(data.data() + 1, 4, true, &value), NX_SUCCESS);
    EXPECT_EQ(value, 0x78563412U);
    const auto before = data;
    EXPECT_EQ(nx_endian_write(data.data(), 2, true, 65536), NX_ERROR_OVERFLOW);
    EXPECT_EQ(data, before);
    EXPECT_EQ(nx_endian_read(data.data(), 3, true, &value), NX_ERROR_INVALID);
    EXPECT_EQ(value, 0x78563412U);
}

TEST(CommonStatistics, StableVariancePopulationSampleAndInvalidIsolation) {
    nx_statistics_t state = {};
    nx_statistics_result_t result = {};
    result.count = 91;
    EXPECT_EQ(nx_statistics_read(&state, false, &result), NX_ERROR_EMPTY);
    EXPECT_EQ(result.count, 91U);
    for (double sample : {1000000001.0, 1000000002.0, 1000000003.0}) {
        ASSERT_EQ(nx_statistics_push(&state, sample), NX_SUCCESS);
    }
    ASSERT_EQ(nx_statistics_read(&state, false, &result), NX_SUCCESS);
    EXPECT_DOUBLE_EQ(result.mean, 1000000002.0);
    EXPECT_NEAR(result.variance, 2.0 / 3.0, 1e-12);
    EXPECT_DOUBLE_EQ(result.minimum, 1000000001.0);
    EXPECT_DOUBLE_EQ(result.maximum, 1000000003.0);
    ASSERT_EQ(nx_statistics_read(&state, true, &result), NX_SUCCESS);
    EXPECT_DOUBLE_EQ(result.variance, 1.0);
    const auto before = state;
    EXPECT_EQ(
        nx_statistics_push(&state, std::numeric_limits<double>::quiet_NaN()),
        NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&state, &before, sizeof(state)), 0);
    nx_statistics_t independent = {};
    ASSERT_EQ(nx_statistics_push(&independent, 3), NX_SUCCESS);
    ASSERT_EQ(nx_statistics_push(&independent, 4), NX_SUCCESS);
    ASSERT_EQ(nx_statistics_read(&independent, false, &result), NX_SUCCESS);
    EXPECT_DOUBLE_EQ(result.rms, std::sqrt(12.5));
    EXPECT_EQ(state.count, 3U);
}

TEST(CommonFilters, StartupWindowsMedianAndRejectionPreserveHistory) {
    nx_ema_t ema = {};
    float out = 0;
    ASSERT_EQ(nx_ema_step(&ema, 4, 0.25F, &out), NX_SUCCESS);
    ASSERT_EQ(nx_ema_step(&ema, 8, 0.25F, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 5);
    EXPECT_EQ(nx_ema_step(&ema, 100, 0, &out), NX_ERROR_INVALID);
    EXPECT_FLOAT_EQ(ema.value, 5);
    float samples[2] = {};
    nx_moving_mean_t mean = {};
    ASSERT_EQ(nx_moving_mean_initialize(&mean, samples, 2), NX_SUCCESS);
    ASSERT_EQ(nx_moving_mean_step(&mean, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 1);
    ASSERT_EQ(nx_moving_mean_step(&mean, 3, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 2);
    ASSERT_EQ(nx_moving_mean_step(&mean, 5, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 4);
    const float window[] = {9, 1, 4, 3, 7};
    float scratch[5] = {};
    ASSERT_EQ(nx_median_f32(window, 5, scratch, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 4);
    EXPECT_EQ(nx_median_f32(window, 4, scratch, &out), NX_ERROR_INVALID);
    EXPECT_FLOAT_EQ(out, 4);
    EXPECT_FLOAT_EQ(window[0], 9);
}

TEST(CommonFilters, MovingMeanRecoversSmallSamplesAfterLargeValueLeavesWindow) {
    float samples[2] = {};
    nx_moving_mean_t mean = {};
    ASSERT_EQ(nx_moving_mean_initialize(&mean, samples, 2), NX_SUCCESS);
    float out = 0;
    ASSERT_EQ(nx_moving_mean_step(&mean, 1e20F, &out), NX_SUCCESS);
    ASSERT_EQ(nx_moving_mean_step(&mean, 1, &out), NX_SUCCESS);
    ASSERT_EQ(nx_moving_mean_step(&mean, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 1);
    ASSERT_EQ(nx_moving_mean_step(&mean, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 1);
}

TEST(CommonDsp, FirImpulseAndSosSignConventionMatchReference) {
    const float taps[] = {0.25F, 0.5F, 0.25F};
    float history[3] = {};
    nx_fir_f32_t fir = {};
    ASSERT_EQ(nx_fir_f32_initialize(&fir, taps, history, 3), NX_SUCCESS);
    bool first = true;
    for (float expected : {0.25F, 0.5F, 0.25F, 0.0F}) {
        float out = 99;
        ASSERT_EQ(nx_fir_f32_step(&fir, first ? 1.0F : 0.0F, &out), NX_SUCCESS);
        EXPECT_FLOAT_EQ(out, expected);
        first = false;
    }
    const nx_sos_f32_coefficients_t coefficient = {0.5F, 0, 0, -0.5F, 0};
    nx_sos_f32_state_t live[1] = {}, scratch[1] = {};
    nx_sos_f32_t sos = {};
    ASSERT_EQ(nx_sos_f32_initialize(&sos, &coefficient, live, scratch, 1),
              NX_SUCCESS);
    float out = 0;
    ASSERT_EQ(nx_sos_f32_step(&sos, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 0.5F);
    ASSERT_EQ(nx_sos_f32_step(&sos, 0, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 0.25F);
    const auto before = live[0];
    EXPECT_EQ(
        nx_sos_f32_step(&sos, std::numeric_limits<float>::infinity(), &out),
        NX_ERROR_INVALID);
    EXPECT_FLOAT_EQ(live[0].first, before.first);
    auto unstable = coefficient;
    unstable.a2 = 1;
    EXPECT_EQ(nx_sos_f32_initialize(&sos, &unstable, live, scratch, 1),
              NX_ERROR_INVALID);
}

TEST(CommonInterpolation, NodesEdgesAndBoundarySelectionAreExplicit) {
    const float x[] = {0, 2, 5};
    const float y[] = {10, 20, 50};
    nx_interpolation_t table = {};
    ASSERT_EQ(
        nx_interpolation_initialize(&table, x, y, 3, NX_INTERPOLATE_REJECT),
        NX_SUCCESS);
    float out = -91;
    ASSERT_EQ(nx_interpolation_evaluate(&table, 3, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 30);
    EXPECT_EQ(nx_interpolation_evaluate(&table, -1, &out), NX_ERROR_INVALID);
    EXPECT_FLOAT_EQ(out, 30);
    table.boundary = NX_INTERPOLATE_CLAMP;
    ASSERT_EQ(nx_interpolation_evaluate(&table, -1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 10);
    table.boundary = NX_INTERPOLATE_EXTRAPOLATE;
    ASSERT_EQ(nx_interpolation_evaluate(&table, -1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 5);
    const float bad[] = {0, 0, 1};
    const auto before = table;
    EXPECT_EQ(
        nx_interpolation_initialize(&table, bad, y, 3, NX_INTERPOLATE_CLAMP),
        NX_ERROR_INVALID);
    EXPECT_EQ(table.x, before.x);
}

TEST(CommonControl, ExplicitTimeRatesIntegrationAndFilteredDerivative) {
    float out = 99;
    ASSERT_EQ(nx_slew_f32(0, 10, 2, 3, 0.5F, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 1);
    ASSERT_EQ(nx_slew_f32(3, -10, 2, 3, 0.5F, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 1.5F);
    float integral = 0;
    ASSERT_EQ(nx_integrate_f32(&integral, 4, 1, -2, 2), NX_SUCCESS);
    EXPECT_FLOAT_EQ(integral, 2);
    ASSERT_EQ(nx_integrate_f32(&integral, -1, 1, -2, 2), NX_SUCCESS);
    EXPECT_FLOAT_EQ(integral, 1);
    nx_derivative_t derivative = {};
    ASSERT_EQ(nx_derivative_step(&derivative, 3, 1, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 0);
    ASSERT_EQ(nx_derivative_step(&derivative, 5, 1, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 1);
    const auto before = derivative;
    EXPECT_EQ(nx_derivative_step(&derivative, 9, 0, 1, &out), NX_ERROR_INVALID);
    EXPECT_FLOAT_EQ(derivative.previous, before.previous);
    EXPECT_FLOAT_EQ(out, 1);
}

nx_pid_config_t pid_config() {
    return {1, 1, 0, 0, -10, 10, -20, 20};
}

TEST(CommonPid, SaturationCannotWindUpAndOppositeErrorRecovers) {
    nx_pid_t pid = {};
    const auto config = pid_config();
    ASSERT_EQ(nx_pid_initialize(&pid, &config), NX_SUCCESS);
    float out = 0;
    for (unsigned count = 0; count < 100; ++count) {
        ASSERT_EQ(nx_pid_step(&pid, 20, 0, 1, &out), NX_SUCCESS);
        EXPECT_FLOAT_EQ(out, 10);
    }
    EXPECT_FLOAT_EQ(pid.integral, 0);
    ASSERT_EQ(nx_pid_step(&pid, -1, 0, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, -2);
    const auto before = pid;
    EXPECT_EQ(nx_pid_step(&pid, 1, 0, 0, &out), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&pid, &before, sizeof(pid)), 0);
    nx_pid_t other = {};
    ASSERT_EQ(nx_pid_initialize(&other, &config), NX_SUCCESS);
    EXPECT_FLOAT_EQ(other.integral, 0);
}

TEST(CommonPid, TrackingRetuneAndDerivativeOnMeasurementAvoidOutputKick) {
    nx_pid_t pid = {};
    auto config = pid_config();
    config.ki = 0;
    config.kd = 2;
    config.tau_s = 1;
    ASSERT_EQ(nx_pid_initialize(&pid, &config), NX_SUCCESS);
    ASSERT_EQ(nx_pid_track(&pid, 3, 1, 7), NX_SUCCESS);
    float out = 0;
    ASSERT_EQ(nx_pid_step(&pid, 3, 1, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 7);
    config.kp = 2;
    ASSERT_EQ(nx_pid_reconfigure(&pid, &config), NX_SUCCESS);
    ASSERT_EQ(nx_pid_step(&pid, 3, 1, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 7);
    ASSERT_EQ(nx_pid_step(&pid, 4, 1, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 9);
    EXPECT_FLOAT_EQ(pid.derivative.filtered, 0);
    const auto previous = pid.integral;
    ASSERT_EQ(nx_pid_backcalculate(&pid, 5, 0.5F, 1), NX_SUCCESS);
    EXPECT_FLOAT_EQ(pid.integral, previous - 2);
    auto rejected = config;
    rejected.output_maximum = 1;
    EXPECT_EQ(nx_pid_reconfigure(&pid, &rejected), NX_ERROR_INVALID);
    EXPECT_FLOAT_EQ(pid.config.output_maximum, 10);
}

TEST(CommonPid, IntegralCrossingAnOutputBoundReachesItAndThenRecovers) {
    nx_pid_t pid = {};
    const nx_pid_config_t config = {0, 1, 0, 0, 0, 1, 0, 10};
    ASSERT_EQ(nx_pid_initialize(&pid, &config), NX_SUCCESS);
    float out = 0;
    ASSERT_EQ(nx_pid_step(&pid, 0.6F, 0, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 0.6F);
    ASSERT_EQ(nx_pid_step(&pid, 0.6F, 0, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 1);
    EXPECT_FLOAT_EQ(pid.integral, 1);
    ASSERT_EQ(nx_pid_step(&pid, -0.1F, 0, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 0.9F);
}

TEST(CommonPid, DisabledDerivativeCannotRejectRepresentablePiOutput) {
    nx_pid_t pid = {};
    const nx_pid_config_t config = {1, 1, 0, 0, -10, 10, -20, 20};
    ASSERT_EQ(nx_pid_initialize(&pid, &config), NX_SUCCESS);
    const float maximum = std::numeric_limits<float>::max();
    float out = 91;
    ASSERT_EQ(nx_pid_step(&pid, -maximum, -maximum, 1, &out), NX_SUCCESS);
    ASSERT_EQ(nx_pid_step(&pid, maximum, maximum, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 0);
    auto enable = config;
    enable.kd = 1;
    ASSERT_EQ(nx_pid_reconfigure(&pid, &enable), NX_SUCCESS);
    ASSERT_EQ(nx_pid_step(&pid, maximum, maximum, 1, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 0);
}
} /* namespace */
