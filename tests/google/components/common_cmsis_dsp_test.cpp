/**
 * \file            common_cmsis_dsp_test.cpp
 * \brief           Selected real CMSIS-DSP f32 kernels and fixed transform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/cmsis_dsp.h"
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>

namespace {
TEST(CommonCmsisDsp, ActualDotKernelAndFiniteInputAdmission) {
    const float first[] = {1, 2, 3};
    const float second[] = {4, 5, 6};
    float out = 91;
    ASSERT_EQ(nx_cmsis_dot_f32(first, second, 3, &out), NX_SUCCESS);
    EXPECT_FLOAT_EQ(out, 32);
    const float invalid[] = {std::numeric_limits<float>::infinity()};
    EXPECT_EQ(nx_cmsis_dot_f32(first, invalid, 1, &out), NX_ERROR_INVALID);
    EXPECT_FLOAT_EQ(out, 32);
}

TEST(CommonCmsisDsp, FirReversedTapsAndBiquadPositiveFeedbackAreExplicit) {
    const float taps[] = {0.25F, 0.75F};
    float history[3] = {};
    nx_cmsis_fir_t fir = {};
    ASSERT_EQ(nx_cmsis_fir_initialize(&fir, taps, 2, history, 3, 2),
              NX_SUCCESS);
    const float impulse[] = {1, 0};
    float output[2] = {};
    ASSERT_EQ(nx_cmsis_fir_process(&fir, impulse, output, 2), NX_SUCCESS);
    EXPECT_FLOAT_EQ(output[0], 0.75F);
    EXPECT_FLOAT_EQ(output[1], 0.25F);
    const float zero[] = {0, 0};
    ASSERT_EQ(nx_cmsis_fir_process(&fir, zero, output, 2), NX_SUCCESS);
    EXPECT_FLOAT_EQ(output[0], 0);
    EXPECT_FLOAT_EQ(output[1], 0);
    EXPECT_EQ(nx_cmsis_fir_initialize(&fir, taps, 2, history, 2, 2),
              NX_ERROR_INVALID);
    const float coefficients[] = {0.5F, 0, 0, 0.5F, 0};
    float biquad_history[4] = {};
    nx_cmsis_biquad_t biquad = {};
    ASSERT_EQ(
        nx_cmsis_biquad_initialize(&biquad, coefficients, 1, biquad_history, 4),
        NX_SUCCESS);
    ASSERT_EQ(nx_cmsis_biquad_process(&biquad, impulse, output, 2), NX_SUCCESS);
    EXPECT_FLOAT_EQ(output[0], 0.5F);
    EXPECT_FLOAT_EQ(output[1], 0.25F);
}

TEST(CommonCmsisDsp, RealFft1024PackingAndAmplitudeUseActualUpstreamTransform) {
    std::array<float, NX_CMSIS_RFFT_LENGTH> input = {}, workspace = {},
                                            spectrum = {};
    input.fill(1);
    ASSERT_EQ(
        nx_cmsis_rfft1024_f32(input.data(), workspace.data(), spectrum.data()),
        NX_SUCCESS);
    EXPECT_NEAR(spectrum[0], 1024, 0.001F);
    for (size_t index = 1; index < spectrum.size(); ++index) {
        EXPECT_NEAR(spectrum[index], 0, 0.001F);
    }
    EXPECT_FLOAT_EQ(input[19], 1);
    for (size_t index = 0; index < input.size(); ++index) {
        input[index] = (index & 1U) == 0 ? 1.0F : -1.0F;
    }
    ASSERT_EQ(
        nx_cmsis_rfft1024_f32(input.data(), workspace.data(), spectrum.data()),
        NX_SUCCESS);
    EXPECT_NEAR(spectrum[0], 0, 0.001F);
    EXPECT_NEAR(spectrum[1], 1024, 0.001F);
    input[7] = std::numeric_limits<float>::quiet_NaN();
    spectrum[0] = 91;
    EXPECT_EQ(
        nx_cmsis_rfft1024_f32(input.data(), workspace.data(), spectrum.data()),
        NX_ERROR_INVALID);
    EXPECT_FLOAT_EQ(spectrum[0], 91);
}
} /* namespace */
