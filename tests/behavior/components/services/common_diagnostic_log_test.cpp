/**
 * \file            common_diagnostic_log_test.cpp
 * \brief           Ordinary ring events survive downstream backpressure
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/diagnostic_log.h"
#include <array>
#include <gtest/gtest.h>
#include <string>

namespace {
struct EventSink {
    bool busy = true;
    std::string last;
    static nx_result_t write(void* context, nx_log_level_t, const void* bytes,
                             size_t count, nx_time_us_t) {
        auto& self = *static_cast<EventSink*>(context);
        if (self.busy)
            return NX_ERROR_BUSY;
        self.last.assign(static_cast<const char*>(bytes), count);
        return NX_SUCCESS;
    }
};

TEST(CommonDiagnosticLog, OnePendingEventSurvivesBusyAndNoImplicitDrainLoop) {
    nx_diagnostic_ring_t ring{};
    std::array<nx_diagnostic_event_t, 2> events{};
    ASSERT_EQ(nx_diagnostic_ring_init(&ring, events.data(), events.size()),
              NX_SUCCESS);
    nx_diagnostic_event_t first{100, 0x123, 7, 2};
    nx_diagnostic_event_t second{101, 0x456, 8, 3};
    ASSERT_EQ(nx_diagnostic_ring_write(&ring, &first), NX_SUCCESS);
    ASSERT_EQ(nx_diagnostic_ring_write(&ring, &second), NX_SUCCESS);
    EventSink output;
    nx_log_t logger{};
    ASSERT_EQ(nx_log_init(&logger, {&output, EventSink::write, nullptr},
                          NX_LOG_DEBUG),
              NX_SUCCESS);
    nx_diagnostic_log_t consumer{};
    EXPECT_EQ(nx_diagnostic_log_drain(&consumer, &ring, &logger, 200),
              NX_ERROR_BUSY);
    EXPECT_TRUE(consumer.pending);
    EXPECT_EQ(consumer.event.identity, first.identity);
    EXPECT_EQ(ring.count, 1U);
    output.busy = false;
    ASSERT_EQ(nx_diagnostic_log_drain(&consumer, &ring, &logger, 200),
              NX_SUCCESS);
    EXPECT_FALSE(consumer.pending);
    EXPECT_EQ(ring.count, 1U);
    EXPECT_EQ(consumer.delivered, 1U);
    EXPECT_NE(output.last.find("event=2"), std::string::npos);
    ASSERT_EQ(nx_diagnostic_log_drain(&consumer, &ring, &logger, 200),
              NX_SUCCESS);
    EXPECT_EQ(consumer.delivered, 2U);
    EXPECT_EQ(nx_diagnostic_log_drain(&consumer, &ring, &logger, 200),
              NX_ERROR_EMPTY);
}
} /* namespace */
