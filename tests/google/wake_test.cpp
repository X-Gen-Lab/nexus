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
    WakeTarget target;
    nx_irq_wake_t wake = {&target, WakeTarget::notifyCallback, true};
    EXPECT_EQ(nx_irq_wake_validate(&wake, 4U, 4U, 5U), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_irq_wake_validate(&wake, 5U, 4U, 5U), NX_SUCCESS);
    EXPECT_EQ(nx_irq_wake_validate(&wake, 16U, 4U, 5U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_irq_wake_validate(&wake, 5U, 0U, 5U), NX_ERROR_INVALID);
    wake.calls_kernel = false;
    EXPECT_EQ(nx_irq_wake_validate(&wake, 0U, 4U, 5U), NX_SUCCESS);
    EXPECT_CALL(target, notify())
        .Times(1)
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_irq_wake_signal(&wake), NX_SUCCESS);
    EXPECT_EQ(nx_irq_wake_signal(nullptr), NX_SUCCESS);
}
