/**
 * \file            rtc_lptim_start_test.cpp
 * \brief           Reopen preserves static owners and unique epochs
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "rtc_lptim_provider.h"
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

/** \brief Reopening may neither read nor advance this external Core source. */
class Clock {
  public:
    MOCK_METHOD(nx_time_us_t, Read, ());
};

nx_time_us_t ReadClock(void* context) {
    return static_cast<Clock*>(context)->Read();
}

/** \brief Compare complete owner representations across rejected operations. */
template <typename T>
std::array<unsigned char, sizeof(T)> Bytes(const T& value) {
    std::array<unsigned char, sizeof(T)> result{};
    std::memcpy(result.data(), &value, sizeof(value));
    return result;
}

/** \brief Request storage remains alive until real teardown completes. */
class NativeReopen : public testing::Test {
  protected:
    testing::StrictMock<Clock> source;
    nx_clock_t clock{ReadClock, &source};
    nx_native_rtc_state_t calendar{};
    nx_native_lptim_state_t counter{};
    const nx_rtc_port_t rtc{&nx_native_rtc_ops, &calendar};
    const nx_lptim_port_t lptim{&nx_native_lptim_ops, &counter};
    std::array<nx_rtc_request_t, 2> alarms{};
    std::array<nx_lptim_request_t, 2> shots{};

    void SetUp() override {
        ASSERT_EQ(nx_native_rtc_construct(&calendar, &clock), NX_SUCCESS);
        ASSERT_EQ(nx_native_lptim_construct(&counter, &clock, 32768),
                  NX_SUCCESS);
        for (auto& request : alarms) {
            nx_request_initialize(&request.base);
        }
        for (auto& request : shots) {
            nx_request_initialize(&request.base);
        }
    }
    void TearDown() override {
        EXPECT_EQ(nx_rtc_port_stop(&rtc), NX_SUCCESS);
        EXPECT_EQ(nx_lptim_port_stop(&lptim), NX_SUCCESS);
    }
    void SetCalendar(nx_rtc_calendar_t value = {24, 1, 1, 1, 0, 0, 0}) {
        ASSERT_EQ(nx_rtc_port_set(&rtc, &value), NX_SUCCESS);
    }
    void PrepareAlarm(size_t index, nx_rtc_alarm_t pattern = {1, 0, 0, 1}) {
        nx_rtc_snapshot_t snapshot{};
        ASSERT_EQ(nx_rtc_port_read(&rtc, &snapshot), NX_SUCCESS);
        ASSERT_EQ(nx_rtc_request_prepare(&alarms[index], &snapshot.domain,
                                         &pattern, NX_DEADLINE_NEVER),
                  NX_SUCCESS);
    }
    void PrepareShot(size_t index, uint32_t ticks = 10) {
        nx_lptim_sample_t sample{};
        ASSERT_EQ(nx_lptim_port_sample(&lptim, &sample), NX_SUCCESS);
        ASSERT_EQ(nx_lptim_request_prepare(&shots[index], &sample.domain, ticks,
                                           NX_DEADLINE_NEVER),
                  NX_SUCCESS);
    }
    void Result(const nx_request_t& request, nx_result_t expected,
                size_t count) {
        ASSERT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
        nx_result_t result = NX_ERROR_IO;
        size_t transferred = SIZE_MAX;
        ASSERT_EQ(nx_request_result(&request, &result, &transferred),
                  NX_SUCCESS);
        EXPECT_EQ(result, expected);
        EXPECT_EQ(transferred, count);
    }
};

TEST_F(NativeReopen, InvalidAndUnconstructedStorageRejectWithoutWriting) {
    nx_native_rtc_state_t fresh_rtc{};
    nx_native_lptim_state_t fresh_counter{};
    auto rtc_before = Bytes(fresh_rtc);
    auto counter_before = Bytes(fresh_counter);
    EXPECT_EQ(nx_native_rtc_start(nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_native_lptim_start(nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_native_rtc_start(&fresh_rtc), NX_ERROR_STATE);
    EXPECT_EQ(nx_native_lptim_start(&fresh_counter), NX_ERROR_STATE);
    EXPECT_EQ(Bytes(fresh_rtc), rtc_before);
    EXPECT_EQ(Bytes(fresh_counter), counter_before);
    EXPECT_EQ(nx_native_rtc_start(reinterpret_cast<nx_native_rtc_state_t*>(
                  reinterpret_cast<uintptr_t>(&fresh_rtc) + 1)),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_native_lptim_start(
                  reinterpret_cast<nx_native_lptim_state_t*>(UINTPTR_MAX - 7)),
              NX_ERROR_INVALID);
    EXPECT_EQ(Bytes(fresh_rtc), rtc_before);
    EXPECT_EQ(Bytes(fresh_counter), counter_before);
}

TEST_F(NativeReopen, OpenIdleIsStateButRetainedOpenLoansTakeBusyPriority) {
    auto rtc_before = Bytes(calendar);
    auto counter_before = Bytes(counter);
    EXPECT_EQ(nx_native_rtc_start(&calendar), NX_ERROR_STATE);
    EXPECT_EQ(nx_native_lptim_start(&counter), NX_ERROR_STATE);
    EXPECT_EQ(Bytes(calendar), rtc_before);
    EXPECT_EQ(Bytes(counter), counter_before);
    SetCalendar();
    PrepareAlarm(0);
    PrepareShot(0);
    ASSERT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_submit(&lptim, &shots[0]), NX_SUCCESS);
    rtc_before = Bytes(calendar);
    counter_before = Bytes(counter);
    EXPECT_EQ(nx_native_rtc_start(&calendar), NX_ERROR_BUSY);
    EXPECT_EQ(nx_native_lptim_start(&counter), NX_ERROR_BUSY);
    EXPECT_EQ(Bytes(calendar), rtc_before);
    EXPECT_EQ(Bytes(counter), counter_before);
    ASSERT_EQ(nx_rtc_port_stop(&rtc), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_stop(&lptim), NX_SUCCESS);
    Result(alarms[0].base, NX_ERROR_CANCELLED, 0);
    Result(shots[0].base, NX_ERROR_CANCELLED, 0);
}

TEST_F(NativeReopen, ReopenPreservesUnsetCalendarAndCopiedClockAndRate) {
    uint64_t rtc_generation = calendar.generation;
    uint64_t tick_generation = counter.generation;
    ASSERT_EQ(nx_rtc_port_stop(&rtc), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_stop(&lptim), NX_SUCCESS);
    clock.read = nullptr;
    clock.context = nullptr;
    ASSERT_EQ(nx_native_rtc_start(&calendar), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_start(&counter), NX_SUCCESS);
    EXPECT_EQ(calendar.generation, rtc_generation + 1);
    EXPECT_EQ(counter.generation, tick_generation + 1);
    EXPECT_EQ(calendar.clock.read, ReadClock);
    EXPECT_EQ(counter.clock.read, ReadClock);
    EXPECT_EQ(calendar.clock.context, &source);
    EXPECT_EQ(counter.clock.context, &source);
    EXPECT_FALSE(calendar.valid);
    nx_rtc_snapshot_t snapshot{};
    auto before = Bytes(snapshot);
    EXPECT_EQ(nx_rtc_port_read(&rtc, &snapshot), NX_ERROR_STATE);
    EXPECT_EQ(Bytes(snapshot), before);
    EXPECT_EQ(nx_native_rtc_model_advance(&rtc, 0), NX_ERROR_STATE);
    nx_lptim_sample_t sample{};
    ASSERT_EQ(nx_lptim_port_sample(&lptim, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.tick_hz, 32768u);
    EXPECT_EQ(sample.counter, 0);
    EXPECT_FALSE(sample.running);
}

TEST_F(NativeReopen, ClosedSourceProgressIsRetainedAndOldReadyAlarmStaysStale) {
    SetCalendar({24, 2, 28, 7, 23, 59, 59});
    PrepareAlarm(0, {29, 0, 0, 1});
    auto old_request = Bytes(alarms[0]);
    uint64_t generation = calendar.generation;
    ASSERT_EQ(nx_rtc_port_stop(&rtc), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_model_advance(&rtc, 1), NX_SUCCESS);
    nx_rtc_calendar_t retained = calendar.calendar;
    ASSERT_EQ(nx_native_rtc_start(&calendar), NX_SUCCESS);
    nx_rtc_snapshot_t snapshot{};
    ASSERT_EQ(nx_rtc_port_read(&rtc, &snapshot), NX_SUCCESS);
    EXPECT_EQ(Bytes(snapshot.calendar), Bytes(retained));
    EXPECT_EQ(snapshot.calendar.day, 29);
    EXPECT_EQ(snapshot.calendar.weekday, 1);
    EXPECT_EQ(snapshot.domain.generation, generation + 1);
    auto owner_before = Bytes(calendar);
    EXPECT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_ERROR_STATE);
    EXPECT_EQ(Bytes(alarms[0]), old_request);
    EXPECT_EQ(Bytes(calendar), owner_before);
    PrepareAlarm(1, {29, 0, 0, 1});
    ASSERT_EQ(nx_rtc_port_submit(&rtc, &alarms[1]), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_model_advance(&rtc, 1), NX_SUCCESS);
    ASSERT_EQ(nx_rtc_port_service(&rtc), NX_SUCCESS);
    Result(alarms[1].base, NX_SUCCESS, 1);
}

TEST_F(NativeReopen, ClosedHeldLoansRequireActualDrainBeforeReopen) {
    SetCalendar();
    PrepareAlarm(0);
    PrepareShot(0);
    uint64_t rtc_generation = calendar.generation;
    uint64_t tick_generation = counter.generation;
    ASSERT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_submit(&lptim, &shots[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_model_hold_withdraw(&rtc, true), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_hold_withdraw(&lptim, true), NX_SUCCESS);
    EXPECT_EQ(nx_rtc_port_stop(&rtc), NX_ERROR_BUSY);
    EXPECT_EQ(nx_lptim_port_stop(&lptim), NX_ERROR_BUSY);
    auto rtc_before = Bytes(calendar);
    auto counter_before = Bytes(counter);
    EXPECT_EQ(nx_native_rtc_start(&calendar), NX_ERROR_BUSY);
    EXPECT_EQ(nx_native_lptim_start(&counter), NX_ERROR_BUSY);
    EXPECT_EQ(Bytes(calendar), rtc_before);
    EXPECT_EQ(Bytes(counter), counter_before);
    ASSERT_EQ(nx_native_rtc_model_advance(&rtc, 2), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_advance(&lptim, 7), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_model_hold_withdraw(&rtc, false), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_hold_withdraw(&lptim, false), NX_SUCCESS);
    EXPECT_EQ(nx_native_rtc_start(&calendar), NX_ERROR_BUSY);
    EXPECT_EQ(nx_native_lptim_start(&counter), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&alarms[0].base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_request_state(&shots[0].base), NX_REQUEST_QUARANTINED);
    ASSERT_EQ(nx_rtc_port_service(&rtc), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_service(&lptim), NX_SUCCESS);
    Result(alarms[0].base, NX_ERROR_CANCELLED, 0);
    Result(shots[0].base, NX_ERROR_CANCELLED, 7);
    ASSERT_EQ(nx_native_rtc_start(&calendar), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_start(&counter), NX_SUCCESS);
    EXPECT_EQ(calendar.generation, rtc_generation + 1);
    EXPECT_EQ(counter.generation, tick_generation + 2);
}

TEST_F(NativeReopen,
       ExhaustedClosedOwnersRejectWithoutWrappingOrReconstruction) {
    ASSERT_EQ(nx_rtc_port_stop(&rtc), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_stop(&lptim), NX_SUCCESS);
    /* Quiescent private fixture boundary; no borrower or source is modified. */
    calendar.generation = UINT64_MAX;
    counter.generation = UINT64_MAX;
    auto rtc_before = Bytes(calendar);
    auto counter_before = Bytes(counter);
    EXPECT_EQ(nx_native_rtc_start(&calendar), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(nx_native_lptim_start(&counter), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(Bytes(calendar), rtc_before);
    EXPECT_EQ(Bytes(counter), counter_before);
    EXPECT_EQ(nx_native_rtc_construct(&calendar, &clock), NX_ERROR_STATE);
    EXPECT_EQ(nx_native_lptim_construct(&counter, &clock, 32768),
              NX_ERROR_STATE);
    EXPECT_EQ(Bytes(calendar), rtc_before);
    EXPECT_EQ(Bytes(counter), counter_before);
}

TEST_F(NativeReopen, LastAvailableReopenHasNoHiddenEpochReset) {
    SetCalendar();
    ASSERT_EQ(nx_rtc_port_stop(&rtc), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_stop(&lptim), NX_SUCCESS);
    /* Only a joined owner is placed at the exhaustion boundary. */
    calendar.generation = UINT64_MAX - 1;
    counter.generation = UINT64_MAX - 1;
    ASSERT_EQ(nx_native_rtc_start(&calendar), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_start(&counter), NX_SUCCESS);
    EXPECT_EQ(calendar.generation, UINT64_MAX);
    EXPECT_EQ(counter.generation, UINT64_MAX);
    PrepareShot(0, 1);
    auto before = Bytes(counter);
    EXPECT_EQ(nx_lptim_port_submit(&lptim, &shots[0]), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(Bytes(counter), before);
    ASSERT_EQ(nx_rtc_port_stop(&rtc), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_stop(&lptim), NX_SUCCESS);
    EXPECT_EQ(nx_native_rtc_start(&calendar), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(nx_native_lptim_start(&counter), NX_ERROR_EXHAUSTED);
}

TEST_F(NativeReopen, ExternalWithdrawalFaultSurvivesAJoinedReopen) {
    SetCalendar();
    ASSERT_EQ(nx_native_rtc_model_hold_withdraw(&rtc, true), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_hold_withdraw(&lptim, true), NX_SUCCESS);
    ASSERT_EQ(nx_rtc_port_stop(&rtc), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_stop(&lptim), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_start(&calendar), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_start(&counter), NX_SUCCESS);
    EXPECT_TRUE(calendar.hold_withdraw);
    EXPECT_TRUE(counter.hold_withdraw);
    PrepareAlarm(0);
    PrepareShot(0, 1);
    ASSERT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_submit(&lptim, &shots[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_model_advance(&rtc, 1), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_advance(&lptim, 1), NX_SUCCESS);
    EXPECT_EQ(nx_rtc_port_service(&rtc), NX_ERROR_BUSY);
    EXPECT_EQ(nx_lptim_port_service(&lptim), NX_ERROR_BUSY);
    EXPECT_EQ(nx_native_rtc_start(&calendar), NX_ERROR_BUSY);
    EXPECT_EQ(nx_native_lptim_start(&counter), NX_ERROR_BUSY);
    ASSERT_EQ(nx_native_rtc_model_hold_withdraw(&rtc, false), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_hold_withdraw(&lptim, false), NX_SUCCESS);
    ASSERT_EQ(nx_rtc_port_service(&rtc), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_service(&lptim), NX_SUCCESS);
    Result(alarms[0].base, NX_SUCCESS, 1);
    Result(shots[0].base, NX_SUCCESS, 1);
}

TEST_F(NativeReopen, RepeatedStopStartCannotReviveReadyTokensOrFreshConstruct) {
    SetCalendar();
    PrepareAlarm(0);
    PrepareShot(0);
    uint64_t rtc_generation = calendar.generation;
    uint64_t tick_generation = counter.generation;
    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        ASSERT_EQ(nx_rtc_port_stop(&rtc), NX_SUCCESS);
        ASSERT_EQ(nx_lptim_port_stop(&lptim), NX_SUCCESS);
        ASSERT_EQ(nx_native_rtc_start(&calendar), NX_SUCCESS);
        ASSERT_EQ(nx_native_lptim_start(&counter), NX_SUCCESS);
        EXPECT_EQ(calendar.generation, rtc_generation + cycle + 1);
        EXPECT_EQ(counter.generation, tick_generation + cycle + 1);
    }
    auto alarm_before = Bytes(alarms[0]);
    auto shot_before = Bytes(shots[0]);
    auto rtc_before = Bytes(calendar);
    auto counter_before = Bytes(counter);
    EXPECT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_ERROR_STATE);
    EXPECT_EQ(nx_lptim_port_submit(&lptim, &shots[0]), NX_ERROR_STATE);
    EXPECT_EQ(nx_native_rtc_construct(&calendar, &clock), NX_ERROR_STATE);
    EXPECT_EQ(nx_native_lptim_construct(&counter, &clock, 32768),
              NX_ERROR_STATE);
    EXPECT_EQ(Bytes(alarms[0]), alarm_before);
    EXPECT_EQ(Bytes(shots[0]), shot_before);
    EXPECT_EQ(Bytes(calendar), rtc_before);
    EXPECT_EQ(Bytes(counter), counter_before);
}

} /* namespace */
