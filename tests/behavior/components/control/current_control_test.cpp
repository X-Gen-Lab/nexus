/**
 * \file            current_control_test.cpp
 * \brief           Calibrated current provenance and reconstruction contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/control/current.h"
#include <cmath>
#include <cstring>
#include <gtest/gtest.h>

namespace {
nx_current_calibration_t calibration() {
    nx_current_calibration_t c = {};
    c.identity = 42;
    c.revision = 3;
    c.maximum_code = 4095;
    c.maximum_skew_ticks = 10;
    c.maximum_current_a = 10;
    c.maximum_sum_a = 0.05F;
    for (unsigned i = 0; i != 3; ++i) {
        c.zero_code[i] = 2048;
        c.a_per_code[i] = 0.01F;
        c.direction[i] = 1;
    }
    return c;
}
nx_current_capture_t phase_capture() {
    nx_current_capture_t capture = {};
    capture.calibration_identity = 42;
    capture.calibration_revision = 3;
    capture.code[0] = 2148;
    capture.code[1] = capture.code[2] = 1998;
    capture.aperture_tick[0] = 100;
    capture.aperture_tick[1] = 102;
    capture.aperture_tick[2] = 105;
    capture.measured_mask = 7;
    capture.valid = true;
    return capture;
}
nx_current_bus_capture_t bus_capture() {
    nx_current_bus_capture_t capture = {};
    capture.calibration_identity = 42;
    capture.calibration_revision = 3;
    capture.code[0] = 2148;
    capture.code[1] = 2098;
    capture.aperture_tick[0] = 100;
    capture.aperture_tick[1] = 105;
    capture.switching_vector[0] = 4;
    capture.switching_vector[1] = 2;
    capture.valid = true;
    return capture;
}
TEST(CurrentControl, ThreeIndependentPhasesRetainIdentityAndApertureSpan) {
    const auto c = calibration();
    const auto capture = phase_capture();
    nx_phase_current_t out = {};
    ASSERT_EQ(nx_current_convert(&c, &capture, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out.phase_a[0], 1);
    EXPECT_FLOAT_EQ(out.phase_a[1], -0.5F);
    EXPECT_FLOAT_EQ(out.phase_a[2], -0.5F);
    EXPECT_EQ(out.measured_mask, 7);
    EXPECT_EQ(out.reconstructed_mask, 0);
    EXPECT_EQ(out.reference_tick, 100U);
    EXPECT_EQ(out.skew_ticks, 5U);
}
TEST(CurrentControl, TwoPhasesReconstructWithoutClaimingSumDiagnostic) {
    auto c = calibration();
    c.maximum_sum_a = 0;
    auto capture = phase_capture();
    capture.measured_mask = 3;
    capture.code[1] = 2098;
    capture.code[2] = UINT32_MAX;
    nx_phase_current_t out = {};
    ASSERT_EQ(nx_current_convert(&c, &capture, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out.phase_a[0], 1);
    EXPECT_FLOAT_EQ(out.phase_a[1], 0.5F);
    EXPECT_FLOAT_EQ(out.phase_a[2], -1.5F);
    EXPECT_EQ(out.measured_mask, 3);
    EXPECT_EQ(out.reconstructed_mask, 4);
}
TEST(CurrentControl, AllMissingPhaseChoicesAndDirectionAreExplicit) {
    auto c = calibration();
    c.direction[1] = -1;
    auto capture = phase_capture();
    capture.code[1] = 2098;
    for (const auto mask : {uint8_t{3}, uint8_t{5}, uint8_t{6}}) {
        capture.measured_mask = mask;
        nx_phase_current_t out = {};
        ASSERT_EQ(nx_current_convert(&c, &capture, &out), NX_SUCCESS);
        EXPECT_NEAR(out.phase_a[0], 1, 1e-6F);
        EXPECT_NEAR(out.phase_a[1], -0.5F, 1e-6F);
        EXPECT_NEAR(out.phase_a[2], -0.5F, 1e-6F);
        EXPECT_EQ(out.reconstructed_mask, 7U ^ mask);
    }
}
TEST(CurrentControl, IndependentSumFailureDoesNotPublishPartialOutput) {
    const auto c = calibration();
    auto capture = phase_capture();
    capture.code[2] = 2098;
    nx_phase_current_t out = {};
    out.phase_a[0] = 99;
    const auto before = out;
    EXPECT_EQ(nx_current_convert(&c, &capture, &out), NX_ERROR_IO);
    EXPECT_EQ(std::memcmp(&out, &before, sizeof(out)), 0);
}
TEST(CurrentControl, SaturationAndReconstructedOvercurrentReject) {
    auto c = calibration();
    auto capture = phase_capture();
    nx_phase_current_t out = {};
    for (const uint32_t rail : {0U, 4095U, UINT32_MAX}) {
        capture.code[0] = rail;
        EXPECT_EQ(nx_current_convert(&c, &capture, &out), NX_ERROR_OVERFLOW);
    }
    capture = phase_capture();
    capture.measured_mask = 3;
    capture.code[0] = capture.code[1] = 2648;
    EXPECT_EQ(nx_current_convert(&c, &capture, &out), NX_ERROR_IO);
}
TEST(CurrentControl, CalibrationIdentityRevisionAndSkewCannotBeInvented) {
    const auto c = calibration();
    auto capture = phase_capture();
    nx_phase_current_t out = {};
    ++capture.calibration_identity;
    EXPECT_EQ(nx_current_convert(&c, &capture, &out), NX_ERROR_STATE);
    capture = phase_capture();
    ++capture.calibration_revision;
    EXPECT_EQ(nx_current_convert(&c, &capture, &out), NX_ERROR_STATE);
    capture = phase_capture();
    capture.aperture_tick[2] = 111;
    EXPECT_EQ(nx_current_convert(&c, &capture, &out), NX_ERROR_STATE);
    capture.valid = false;
    EXPECT_EQ(nx_current_convert(&c, &capture, &out), NX_ERROR_STATE);
}
TEST(CurrentControl, InvalidCalibrationAndMasksReject) {
    auto c = calibration();
    EXPECT_EQ(nx_current_calibration_check(&c), NX_SUCCESS);
    c.a_per_code[1] = NAN;
    EXPECT_EQ(nx_current_calibration_check(&c), NX_ERROR_INVALID);
    c = calibration();
    c.direction[0] = 0;
    EXPECT_EQ(nx_current_calibration_check(&c), NX_ERROR_INVALID);
    c = calibration();
    c.identity = 0;
    EXPECT_EQ(nx_current_calibration_check(&c), NX_ERROR_INVALID);
    c = calibration();
    auto capture = phase_capture();
    nx_phase_current_t out = {};
    for (const auto mask : {uint8_t{0}, uint8_t{1}, uint8_t{2}, uint8_t{4},
                            uint8_t{8}, uint8_t{255}}) {
        capture.measured_mask = mask;
        EXPECT_EQ(nx_current_convert(&c, &capture, &out), NX_ERROR_INVALID);
    }
}
TEST(CurrentControl, SingleShuntReconstructsPositiveAndNegativeVectors) {
    const auto c = calibration();
    auto capture = bus_capture();
    nx_phase_current_t out = {};
    ASSERT_EQ(nx_current_reconstruct_bus(&c, &capture, &out), NX_SUCCESS);
    EXPECT_NEAR(out.phase_a[0], 1, 1e-6F);
    EXPECT_NEAR(out.phase_a[1], 0.5F, 1e-6F);
    EXPECT_NEAR(out.phase_a[2], -1.5F, 1e-6F);
    EXPECT_EQ(out.measured_mask, 3);
    EXPECT_EQ(out.reconstructed_mask, 4);
    capture.code[0] = 1948;
    capture.code[1] = 1998;
    capture.switching_vector[0] = 3;
    capture.switching_vector[1] = 5;
    ASSERT_EQ(nx_current_reconstruct_bus(&c, &capture, &out), NX_SUCCESS);
    EXPECT_NEAR(out.phase_a[0], 1, 1e-6F);
    EXPECT_NEAR(out.phase_a[1], 0.5F, 1e-6F);
    EXPECT_NEAR(out.phase_a[2], -1.5F, 1e-6F);
}
TEST(CurrentControl, SingleShuntRejectsZeroAndDependentVectors) {
    const auto c = calibration();
    auto capture = bus_capture();
    nx_phase_current_t out = {};
    out.phase_a[0] = 99;
    const auto before = out;
    for (const auto vector :
         {uint8_t{0}, uint8_t{7}, uint8_t{4}, uint8_t{3}, uint8_t{8}}) {
        capture.switching_vector[1] = vector;
        EXPECT_EQ(nx_current_reconstruct_bus(&c, &capture, &out),
                  NX_ERROR_INVALID);
        EXPECT_EQ(std::memcmp(&out, &before, sizeof(out)), 0);
    }
}
TEST(CurrentControl, TwoSampleTimeOrderingAndOverflowAreChecked) {
    const auto c = calibration();
    auto capture = bus_capture();
    nx_phase_current_t out = {};
    capture.aperture_tick[1] = capture.aperture_tick[0];
    EXPECT_EQ(nx_current_reconstruct_bus(&c, &capture, &out), NX_ERROR_STATE);
    capture.aperture_tick[1] = UINT32_MAX;
    EXPECT_EQ(nx_current_reconstruct_bus(&c, &capture, &out), NX_ERROR_STATE);
    capture = bus_capture();
    capture.code[1] = 4095;
    EXPECT_EQ(nx_current_reconstruct_bus(&c, &capture, &out),
              NX_ERROR_OVERFLOW);
}
TEST(CurrentControl, SeparateCalibrationsHaveNoSharedMutableState) {
    const auto first = calibration();
    auto second = first;
    second.identity = 84;
    second.a_per_code[0] = 0.02F;
    auto capture = phase_capture();
    capture.measured_mask = 3;
    nx_phase_current_t a = {}, b = {};
    ASSERT_EQ(nx_current_convert(&first, &capture, &a), NX_SUCCESS);
    capture.calibration_identity = second.identity;
    ASSERT_EQ(nx_current_convert(&second, &capture, &b), NX_SUCCESS);
    EXPECT_FLOAT_EQ(a.phase_a[0], 1);
    EXPECT_FLOAT_EQ(b.phase_a[0], 2);
}
TEST(CurrentControl, MissingPhaseCalibrationDoesNotInventAnUnusedSensor) {
    auto c = calibration();
    c.zero_code[2] = 0;
    c.a_per_code[2] = 0;
    c.direction[2] = 0;
    auto capture = phase_capture();
    capture.measured_mask = 3;
    capture.code[2] = UINT32_MAX;
    nx_phase_current_t out = {};
    ASSERT_EQ(nx_current_calibration_check(&c), NX_SUCCESS);
    ASSERT_EQ(nx_current_convert(&c, &capture, &out), NX_SUCCESS);
    EXPECT_EQ(out.reconstructed_mask, 4);
    capture.measured_mask = 7;
    EXPECT_EQ(nx_current_convert(&c, &capture, &out), NX_ERROR_INVALID);
}
TEST(CurrentControl, BusCalibrationOnlyRequiresTheActualDcSensor) {
    auto c = calibration();
    for (unsigned phase = 1; phase != 3; ++phase) {
        c.zero_code[phase] = 0;
        c.a_per_code[phase] = 0;
        c.direction[phase] = 0;
    }
    const auto capture = bus_capture();
    nx_phase_current_t out = {};
    ASSERT_EQ(nx_current_calibration_check(&c), NX_SUCCESS);
    ASSERT_EQ(nx_current_reconstruct_bus(&c, &capture, &out), NX_SUCCESS);
    EXPECT_EQ(out.measured_mask, 3);
    EXPECT_NEAR(out.phase_a[2], -1.5F, 1e-6F);
    c.zero_code[0] = 0;
    c.a_per_code[0] = 0;
    c.direction[0] = 0;
    EXPECT_EQ(nx_current_calibration_check(&c), NX_ERROR_INVALID);
}
TEST(CurrentControl, GroupBoundNeverInventsSequentialAperturePositions) {
    const auto c = calibration();
    auto capture = phase_capture();
    capture.timing = NX_CURRENT_APERTURES_BOUNDED;
    capture.reference_tick = 400;
    capture.skew_bound_ticks = 8;
    capture.aperture_tick[0] = UINT32_MAX;
    capture.aperture_tick[1] = 0;
    capture.aperture_tick[2] = UINT32_MAX;
    nx_phase_current_t out = {};
    ASSERT_EQ(nx_current_convert(&c, &capture, &out), NX_SUCCESS);
    EXPECT_EQ(out.reference_tick, 400U);
    EXPECT_EQ(out.skew_ticks, 8U);
    EXPECT_EQ(out.timing, NX_CURRENT_APERTURES_BOUNDED);
    capture.skew_bound_ticks = 0;
    EXPECT_EQ(nx_current_convert(&c, &capture, &out), NX_ERROR_STATE);
    capture.skew_bound_ticks = 11;
    EXPECT_EQ(nx_current_convert(&c, &capture, &out), NX_ERROR_STATE);
}
TEST(CurrentControl, DeclaredBoundCoversActualSpanAndIsNotNarrowed) {
    const auto c = calibration();
    auto capture = phase_capture();
    capture.skew_bound_ticks = 3;
    nx_phase_current_t out = {};
    EXPECT_EQ(nx_current_convert(&c, &capture, &out), NX_ERROR_STATE);
    capture.skew_bound_ticks = 8;
    ASSERT_EQ(nx_current_convert(&c, &capture, &out), NX_SUCCESS);
    EXPECT_EQ(out.reference_tick, 100U);
    EXPECT_EQ(out.skew_ticks, 8U);
    EXPECT_EQ(out.timing, NX_CURRENT_APERTURES_EXPLICIT);
}
TEST(CurrentControl, SingleShuntCanCarryAQualifiedGroupTimingBound) {
    const auto c = calibration();
    auto capture = bus_capture();
    capture.timing = NX_CURRENT_APERTURES_BOUNDED;
    capture.reference_tick = 400;
    capture.skew_bound_ticks = 7;
    capture.aperture_tick[0] = UINT32_MAX;
    capture.aperture_tick[1] = 0;
    nx_phase_current_t out = {};
    ASSERT_EQ(nx_current_reconstruct_bus(&c, &capture, &out), NX_SUCCESS);
    EXPECT_EQ(out.reference_tick, 400U);
    EXPECT_EQ(out.skew_ticks, 7U);
    EXPECT_EQ(out.timing, NX_CURRENT_APERTURES_BOUNDED);
}
} /* namespace */
