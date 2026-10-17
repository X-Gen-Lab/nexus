/**
 * \file            cycle_probe_native_test.cpp
 * \brief           Native cycle observation never invents MCU cycles.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/cycle_probe.h"
#include <gtest/gtest.h>

TEST(CycleProbeNative, RealNativeCounterRemainsUnavailable) {
    nx_io_cycle_probe_t probe{};
    nx_io_cycle_token_t token{};
    nx_io_cycle_observation_t out{};
    ASSERT_EQ(nx_io_cycle_probe_begin(&probe, &token), NX_SUCCESS);
    ASSERT_EQ(nx_io_cycle_probe_end(&probe, &token), NX_SUCCESS);
    ASSERT_EQ(nx_io_cycle_probe_snapshot(&probe, &out), NX_SUCCESS);
    EXPECT_EQ(out.attempts, 1U);
    EXPECT_EQ(out.unavailable_pairs, 1U);
    EXPECT_EQ(out.raw_pairs, 0U);
    EXPECT_FALSE(out.last_available);
    EXPECT_FALSE(out.elapsed_known);
}
