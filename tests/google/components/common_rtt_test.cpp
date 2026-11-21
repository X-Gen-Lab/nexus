/**
 * \file            common_rtt_test.cpp
 * \brief           Actual upstream RTT keeps whole records and prior masks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/rtt.h"
extern "C" {
#include "SEGGER_RTT.h"
}
#include <array>
#include <cstring>
#include <gtest/gtest.h>

namespace {
TEST(CommonRtt, UpstreamSkipNeverPublishesPartialRecordsAndRestoresMasks) {
    nx_rtt_t rtt{};
    static std::array<char, 12> up{};
    static std::array<char, 8> down{};
    ASSERT_EQ(nx_rtt_init(&rtt, 1, "diagnostics", up.data(), up.size(),
                          down.data(), down.size()),
              NX_SUCCESS);
    const auto sink = nx_rtt_log_sink(&rtt);
    const auto previous = nx_arch_irq_save();
    EXPECT_EQ(sink.write(sink.context, NX_LOG_INFO, "hello", 5, 100),
              NX_SUCCESS);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(previous);
    EXPECT_EQ(sink.write(sink.context, NX_LOG_INFO, "overflow", 8, 100),
              NX_ERROR_OVERFLOW);
    EXPECT_EQ(rtt.accepted, 1U);
    EXPECT_EQ(rtt.dropped, 1U);
    std::array<char, 16> host{};
    EXPECT_EQ(SEGGER_RTT_ReadUpBuffer(1, host.data(), host.size()), 5U);
    EXPECT_EQ(std::memcmp(host.data(), "hello", 5), 0);
    EXPECT_EQ(sink.flush(sink.context, 100), NX_SUCCESS);
    EXPECT_EQ(nx_rtt_stop(&rtt), NX_SUCCESS);
}

TEST(CommonRtt, HostDownlinkUsesActualRingAndStopRetainsUnconsumedUplink) {
    nx_rtt_t rtt{};
    static std::array<char, 16> up{};
    static std::array<char, 8> down{};
    ASSERT_EQ(nx_rtt_init(&rtt, 2, "shell", up.data(), up.size(), down.data(),
                          down.size()),
              NX_SUCCESS);
    ASSERT_EQ(SEGGER_RTT_WriteDownBuffer(2, "ok", 2), 2U);
    std::array<char, 4> received{};
    size_t count = 99;
    EXPECT_EQ(nx_rtt_read(&rtt, received.data(), received.size(), &count),
              NX_SUCCESS);
    EXPECT_EQ(count, 2U);
    EXPECT_EQ(std::memcmp(received.data(), "ok", 2), 0);
    EXPECT_EQ(nx_rtt_read(&rtt, received.data(), received.size(), &count),
              NX_ERROR_EMPTY);
    EXPECT_EQ(count, 0U);
    auto sink = nx_rtt_log_sink(&rtt);
    ASSERT_EQ(sink.write(sink.context, NX_LOG_INFO, "pending", 7, 100),
              NX_SUCCESS);
    EXPECT_EQ(nx_rtt_stop(&rtt), NX_ERROR_BUSY);
    EXPECT_EQ(sink.write(sink.context, NX_LOG_INFO, "x", 1, 100),
              NX_ERROR_STATE);
    EXPECT_EQ(SEGGER_RTT_ReadUpBuffer(2, received.data(), received.size()), 4U);
    EXPECT_EQ(SEGGER_RTT_ReadUpBuffer(2, received.data(), received.size()), 3U);
    EXPECT_EQ(nx_rtt_stop(&rtt), NX_SUCCESS);
}
} /* namespace */
