/**
 * \file            pwm_adc_loop_test.cpp
 * \brief           IRQ-session admission keeps rejected caller storage intact
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/pwm_adc_loop.h"
#include <gtest/gtest.h>

namespace {
nx_result_t compute(void*, const uint16_t*, size_t, uint64_t, uint32_t*,
                    size_t) {
    return NX_SUCCESS;
}

TEST(PwmAdcLoopApi, RejectedPreparationCannotReplaceAnExistingOrBorrowedSpan) {
    nx_pwm_adc_loop_session_t session{};
    nx_request_initialize(&session.base);
    uint16_t samples[1]{};
    uint32_t duties[3]{};
    int context = 0;
    ASSERT_EQ(nx_pwm_adc_loop_prepare(&session, samples, 1U, duties, 3U,
                                      compute, &context, 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_pwm_adc_loop_prepare(&session,
                                      reinterpret_cast<uint16_t*>(&session), 1U,
                                      duties, 3U, compute, &context, 2000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_pwm_adc_loop_prepare(
                  &session, reinterpret_cast<uint16_t*>(UINTPTR_MAX - 1U), 1U,
                  duties, 3U, compute, &context, 2000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_pwm_adc_loop_prepare(&session, samples,
                                      SIZE_MAX / sizeof(uint16_t) + 1U, duties,
                                      3U, compute, &context, 2000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_pwm_adc_loop_prepare(&session, samples, 1U, duties, 3U,
                                      compute, &session, 2000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_pwm_adc_loop_prepare(&session, samples, 1U, duties, 3U,
                                      compute, samples, 2000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_pwm_adc_loop_prepare(&session, samples, 1U, duties, 3U,
                                      compute, duties, 2000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_pwm_adc_loop_prepare(&session, samples, 1U, duties, 3U, compute,
                                reinterpret_cast<void*>(UINTPTR_MAX), 2000U),
        NX_ERROR_INVALID);
    EXPECT_EQ(session.samples, samples);
    EXPECT_EQ(session.duty_ticks, duties);
    EXPECT_EQ(session.base.deadline, 1000U);
    ASSERT_EQ(nx_request_admit(&session.base, NX_REQUEST_ACTIVE), NX_SUCCESS);
    EXPECT_EQ(nx_pwm_adc_loop_prepare(&session, samples, 1U, duties, 3U,
                                      compute, &context, 2000U),
              NX_ERROR_STATE);
    EXPECT_EQ(session.base.deadline, 1000U);
}

TEST(PwmAdcLoopApi, AProviderRejectionNeverBecomesAWrapperOwnedBorrow) {
    nx_pwm_adc_loop_session_t session{};
    nx_request_initialize(&session.base);
    uint16_t samples[1]{};
    uint32_t duties[3]{};
    int calls = 0;
    const nx_pwm_adc_loop_ops_t operations{
        [](void* context, nx_pwm_adc_loop_session_t*) {
            ++*static_cast<int*>(context);
            return NX_ERROR_BUSY;
        },
        nullptr, nullptr, nullptr};
    const nx_pwm_adc_loop_t port{&operations, &calls};
    ASSERT_EQ(nx_pwm_adc_loop_prepare(&session, samples, 1U, duties, 3U,
                                      compute, &calls, 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_pwm_adc_loop_start(&port, &session), NX_ERROR_BUSY);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_READY);
    EXPECT_EQ(nx_pwm_adc_loop_service(&port), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(calls, 1);
}
} /* namespace */
