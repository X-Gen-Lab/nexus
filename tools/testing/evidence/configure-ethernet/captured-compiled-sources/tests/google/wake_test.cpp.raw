/**
 * \file            wake_test.cpp
 *
 * \brief           IRQ wake callback ceiling and no hidden task requirements
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/wake.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

class WakeTarget {
  public:
    MOCK_METHOD(nx_result_t, notify, ());
    static nx_result_t notifyCallback(void* context) {
        return static_cast<WakeTarget*>(context)->notify();
    }
};

TEST(IrqWake, RejectsKernelUnsafePrioritiesBeforeAttachingSink) {
    testing::StrictMock<WakeTarget> target;
    nx_irq_wake_t wake = {&target, WakeTarget::notifyCallback, true};
    nx_irq_policy_t policy = {NX_IRQ_KERNEL_BASEPRI, 82U, 4U, 5U};
    nx_irq_source_t source = {37, 4U, 0U};
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source),
              NX_ERROR_PERMISSION);
    source.priority = 5U;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_SUCCESS);
    source.priority = 16U;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_ERROR_INVALID);
    source.priority = 5U;
    policy.priority_bits = 0U;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_ERROR_INVALID);
    policy.priority_bits = 4U;
    wake.calls_kernel = false;
    source.priority = 0U;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_SUCCESS);
    EXPECT_CALL(target, notify())
        .Times(1)
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_irq_wake_signal(&wake), NX_SUCCESS);
    EXPECT_EQ(nx_irq_wake_signal(nullptr), NX_SUCCESS);
}

TEST(IrqWake, BaselineKernelAcceptsEveryValidExternalPriority) {
    testing::StrictMock<WakeTarget> target;
    const nx_irq_wake_t wake = {&target, WakeTarget::notifyCallback, true};
    const nx_irq_policy_t policy = {NX_IRQ_KERNEL_PRIMASK, 32U, 2U, 0U};
    for (uint8_t priority = 0U; priority < 4U; ++priority) {
        const nx_irq_source_t source = {31, priority, 0U};
        EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_SUCCESS);
    }
}

TEST(IrqWake, KernelPolicyCannotBeInferredFromZeroCeiling) {
    testing::StrictMock<WakeTarget> target;
    const nx_irq_wake_t wake = {&target, WakeTarget::notifyCallback, true};
    nx_irq_policy_t policy = {NX_IRQ_KERNEL_NONE, 32U, 2U, 0U};
    const nx_irq_source_t source = {0, 0U, 0U};
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source),
              NX_ERROR_PERMISSION);
    policy.kernel = NX_IRQ_KERNEL_BASEPRI;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_ERROR_INVALID);
}

TEST(IrqWake, RejectsNonExternalAndOutOfRangePublishers) {
    testing::StrictMock<WakeTarget> target;
    const nx_irq_wake_t wake = {&target, WakeTarget::notifyCallback, false};
    nx_irq_policy_t policy = {NX_IRQ_KERNEL_PRIMASK, 240U, 2U, 0U};
    nx_irq_source_t source = {-1, 0U, 0U};
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_ERROR_INVALID);
    source.irq_number = 240;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_ERROR_INVALID);
    source.irq_number = 239;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_SUCCESS);
    policy.external_irq_count = 0U;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_ERROR_INVALID);
    policy.external_irq_count = 481U;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_ERROR_INVALID);
}

TEST(IrqWake, BasepriRequiresPreemptionOnlyPriorityGrouping) {
    testing::StrictMock<WakeTarget> target;
    const nx_irq_wake_t wake = {&target, WakeTarget::notifyCallback, true};
    const nx_irq_policy_t policy = {NX_IRQ_KERNEL_BASEPRI, 82U, 4U, 5U};
    nx_irq_source_t source = {37, 5U, 3U};
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_SUCCESS);
    source.priority_group = 4U;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source),
              NX_ERROR_PERMISSION);
    source.priority_group = 8U;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_ERROR_INVALID);
}

TEST(IrqWake, EightBitPriorityAndV8ExternalBoundRemainExplicit) {
    testing::StrictMock<WakeTarget> target;
    const nx_irq_wake_t wake = {&target, WakeTarget::notifyCallback, true};
    const nx_irq_policy_t policy = {NX_IRQ_KERNEL_BASEPRI, 480U, 8U, 10U};
    nx_irq_source_t source = {479, 255U, 0U};
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_SUCCESS);
    source.priority_group = 1U;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source),
              NX_ERROR_PERMISSION);
    source.priority_group = 0U;
    source.priority = 9U;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source),
              NX_ERROR_PERMISSION);
    source.irq_number = 480;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_ERROR_INVALID);
}

TEST(IrqWake, RejectsMalformedPolicyBeforeAnyCallback) {
    testing::StrictMock<WakeTarget> target;
    nx_irq_wake_t wake = {&target, WakeTarget::notifyCallback, true};
    nx_irq_policy_t policy = {NX_IRQ_KERNEL_PRIMASK, 240U, 2U, 0U};
    nx_irq_source_t source = {1, 1U, 0U};
    EXPECT_EQ(nx_irq_wake_validate(&wake, nullptr, &source), NX_ERROR_INVALID);
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, nullptr), NX_ERROR_INVALID);
    policy.kernel = static_cast<nx_irq_kernel_policy_t>(3);
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_ERROR_INVALID);
    policy.kernel = NX_IRQ_KERNEL_PRIMASK;
    policy.syscall_ceiling = 1U;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_ERROR_INVALID);
    policy.syscall_ceiling = 0U;
    source.priority_group = 1U;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_ERROR_INVALID);
    source.priority_group = 0U;
    wake.notify = nullptr;
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_ERROR_INVALID);
    EXPECT_EQ(nx_irq_wake_signal(&wake), NX_ERROR_INVALID);
}

TEST(IrqWake, NonkernelTargetsAndDetachRemainLegalWithNoKernel) {
    testing::StrictMock<WakeTarget> target;
    const nx_irq_wake_t wake = {&target, WakeTarget::notifyCallback, false};
    const nx_irq_policy_t policy = {NX_IRQ_KERNEL_NONE, 82U, 4U, 0U};
    const nx_irq_source_t source = {0, 0U, 7U};
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_SUCCESS);
    EXPECT_EQ(nx_irq_wake_validate(nullptr, &policy, &source), NX_SUCCESS);
}

TEST(IrqWake, NonkernelMainlineTargetMayUseSubpriorityGrouping) {
    testing::StrictMock<WakeTarget> target;
    const nx_irq_wake_t wake = {&target, WakeTarget::notifyCallback, false};
    const nx_irq_policy_t policy = {NX_IRQ_KERNEL_BASEPRI, 82U, 4U, 5U};
    const nx_irq_source_t source = {37, 0U, 7U};
    EXPECT_EQ(nx_irq_wake_validate(&wake, &policy, &source), NX_SUCCESS);
    EXPECT_EQ(nx_irq_wake_validate(nullptr, &policy, &source), NX_SUCCESS);
}
