/**
 * \file            stack_observation_test.cpp
 * \brief           Caller-provenance bounded paint-prefix observations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "nexus/core/stack_observation.h"
#include <cstring>
#include <gtest/gtest.h>

TEST(StackObservation, UntouchedFullCapacityTerminatesAtTheDeclaredBoundary) {
    unsigned char bytes[16];
    std::memset(bytes, 0xA5, sizeof bytes);
    nx_stack_observation_t out{};
    ASSERT_EQ(nx_stack_observe(bytes, sizeof bytes, 0xA5, &out), NX_SUCCESS);
    EXPECT_EQ(out.capacity_bytes, sizeof bytes);
    EXPECT_EQ(out.untouched_bytes, sizeof bytes);
    EXPECT_EQ(out.touched_bytes, 0U);
    EXPECT_EQ(bytes[15], 0xA5);
}

TEST(StackObservation, PrefixStopsAtTheLowestWrittenByte) {
    for (size_t touched = 0U; touched < 16U; ++touched) {
        unsigned char bytes[16];
        std::memset(bytes, 0x5C, sizeof bytes);
        bytes[touched] = 0U;
        nx_stack_observation_t out{};
        ASSERT_EQ(nx_stack_observe(bytes, sizeof bytes, 0x5C, &out),
                  NX_SUCCESS);
        EXPECT_EQ(out.untouched_bytes, touched);
        EXPECT_EQ(out.touched_bytes, sizeof bytes - touched);
        EXPECT_EQ(bytes[touched], 0U);
    }
}

TEST(StackObservation, InvalidSpanAndAliasPreserveOutputAndStorage) {
    unsigned char bytes[32]{};
    nx_stack_observation_t out{77U, 78U, 79U};
    EXPECT_EQ(nx_stack_observe(nullptr, sizeof bytes, 0U, &out),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_stack_observe(bytes, 0U, 0U, &out), NX_ERROR_INVALID);
    EXPECT_EQ(nx_stack_observe(bytes, sizeof bytes, 0U, nullptr),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_stack_observe(reinterpret_cast<void*>(UINTPTR_MAX - 1U), 3U,
                               0U, &out),
              NX_ERROR_INVALID);
    EXPECT_EQ(out.capacity_bytes, 77U);
    EXPECT_EQ(
        nx_stack_observe(bytes, sizeof bytes, 0U,
                         reinterpret_cast<nx_stack_observation_t*>(bytes)),
        NX_ERROR_INVALID);
    EXPECT_EQ(bytes[0], 0U);
}
