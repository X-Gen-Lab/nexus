/**
 * \file            common_runtime_test.cpp
 * \brief           Real explicit-time helpers and existing stream fanout
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/event_fanout.h"
#include "nexus/components/periodic.h"
#include "nexus/components/timers.h"
#include "nexus/io/stream.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
TEST(CommonPeriodic, FixedPhaseSkipReportsLatestReleaseWithoutDrift) {
    nx_periodic_t periodic = {};
    ASSERT_EQ(nx_periodic_reset(&periodic, 100, 10, NX_PERIODIC_SKIP),
              NX_SUCCESS);
    nx_periodic_release_t release = {};
    release.releases = 91;
    EXPECT_EQ(nx_periodic_poll(&periodic, 109, 1, &release), NX_ERROR_EMPTY);
    EXPECT_EQ(release.releases, 91U);
    ASSERT_EQ(nx_periodic_poll(&periodic, 135, 1, &release), NX_SUCCESS);
    EXPECT_EQ(release.first_due, 130U);
    EXPECT_EQ(release.releases, 1U);
    EXPECT_EQ(release.skipped, 2U);
    EXPECT_EQ(periodic.next_due, 140U);
    ASSERT_EQ(nx_periodic_poll(&periodic, 140, 1, &release), NX_SUCCESS);
    EXPECT_EQ(release.first_due, 140U);
    EXPECT_EQ(release.skipped, 0U);
}

TEST(CommonPeriodic, BoundedCatchupRetainsBacklogAndDomainResetIsExplicit) {
    nx_periodic_t periodic = {};
    ASSERT_EQ(nx_periodic_reset(&periodic, 0, 10, NX_PERIODIC_CATCH_UP),
              NX_SUCCESS);
    nx_periodic_release_t release = {};
    ASSERT_EQ(nx_periodic_poll(&periodic, 55, 2, &release), NX_SUCCESS);
    EXPECT_EQ(release.first_due, 10U);
    EXPECT_EQ(release.releases, 2U);
    EXPECT_EQ(release.backlog, 3U);
    ASSERT_EQ(nx_periodic_poll(&periodic, 55, 2, &release), NX_SUCCESS);
    EXPECT_EQ(release.first_due, 30U);
    EXPECT_EQ(release.backlog, 1U);
    const auto before = periodic;
    EXPECT_EQ(nx_periodic_poll(&periodic, 54, 2, &release), NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&periodic, &before, sizeof(periodic)), 0);
    ASSERT_EQ(nx_periodic_reset(&periodic, 1, 10, NX_PERIODIC_CATCH_UP),
              NX_SUCCESS);
    EXPECT_EQ(periodic.next_due, 11U);
    EXPECT_EQ(nx_periodic_reset(&periodic, NX_DEADLINE_NEVER - 4, 4,
                                NX_PERIODIC_SKIP),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(periodic.next_due, 11U);
}

TEST(CommonTimers, SameDeadlineKeepsArmOrderAndCancelCannotTouchReuse) {
    nx_timers_t timers = {};
    nx_timer_slot_t slots[2] = {};
    ASSERT_EQ(nx_timers_initialize(&timers, slots, 2, 0), NX_SUCCESS);
    nx_timer_ticket_t first = {}, second = {}, reused = {};
    ASSERT_EQ(nx_timers_arm(&timers, 20, 11, &first), NX_SUCCESS);
    ASSERT_EQ(nx_timers_arm(&timers, 20, 22, &second), NX_SUCCESS);
    nx_timer_ticket_t untouched = {91, 92};
    EXPECT_EQ(nx_timers_arm(&timers, 20, 33, &untouched), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(untouched.slot, 91U);
    nx_time_us_t next = 99;
    ASSERT_EQ(nx_timers_next_due(&timers, &next), NX_SUCCESS);
    EXPECT_EQ(next, 20U);
    nx_timer_event_t event = {};
    event.tag = 91;
    EXPECT_EQ(nx_timers_pop_due(&timers, 19, &event), NX_ERROR_EMPTY);
    EXPECT_EQ(event.tag, 91U);
    ASSERT_EQ(nx_timers_pop_due(&timers, 20, &event), NX_SUCCESS);
    EXPECT_EQ(event.tag, 11U);
    ASSERT_EQ(nx_timers_arm(&timers, 20, 33, &reused), NX_SUCCESS);
    EXPECT_EQ(reused.slot, first.slot);
    EXPECT_GT(reused.epoch, first.epoch);
    EXPECT_EQ(nx_timers_cancel(&timers, first), NX_ERROR_STATE);
    ASSERT_EQ(nx_timers_pop_due(&timers, 20, &event), NX_SUCCESS);
    EXPECT_EQ(event.tag, 22U);
    ASSERT_EQ(nx_timers_cancel(&timers, reused), NX_SUCCESS);
    EXPECT_EQ(nx_timers_next_due(&timers, &next), NX_ERROR_EMPTY);
    EXPECT_EQ(next, 20U);
}

TEST(CommonTimers, ResetCancelsOldEpochAndCountersNeverWrap) {
    nx_timers_t timers = {};
    nx_timer_slot_t slot = {};
    ASSERT_EQ(nx_timers_initialize(&timers, &slot, 1, 100), NX_SUCCESS);
    nx_timer_ticket_t first = {}, next = {};
    ASSERT_EQ(nx_timers_arm(&timers, 200, 7, &first), NX_SUCCESS);
    nx_timer_event_t event = {};
    EXPECT_EQ(nx_timers_pop_due(&timers, 99, &event), NX_ERROR_STATE);
    EXPECT_TRUE(slot.active);
    ASSERT_EQ(nx_timers_reset(&timers, 0), NX_SUCCESS);
    ASSERT_EQ(nx_timers_arm(&timers, 1, 8, &next), NX_SUCCESS);
    EXPECT_GT(next.epoch, first.epoch);
    EXPECT_EQ(nx_timers_cancel(&timers, first), NX_ERROR_STATE);
    ASSERT_EQ(nx_timers_pop_due(&timers, 1, &event), NX_SUCCESS);
    slot.epoch = UINT64_MAX;
    EXPECT_EQ(nx_timers_arm(&timers, 2, 9, &next), NX_ERROR_EXHAUSTED);
    EXPECT_FALSE(slot.active);
}

class EventReceiver {
  public:
    MOCK_METHOD(nx_result_t, Write, (const void*, size_t));
    static nx_result_t write(void* context, const void* data, size_t length) {
        return static_cast<EventReceiver*>(context)->Write(data, length);
    }
};

TEST(CommonFanout, SlowSinkFailureDoesNotSuppressOtherStaticSinks) {
    testing::StrictMock<EventReceiver> first, second;
    const nx_event_sink_t sinks[] = {{&first, EventReceiver::write},
                                     {&second, EventReceiver::write}};
    nx_event_fanout_t fanout = {};
    ASSERT_EQ(nx_event_fanout_initialize(&fanout, sinks, 2), NX_SUCCESS);
    const int value = 42;
    {
        testing::InSequence sequence;
        EXPECT_CALL(first, Write(&value, sizeof(value)))
            .WillOnce(testing::Return(NX_ERROR_OVERFLOW));
        EXPECT_CALL(second, Write(&value, sizeof(value)))
            .WillOnce(testing::Return(NX_SUCCESS));
    }
    nx_event_fanout_report_t report = {};
    ASSERT_EQ(nx_event_fanout_emit(&fanout, &value, sizeof(value), &report),
              NX_SUCCESS);
    EXPECT_EQ(report.attempted, 2U);
    EXPECT_EQ(report.delivered, 1U);
    EXPECT_EQ(report.first_error, NX_ERROR_OVERFLOW);
}

nx_result_t write_stream(void* context, const void* data, size_t length) {
    auto* stream = static_cast<nx_stream_t*>(context);
    nx_stream_fill_t fill = {};
    const auto result = nx_stream_reserve(stream, &fill);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (length > fill.capacity) {
        (void)nx_stream_abort(stream, &fill, true);
        return NX_ERROR_OVERFLOW;
    }
    std::memcpy(fill.data, data, length);
    return nx_stream_publish(stream, &fill, length, 0);
}

TEST(CommonFanout, CopiesIntoExistingStreamLoansWithoutAnotherQueue) {
    uint8_t payload[sizeof(uint32_t)] = {};
    nx_stream_slot_t slot = {};
    slot.data = payload;
    slot.capacity = sizeof(payload);
    nx_stream_t stream = {};
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1), NX_SUCCESS);
    const nx_event_sink_t sink = {&stream, write_stream};
    nx_event_fanout_t fanout = {};
    ASSERT_EQ(nx_event_fanout_initialize(&fanout, &sink, 1), NX_SUCCESS);
    uint32_t event = 42;
    nx_event_fanout_report_t report = {};
    ASSERT_EQ(nx_event_fanout_emit(&fanout, &event, sizeof(event), &report),
              NX_SUCCESS);
    event = 99;
    ASSERT_EQ(nx_event_fanout_emit(&fanout, &event, sizeof(event), &report),
              NX_SUCCESS);
    EXPECT_EQ(report.first_error, NX_ERROR_OVERFLOW);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    uint32_t received = 0;
    std::memcpy(&received, block.data, sizeof(received));
    EXPECT_EQ(received, 42U);
    EXPECT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_stream_stop(&stream), NX_SUCCESS);
}

TEST(CommonFanout, ReentryRejectsBeforeCallingAnySinkTwice) {
    testing::StrictMock<EventReceiver> receiver;
    const nx_event_sink_t sink = {&receiver, EventReceiver::write};
    nx_event_fanout_t fanout = {};
    ASSERT_EQ(nx_event_fanout_initialize(&fanout, &sink, 1), NX_SUCCESS);
    int event = 1;
    EXPECT_CALL(receiver, Write(&event, sizeof(event)))
        .WillOnce([&](const void*, size_t) {
            nx_event_fanout_report_t nested = {91, 92, NX_ERROR_IO};
            EXPECT_EQ(
                nx_event_fanout_emit(&fanout, &event, sizeof(event), &nested),
                NX_ERROR_BUSY);
            EXPECT_EQ(nested.attempted, 91U);
            return NX_SUCCESS;
        });
    nx_event_fanout_report_t report = {};
    EXPECT_EQ(nx_event_fanout_emit(&fanout, &event, sizeof(event), &report),
              NX_SUCCESS);
    EXPECT_FALSE(fanout.emitting);
}
} /* namespace */
