/**
 * \file            threadx_irq_test.cpp
 * \brief           ThreadX ISR admission over mocked Arch and IRQ registers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/threadx.h"
#include "private/irq_model.h"
#include <array>
#include <gtest/gtest.h>
namespace {
bool privileged, masked;
uint32_t exception, grouping;
std::array<uint8_t, 82> priorities;
class ThreadXIRQ : public ::testing::Test {
    void SetUp() override {
        privileged = true;
        masked = false;
        exception = 16;
        grouping = 3U << 8;
        priorities.fill(80);
    }
};
} /* namespace */
extern "C" bool __wrap_nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool __wrap_nx_arch_irq_is_masked(void) {
    return masked;
}
extern "C" uint32_t __wrap_nx_arch_exception_number(void) {
    return exception;
}
extern "C" volatile void* nx_threadx_irq_model_register(uintptr_t address) {
    if (address == 0xe000ed0cU)
        return &grouping;
    if (address == 0xe000e400U)
        return priorities.data();
    std::abort();
}
TEST_F(ThreadXIRQ, ExactCeilingAndLowestReviewedIRQAreAdmitted) {
    EXPECT_TRUE(nx_threadx_isr_allowed());
    exception = 97;
    priorities[81] = 240;
    EXPECT_TRUE(nx_threadx_isr_allowed());
}
TEST_F(ThreadXIRQ, AboveCeilingIsRejected) {
    priorities[0] = 64;
    EXPECT_FALSE(nx_threadx_isr_allowed());
}
TEST_F(ThreadXIRQ, SystemExceptionsAndOutOfRangeIRQsAreRejected) {
    for (auto invalid : {0U, 11U, 15U, 98U, 1000U}) {
        exception = invalid;
        EXPECT_FALSE(nx_threadx_isr_allowed());
    }
}
TEST_F(ThreadXIRQ, PriorityGroupingCannotWeakenTheCeiling) {
    grouping = 4U << 8;
    EXPECT_FALSE(nx_threadx_isr_allowed());
    grouping = 0;
    EXPECT_TRUE(nx_threadx_isr_allowed());
}
TEST_F(ThreadXIRQ, UnprivilegedOrMaskedIRQIsRejected) {
    privileged = false;
    EXPECT_FALSE(nx_threadx_isr_allowed());
    privileged = true;
    masked = true;
    EXPECT_FALSE(nx_threadx_isr_allowed());
}
