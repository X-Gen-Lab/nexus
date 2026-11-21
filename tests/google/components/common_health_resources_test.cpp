/**
 * \file            common_health_resources_test.cpp
 * \brief           Resource snapshots preserve paint provenance and unknowns
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/health_resources.h"
#include <array>
#include <gtest/gtest.h>

namespace {
TEST(CommonHealthResources, MissingResourceGroupsRemainUnknown) {
    nx_health_resources_t output{};
    ASSERT_EQ(nx_health_stack_resources(nullptr, 0, 0xa5, nullptr, &output),
              NX_SUCCESS);
    EXPECT_EQ(output.known, 0U);
}

TEST(CommonHealthResources, ActualBoundedStackAndRingMechanismsSupplyFacts) {
    std::array<uint8_t, 32> quiescent{};
    quiescent.fill(0xa5);
    quiescent[24] = 0;
    nx_diagnostic_ring_t ring{};
    std::array<nx_diagnostic_event_t, 1> entries{};
    ASSERT_EQ(nx_diagnostic_ring_init(&ring, entries.data(), entries.size()),
              NX_SUCCESS);
    nx_diagnostic_event_t event{};
    ASSERT_EQ(nx_diagnostic_ring_write(&ring, &event), NX_SUCCESS);
    ASSERT_EQ(nx_diagnostic_ring_write(&ring, &event), NX_ERROR_BUSY);
    nx_health_resources_t output{};
    ASSERT_EQ(nx_health_stack_resources(quiescent.data(), quiescent.size(),
                                        0xa5, &ring, &output),
              NX_SUCCESS);
    EXPECT_EQ(output.known, NX_HEALTH_STACK_KNOWN | NX_HEALTH_RING_KNOWN);
    EXPECT_EQ(output.ring_dropped, 1U);
    EXPECT_EQ(output.stack.capacity_bytes, 32U);
    EXPECT_EQ(output.stack.untouched_bytes, 24U);
    EXPECT_EQ(output.stack.touched_bytes, 8U);
}
} /* namespace */
