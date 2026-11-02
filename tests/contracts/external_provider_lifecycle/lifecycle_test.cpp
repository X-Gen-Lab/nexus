/**
 * \file            lifecycle_test.cpp
 * \brief           External provider C contract through the generated platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/native/model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "nexus_reference/lifecycle_probe.h"
#include <gtest/gtest.h>

namespace {
class ExternalLifecycle : public testing::Test {
  protected:
    void SetUp() override {
        ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
        nx_reference_reset();
    }
    void TearDown() override {
        nx_reference_fail_prepare(0U);
        EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    }
};

TEST_F(ExternalLifecycle, LookupIsPureAndSharesOnlyTheReadonlyTable) {
    const nx_gpio_port_t* alpha = nx_factory_gpio(NX_GPIO_ID_ALPHA);
    const nx_gpio_port_t* beta = nx_factory_gpio(NX_GPIO_ID_BETA);
    ASSERT_NE(alpha, nullptr);
    ASSERT_NE(beta, nullptr);
    EXPECT_NE(alpha, beta);
    EXPECT_NE(alpha->context, beta->context);
    EXPECT_EQ(alpha->ops, beta->ops);
    EXPECT_EQ(nx_factory_gpio(NX_GPIO_ID_ALPHA), alpha);
    EXPECT_EQ(nx_factory_gpio(NX_GPIO_ID_COUNT), nullptr);
    EXPECT_EQ(nx_reference_event_count(), 0U);
    EXPECT_EQ(nx_gpio_port_write(alpha, 1U, 0U), NX_ERROR_INVALID);
}

TEST_F(ExternalLifecycle, InstancesOperateIndependentlyAndKeepStaticIdentity) {
    const nx_gpio_port_t* alpha = nx_factory_gpio(NX_GPIO_ID_ALPHA);
    const nx_gpio_port_t* beta = nx_factory_gpio(NX_GPIO_ID_BETA);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_EQ(nx_gpio_port_write(alpha, 1U, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_native_gpio_model_output(alpha), 1U);
    EXPECT_EQ(nx_native_gpio_model_output(beta), 0U);
    ASSERT_EQ(nx_gpio_port_write(beta, 8U, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_gpio_port_toggle(alpha, 1U), NX_SUCCESS);
    EXPECT_EQ(nx_native_gpio_model_output(alpha), 0U);
    EXPECT_EQ(nx_native_gpio_model_output(beta), 8U);
    EXPECT_EQ(nx_gpio_port_write(beta, 1U, 0U), NX_ERROR_PERMISSION);
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_factory_gpio(NX_GPIO_ID_ALPHA), alpha);
    EXPECT_EQ(nx_factory_gpio(NX_GPIO_ID_BETA), beta);
    EXPECT_EQ(nx_gpio_port_write(alpha, 1U, 0U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_gpio_port_write(beta, 8U, 0U), NX_ERROR_INVALID);
}

TEST_F(ExternalLifecycle, SecondPrepareFailureRollsFirstBackAndRecovers) {
    const nx_gpio_port_t* alpha = nx_factory_gpio(NX_GPIO_ID_ALPHA);
    const nx_gpio_port_t* beta = nx_factory_gpio(NX_GPIO_ID_BETA);
    nx_reference_fail_prepare(8U);
    nx_platform_diagnostic_t diagnostic = {};
    const nx_platform_start_result_t result =
        nx_platform_start_diagnosed(&diagnostic);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(diagnostic.phase, NX_PLATFORM_PHASE_CONTROLLER);
    EXPECT_EQ(diagnostic.controller_index,
              static_cast<size_t>(NX_PLATFORM_CONTROLLER_INDEX_GPIO_BETA));
    ASSERT_EQ(nx_reference_event_count(), 3U);
    EXPECT_EQ(nx_reference_event(0U).kind, NX_REFERENCE_PREPARE);
    EXPECT_EQ(nx_reference_event(0U).mask, 1U);
    EXPECT_EQ(nx_reference_event(1U).kind, NX_REFERENCE_PREPARE);
    EXPECT_EQ(nx_reference_event(1U).mask, 8U);
    EXPECT_EQ(nx_reference_event(2U).kind, NX_REFERENCE_STOP);
    EXPECT_EQ(nx_reference_event(2U).mask, 1U);
    EXPECT_EQ(nx_gpio_port_write(alpha, 1U, 0U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_gpio_port_write(beta, 8U, 0U), NX_ERROR_INVALID);
    nx_reference_fail_prepare(0U);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_EQ(nx_factory_gpio(NX_GPIO_ID_ALPHA), alpha);
    EXPECT_EQ(nx_factory_gpio(NX_GPIO_ID_BETA), beta);
    EXPECT_EQ(nx_gpio_port_write(alpha, 1U, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_gpio_port_write(beta, 8U, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}

TEST_F(ExternalLifecycle, FailureAndRecoveryRepeatWithoutAStaleInstance) {
    const nx_gpio_port_t* alpha = nx_factory_gpio(NX_GPIO_ID_ALPHA);
    const nx_gpio_port_t* beta = nx_factory_gpio(NX_GPIO_ID_BETA);
    for (unsigned cycle = 0U; cycle < 8U; ++cycle) {
        nx_reference_reset();
        nx_reference_fail_prepare(8U);
        const nx_platform_start_result_t failed = nx_platform_start();
        ASSERT_EQ(failed.primary, NX_ERROR_IO);
        ASSERT_EQ(failed.cleanup, NX_SUCCESS);
        ASSERT_EQ(failed.remaining_effects, 0U);
        ASSERT_EQ(nx_reference_event_count(), 3U);
        nx_reference_fail_prepare(0U);
        ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
        EXPECT_EQ(nx_factory_gpio(NX_GPIO_ID_ALPHA), alpha);
        EXPECT_EQ(nx_factory_gpio(NX_GPIO_ID_BETA), beta);
        ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
        EXPECT_EQ(nx_reference_event_count(), 7U);
    }
}
} /* namespace */
