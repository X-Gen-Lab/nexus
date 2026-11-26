/**
 * \file            common_fault_m4_test.cpp
 * \brief           M4F MSP/PSP bounds and extended-frame copy contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/fault_m4.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>

namespace {
TEST(CommonFaultM4, UsesExplicitPspBindingAndPreservesExtendedFrameFacts) {
    alignas(8) std::array<uint32_t, 32> task{};
    alignas(8) std::array<uint32_t, 32> main{};
    task[4] = 0x1234;
    task[10] = 0x08001234;
    task[11] = (1U << 24U) | (1U << 9U);
    for (size_t i = 12; i < 30; ++i)
        task[i] = 0xee000000U + static_cast<uint32_t>(i);
    nx_fault_stack_binding_t main_binding{main.data(), main.size()};
    nx_fault_stack_binding_t task_binding{task.data(), task.size()};
    nx_fault_capture_t facts{};
    facts.sequence = 7;
    ASSERT_EQ(nx_fault_m4_frame_copy(
                  &facts, 0xffffffedU, reinterpret_cast<uintptr_t>(main.data()),
                  reinterpret_cast<uintptr_t>(task.data() + 4), &main_binding,
                  &task_binding),
              NX_SUCCESS);
    EXPECT_EQ(facts.sequence, 7U);
    EXPECT_EQ(facts.core[0], 0x1234U);
    EXPECT_EQ(facts.core[6], 0x08001234U);
    EXPECT_EQ(facts.flags & (NX_FAULT_CORE_VALID | NX_FAULT_STACK_VALID |
                             NX_FAULT_FRAME_EXTENDED),
              NX_FAULT_CORE_VALID | NX_FAULT_STACK_VALID |
                  NX_FAULT_FRAME_EXTENDED);
}

TEST(CommonFaultM4, BadOrMissingStackBoundsKeepRawSpWithoutReadingFrame) {
    alignas(8) std::array<uint32_t, 8> main{};
    nx_fault_stack_binding_t binding{main.data(), main.size()};
    nx_fault_capture_t facts{};
    facts.flags = NX_FAULT_CORE_VALID | NX_FAULT_STATUS_VALID;
    facts.core[0] = 99;
    ASSERT_EQ(nx_fault_m4_frame_copy(&facts, 0xfffffffdU,
                                     reinterpret_cast<uintptr_t>(main.data()),
                                     0x1234, &binding, nullptr),
              NX_SUCCESS);
    EXPECT_EQ(facts.psp, 0x1234U);
    EXPECT_EQ(facts.flags, NX_FAULT_STATUS_VALID);
    EXPECT_EQ(facts.core[0], 0U);
    const auto before = facts;
    EXPECT_EQ(nx_fault_m4_frame_copy(&facts, 0, 0, 0, &binding, nullptr),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&before, &facts, sizeof(facts)), 0);
}

TEST(CommonFaultM4, BasicMspFrameRequiresSpaceForAlignmentPadding) {
    alignas(8) std::array<uint32_t, 8> frame{};
    frame[7] = 1U << 9U;
    nx_fault_stack_binding_t binding{frame.data(), frame.size()};
    nx_fault_capture_t facts{};
    ASSERT_EQ(nx_fault_m4_frame_copy(&facts, 0xfffffff9U,
                                     reinterpret_cast<uintptr_t>(frame.data()),
                                     0, &binding, nullptr),
              NX_SUCCESS);
    EXPECT_EQ(facts.flags & NX_FAULT_CORE_VALID, 0U);
    frame[7] = 1U << 24U;
    ASSERT_EQ(nx_fault_m4_frame_copy(&facts, 0xfffffff9U,
                                     reinterpret_cast<uintptr_t>(frame.data()),
                                     0, &binding, nullptr),
              NX_SUCCESS);
    EXPECT_NE(facts.flags & NX_FAULT_CORE_VALID, 0U);
}

TEST(CommonFaultM4, EveryStackingOrLazyStateErrorGatesOriginalStackReads) {
    const auto* original =
        reinterpret_cast<const uint32_t*>(uintptr_t{0x20000000});
    nx_fault_stack_binding_t binding{original, 64};
    for (uint32_t error : {0x8U, 0x10U, 0x20U, 0x800U, 0x1000U, 0x2000U}) {
        SCOPED_TRACE(error);
        nx_fault_capture_t facts{};
        facts.flags = NX_FAULT_STATUS_VALID | NX_FAULT_CORE_VALID;
        facts.status[0] = error;
        facts.core[0] = 0xabcdef;
        ASSERT_EQ(nx_fault_m4_frame_copy(&facts, 0xfffffff9U, 0x20000020, 0,
                                         &binding, nullptr),
                  NX_SUCCESS);
        EXPECT_EQ(facts.flags, NX_FAULT_STATUS_VALID);
        EXPECT_EQ(facts.core[0], 0U);
        EXPECT_EQ(facts.status[0], error);
        EXPECT_EQ(facts.msp, 0x20000020U);
    }
}
} /* namespace */
