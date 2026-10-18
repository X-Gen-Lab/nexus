/**
 * \file            rtc_lptim_test.cpp
 * \brief           Raw calendar, finite ticks and explicit loan contracts
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
#include <limits>

namespace {

/** \brief The sole external Core source; provider/model never advances it. */
class Clock {
  public:
    MOCK_METHOD(nx_time_us_t, Read, ());
};

/** \brief Forward only the injected external dependency, not the provider. */
nx_time_us_t ReadClock(void* context) {
    return static_cast<Clock*>(context)->Read();
}

/** \brief A legal stateless clock does not require a non-NULL context. */
nx_time_us_t StatelessClock(void*) {
    return 0;
}

/** \brief Preserve padding too when testing a rejected writable object. */
template <typename T>
std::array<unsigned char, sizeof(T)> Bytes(const T& value) {
    std::array<unsigned char, sizeof(T)> bytes{};
    std::memcpy(bytes.data(), &value, sizeof(value));
    return bytes;
}

/** \brief Exact final result is read only after acquire-observed settlement. */
void Result(const nx_request_t& request, nx_result_t expected, size_t count) {
    ASSERT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
    nx_result_t result = NX_ERROR_IO;
    size_t transferred = SIZE_MAX;
    ASSERT_EQ(nx_request_result(&request, &result, &transferred), NX_SUCCESS);
    EXPECT_EQ(result, expected);
    EXPECT_EQ(transferred, count);
}

/** \brief Each case owns fresh sources; requests outlive real teardown stop. */
class NativeTime : public testing::Test {
  protected:
    testing::StrictMock<Clock> source;
    nx_time_us_t now = 0;
    nx_clock_t clock{ReadClock, &source};
    nx_native_rtc_state_t rtc_a{};
    nx_native_rtc_state_t rtc_b{};
    nx_native_lptim_state_t timer_a{};
    nx_native_lptim_state_t timer_b{};
    const nx_rtc_port_t rtc{&nx_native_rtc_ops, &rtc_a};
    const nx_rtc_port_t other_rtc{&nx_native_rtc_ops, &rtc_b};
    const nx_lptim_port_t timer{&nx_native_lptim_ops, &timer_a};
    const nx_lptim_port_t other_timer{&nx_native_lptim_ops, &timer_b};
    std::array<nx_rtc_request_t, 4> alarms{};
    std::array<nx_lptim_request_t, 4> shots{};

    void SetUp() override {
        ASSERT_EQ(nx_native_rtc_construct(&rtc_a, &clock), NX_SUCCESS);
        ASSERT_EQ(nx_native_rtc_construct(&rtc_b, &clock), NX_SUCCESS);
        ASSERT_EQ(nx_native_lptim_construct(&timer_a, &clock, 32768),
                  NX_SUCCESS);
        ASSERT_EQ(nx_native_lptim_construct(&timer_b, &clock, 1000),
                  NX_SUCCESS);
        for (auto& alarm : alarms) {
            nx_request_initialize(&alarm.base);
        }
        for (auto& shot : shots) {
            nx_request_initialize(&shot.base);
        }
    }
    void TearDown() override {
        EXPECT_EQ(nx_rtc_port_stop(&rtc), NX_SUCCESS);
        EXPECT_EQ(nx_rtc_port_stop(&other_rtc), NX_SUCCESS);
        EXPECT_EQ(nx_lptim_port_stop(&timer), NX_SUCCESS);
        EXPECT_EQ(nx_lptim_port_stop(&other_timer), NX_SUCCESS);
    }
    void Set(const nx_rtc_port_t& port,
             nx_rtc_calendar_t value = {24, 1, 1, 1, 0, 0, 0}) {
        ASSERT_EQ(nx_rtc_port_set(&port, &value), NX_SUCCESS);
    }
    void Alarm(size_t index, const nx_rtc_port_t& port,
               nx_rtc_alarm_t value = {1, 0, 0, 1},
               nx_time_us_t deadline = NX_DEADLINE_NEVER) {
        nx_rtc_snapshot_t snapshot{};
        ASSERT_EQ(nx_rtc_port_read(&port, &snapshot), NX_SUCCESS);
        ASSERT_EQ(nx_rtc_request_prepare(&alarms[index], &snapshot.domain,
                                         &value, deadline),
                  NX_SUCCESS);
    }
    void Shot(size_t index, const nx_lptim_port_t& port, uint32_t ticks = 10,
              nx_time_us_t deadline = NX_DEADLINE_NEVER) {
        nx_lptim_sample_t sample{};
        ASSERT_EQ(nx_lptim_port_sample(&port, &sample), NX_SUCCESS);
        ASSERT_EQ(nx_lptim_request_prepare(&shots[index], &sample.domain, ticks,
                                           deadline),
                  NX_SUCCESS);
    }
};

TEST_F(NativeTime, ColdConstructionCopiesClockWithoutReadingOrSettingTime) {
    nx_rtc_snapshot_t snapshot;
    std::memset(&snapshot, 0xa5, sizeof(snapshot));
    auto before = Bytes(snapshot);
    EXPECT_EQ(nx_rtc_port_read(&rtc, &snapshot), NX_ERROR_STATE);
    EXPECT_EQ(Bytes(snapshot), before);
    nx_lptim_sample_t sample{};
    ASSERT_EQ(nx_lptim_port_sample(&timer, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.tick_hz, 32768u);
    EXPECT_EQ(sample.counter, 0);
    EXPECT_FALSE(sample.running);
    EXPECT_NE(sample.domain.identity, nullptr);
    EXPECT_NE(sample.domain.generation, 0u);
    EXPECT_EQ(rtc_a.clock.context, &source);
    EXPECT_EQ(rtc_a.clock.read, ReadClock);
    EXPECT_EQ(rtc.ops, other_rtc.ops);
    EXPECT_EQ(timer.ops, other_timer.ops);
    clock.read = nullptr;
    EXPECT_EQ(rtc_a.clock.read, ReadClock);
}

TEST_F(NativeTime, StatelessClockAndRejectedConstructionLeaveStorageIntact) {
    nx_native_lptim_state_t local{};
    const nx_clock_t stateless{StatelessClock, nullptr};
    const nx_lptim_port_t face{&nx_native_lptim_ops, &local};
    EXPECT_EQ(nx_native_lptim_construct(&local, &stateless, 0),
              NX_ERROR_INVALID);
    EXPECT_EQ(Bytes(local), Bytes(nx_native_lptim_state_t{}));
    ASSERT_EQ(nx_native_lptim_construct(&local, &stateless, 1), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_stop(&face), NX_SUCCESS);
    auto before = Bytes(local);
    EXPECT_EQ(nx_native_lptim_construct(&local, &stateless, 1), NX_ERROR_STATE);
    EXPECT_EQ(Bytes(local), before);
    const nx_clock_t missing{};
    nx_native_rtc_state_t fresh{};
    EXPECT_EQ(nx_native_rtc_construct(&fresh, &missing), NX_ERROR_INVALID);
    EXPECT_EQ(Bytes(fresh), Bytes(nx_native_rtc_state_t{}));
    EXPECT_EQ(nx_native_rtc_construct(
                  &fresh, reinterpret_cast<const nx_clock_t*>(&fresh)),
              NX_ERROR_INVALID);
    EXPECT_EQ(Bytes(fresh), Bytes(nx_native_rtc_state_t{}));
}

TEST_F(NativeTime, CalendarValidationUsesRawMod4YearsAndIndependentWeekday) {
    std::array<nx_rtc_calendar_t, 8> valid{{{0, 2, 29, 7, 23, 59, 59},
                                            {4, 2, 29, 1, 0, 0, 0},
                                            {99, 12, 31, 3, 23, 59, 59},
                                            {24, 4, 30, 1, 0, 0, 0},
                                            {1, 2, 28, 7, 0, 0, 0},
                                            {24, 1, 31, 1, 0, 0, 0},
                                            {24, 6, 30, 1, 0, 0, 0},
                                            {24, 11, 30, 1, 0, 0, 0}}};
    for (const auto& value : valid) {
        EXPECT_EQ(nx_rtc_calendar_validate(&value), NX_SUCCESS);
    }
    std::array<nx_rtc_calendar_t, 11> invalid{{{100, 1, 1, 1, 0, 0, 0},
                                               {1, 2, 29, 1, 0, 0, 0},
                                               {24, 0, 1, 1, 0, 0, 0},
                                               {24, 13, 1, 1, 0, 0, 0},
                                               {24, 4, 31, 1, 0, 0, 0},
                                               {24, 1, 0, 1, 0, 0, 0},
                                               {24, 1, 1, 0, 0, 0, 0},
                                               {24, 1, 1, 8, 0, 0, 0},
                                               {24, 1, 1, 1, 24, 0, 0},
                                               {24, 1, 1, 1, 0, 60, 0},
                                               {24, 1, 1, 1, 0, 0, 60}}};
    for (const auto& value : invalid) {
        EXPECT_EQ(nx_rtc_calendar_validate(&value), NX_ERROR_INVALID);
    }
    EXPECT_EQ(nx_rtc_calendar_validate(nullptr), NX_ERROR_INVALID);
    const nx_rtc_alarm_t invalid_alarm{32, 0, 0, 0};
    EXPECT_EQ(nx_rtc_alarm_validate(&invalid_alarm), NX_ERROR_INVALID);
}

TEST_F(NativeTime, PreparationValidatesEveryInputBeforeChangingBaseOrPayload) {
    const nx_rtc_domain_t domain{&rtc_a, 1};
    const nx_rtc_domain_t missing{&rtc_a, 0};
    const nx_rtc_alarm_t good{1, 0, 0, 1};
    const nx_rtc_alarm_t bad{1, 24, 0, 0};
    auto before = Bytes(alarms[0]);
    EXPECT_EQ(nx_rtc_request_prepare(&alarms[0], &domain, &bad, 17),
              NX_ERROR_INVALID);
    EXPECT_EQ(Bytes(alarms[0]), before);
    EXPECT_EQ(nx_rtc_request_prepare(&alarms[0], &missing, &good, 17),
              NX_ERROR_INVALID);
    EXPECT_EQ(Bytes(alarms[0]), before);
    const nx_lptim_domain_t ticks{&timer_a, 1};
    auto tick_before = Bytes(shots[0]);
    for (uint32_t count : {0u, 65537u, UINT32_MAX}) {
        EXPECT_EQ(nx_lptim_request_prepare(&shots[0], &ticks, count, 19),
                  NX_ERROR_INVALID);
        EXPECT_EQ(Bytes(shots[0]), tick_before);
    }
}

TEST_F(NativeTime,
       PreparationRejectsPartialInputOverlapWithoutDereferencingIt) {
    const nx_rtc_domain_t domain{&rtc_a, 1};
    const nx_rtc_alarm_t alarm{1, 0, 0, 1};
    auto before = Bytes(alarms[0]);
    auto* interior = reinterpret_cast<const nx_rtc_domain_t*>(
        reinterpret_cast<unsigned char*>(&alarms[0]) +
        alignof(nx_rtc_domain_t));
    EXPECT_EQ(nx_rtc_request_prepare(&alarms[0], interior, &alarm, 5),
              NX_ERROR_INVALID);
    EXPECT_EQ(Bytes(alarms[0]), before);
    auto* partial = reinterpret_cast<const nx_rtc_alarm_t*>(
        reinterpret_cast<unsigned char*>(&alarms[0]) + sizeof(alarms[0]) - 2);
    EXPECT_EQ(nx_rtc_request_prepare(&alarms[0], &domain, partial, 5),
              NX_ERROR_INVALID);
    EXPECT_EQ(Bytes(alarms[0]), before);
    auto shot_before = Bytes(shots[0]);
    auto* tick_domain = reinterpret_cast<const nx_lptim_domain_t*>(
        reinterpret_cast<unsigned char*>(&shots[0]) + sizeof(shots[0]) - 8);
    EXPECT_EQ(nx_lptim_request_prepare(&shots[0], tick_domain, 1, 5),
              NX_ERROR_INVALID);
    EXPECT_EQ(Bytes(shots[0]), shot_before);
}

TEST_F(NativeTime, TwoCalendarsAndTwoCountersHaveIndependentOwnerDomains) {
    Set(rtc);
    nx_rtc_snapshot_t first{}, other{};
    ASSERT_EQ(nx_rtc_port_read(&rtc, &first), NX_SUCCESS);
    EXPECT_EQ(nx_rtc_port_read(&other_rtc, &other), NX_ERROR_STATE);
    Set(other_rtc);
    ASSERT_EQ(nx_rtc_port_read(&other_rtc, &other), NX_SUCCESS);
    EXPECT_NE(first.domain.identity, other.domain.identity);
    Alarm(0, rtc);
    EXPECT_EQ(nx_rtc_port_submit(&other_rtc, &alarms[0]), NX_ERROR_STATE);
    ASSERT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_model_advance(&other_rtc, 1), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&alarms[0].base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(nx_rtc_port_service(&rtc), NX_ERROR_BUSY);
    ASSERT_EQ(nx_rtc_port_cancel(&rtc, &alarms[0]), NX_SUCCESS);
    Shot(0, timer);
    EXPECT_EQ(nx_lptim_port_submit(&other_timer, &shots[0]), NX_ERROR_STATE);
    ASSERT_EQ(nx_lptim_port_submit(&timer, &shots[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_advance(&other_timer, 7), NX_SUCCESS);
    nx_lptim_sample_t sample{};
    ASSERT_EQ(nx_lptim_port_sample(&timer, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.counter, 0);
    ASSERT_EQ(nx_lptim_port_sample(&other_timer, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.tick_hz, 1000u);
    EXPECT_FALSE(sample.running);
    ASSERT_EQ(nx_lptim_port_cancel(&timer, &shots[0]), NX_SUCCESS);
}

TEST_F(NativeTime, SetInvalidatesOldDomainAndCannotMutateAnAcceptedAlarm) {
    Set(rtc);
    Alarm(0, rtc);
    Set(rtc, {24, 1, 1, 5, 0, 0, 0});
    auto request_before = Bytes(alarms[0]);
    EXPECT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_ERROR_STATE);
    EXPECT_EQ(Bytes(alarms[0]), request_before);
    Alarm(0, rtc);
    ASSERT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_SUCCESS);
    auto before = Bytes(rtc_a);
    const nx_rtc_calendar_t value{24, 1, 2, 2, 0, 0, 0};
    EXPECT_EQ(nx_rtc_port_set(&rtc, &value), NX_ERROR_BUSY);
    EXPECT_EQ(Bytes(rtc_a), before);
    EXPECT_EQ(nx_rtc_port_cancel(&rtc, &alarms[0]), NX_SUCCESS);
}

TEST_F(NativeTime, AlreadyMatchingAlarmIsRejectedWithoutEffects) {
    Set(rtc);
    Alarm(0, rtc, {1, 0, 0, 0});
    auto state = Bytes(rtc_a);
    auto request = Bytes(alarms[0]);
    EXPECT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_ERROR_STATE);
    EXPECT_EQ(Bytes(rtc_a), state);
    EXPECT_EQ(Bytes(alarms[0]), request);
}

TEST_F(NativeTime, EqualityDeadlineRejectsBeforeBorrowOrSourceEffects) {
    Set(rtc);
    Alarm(0, rtc, {1, 0, 0, 1}, 100);
    Shot(0, timer, 10, 100);
    auto rtc_before = Bytes(rtc_a);
    auto timer_before = Bytes(timer_a);
    auto alarm_before = Bytes(alarms[0]);
    auto shot_before = Bytes(shots[0]);
    EXPECT_CALL(source, Read()).Times(2).WillRepeatedly(testing::Return(100));
    EXPECT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_lptim_port_submit(&timer, &shots[0]), NX_ERROR_TIMEOUT);
    EXPECT_EQ(Bytes(rtc_a), rtc_before);
    EXPECT_EQ(Bytes(timer_a), timer_before);
    EXPECT_EQ(Bytes(alarms[0]), alarm_before);
    EXPECT_EQ(Bytes(shots[0]), shot_before);
}

TEST_F(NativeTime, InvalidStaticAdmissionDoesNotReadCoreClock) {
    Set(rtc);
    Alarm(0, rtc, {1, 0, 0, 1}, 10);
    alarms[0].domain.generation++;
    EXPECT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_ERROR_STATE);
    Shot(0, timer, 10, 10);
    shots[0].ticks = 0;
    EXPECT_EQ(nx_lptim_port_submit(&timer, &shots[0]), NX_ERROR_INVALID);
}

TEST_F(NativeTime, AlarmNeedsFutureBoundaryAndExplicitServiceBeforeSettlement) {
    Set(rtc);
    Alarm(0, rtc);
    ASSERT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_model_advance(&rtc, 0), NX_SUCCESS);
    EXPECT_EQ(nx_rtc_port_service(&rtc), NX_ERROR_BUSY);
    auto before = Bytes(alarms[0]);
    ASSERT_EQ(nx_native_rtc_model_advance(&rtc, 1), NX_SUCCESS);
    EXPECT_EQ(Bytes(alarms[0]), before);
    ASSERT_EQ(nx_rtc_port_service(&rtc), NX_SUCCESS);
    Result(alarms[0].base, NX_SUCCESS, 1);
    EXPECT_EQ(rtc_a.active, nullptr);
    EXPECT_FALSE(rtc_a.armed);
    EXPECT_FALSE(rtc_a.pending);
}

TEST_F(NativeTime, CalendarCarryUsesActualMonthLengthAndIndependentWeekday) {
    struct Case {
        nx_rtc_calendar_t from;
        nx_rtc_calendar_t to;
    };
    const std::array<Case, 5> cases{
        {{{0, 2, 28, 7, 0, 0, 0}, {0, 2, 29, 1, 0, 0, 0}},
         {{1, 2, 28, 3, 0, 0, 0}, {1, 3, 1, 4, 0, 0, 0}},
         {{24, 4, 30, 4, 0, 0, 0}, {24, 5, 1, 5, 0, 0, 0}},
         {{99, 12, 31, 7, 0, 0, 0}, {0, 1, 1, 1, 0, 0, 0}},
         {{24, 1, 31, 2, 0, 0, 0}, {24, 2, 1, 3, 0, 0, 0}}}};
    for (const auto& item : cases) {
        Set(rtc, item.from);
        nx_rtc_snapshot_t before{}, after{};
        ASSERT_EQ(nx_rtc_port_read(&rtc, &before), NX_SUCCESS);
        ASSERT_EQ(nx_native_rtc_model_advance(&rtc, 86400), NX_SUCCESS);
        ASSERT_EQ(nx_rtc_port_read(&rtc, &after), NX_SUCCESS);
        EXPECT_EQ(Bytes(after.calendar), Bytes(item.to));
        EXPECT_EQ(after.domain.generation, before.domain.generation);
    }
}

TEST_F(NativeTime, AlarmCrossesMidnightWithoutLosingIntermediateMatch) {
    Set(rtc, {99, 12, 31, 7, 23, 59, 59});
    Alarm(0, rtc, {1, 0, 0, 0});
    ASSERT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_model_advance(&rtc, 2), NX_SUCCESS);
    nx_rtc_snapshot_t snapshot{};
    ASSERT_EQ(nx_rtc_port_read(&rtc, &snapshot), NX_SUCCESS);
    EXPECT_EQ(snapshot.calendar.year, 0);
    EXPECT_EQ(snapshot.calendar.weekday, 1);
    EXPECT_EQ(snapshot.calendar.second, 1);
    ASSERT_EQ(nx_rtc_port_service(&rtc), NX_SUCCESS);
    Result(alarms[0].base, NX_SUCCESS, 1);
}

TEST_F(NativeTime, ClosedCalendarCanAdvanceButPublicReadAndReconstructReject) {
    Set(rtc);
    ASSERT_EQ(nx_rtc_port_stop(&rtc), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_model_advance(&rtc, 1), NX_SUCCESS);
    EXPECT_EQ(rtc_a.calendar.second, 1);
    nx_rtc_snapshot_t snapshot{};
    auto output_before = Bytes(snapshot);
    EXPECT_EQ(nx_rtc_port_read(&rtc, &snapshot), NX_ERROR_STATE);
    EXPECT_EQ(Bytes(snapshot), output_before);
    auto before = Bytes(rtc_a);
    EXPECT_EQ(nx_native_rtc_construct(&rtc_a, &clock), NX_ERROR_STATE);
    EXPECT_EQ(Bytes(rtc_a), before);
    EXPECT_EQ(nx_native_rtc_model_advance(&rtc, 86401), NX_ERROR_INVALID);
    EXPECT_EQ(Bytes(rtc_a), before);
    EXPECT_EQ(nx_native_rtc_model_advance(&other_rtc, 0), NX_ERROR_STATE);
}

TEST_F(NativeTime, Full65536ShotWrapsCounterWithoutPublishingEarly) {
    Shot(0, timer, 65536);
    ASSERT_EQ(nx_lptim_port_submit(&timer, &shots[0]), NX_SUCCESS);
    auto before = Bytes(shots[0]);
    ASSERT_EQ(nx_native_lptim_model_advance(&timer, 65536), NX_SUCCESS);
    EXPECT_EQ(Bytes(shots[0]), before);
    nx_lptim_sample_t sample{};
    ASSERT_EQ(nx_lptim_port_sample(&timer, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.counter, 0);
    EXPECT_TRUE(sample.running);
    auto state = Bytes(timer_a);
    EXPECT_EQ(nx_native_lptim_model_advance(&timer, 65537), NX_ERROR_INVALID);
    EXPECT_EQ(Bytes(timer_a), state);
    ASSERT_EQ(nx_lptim_port_service(&timer), NX_SUCCESS);
    Result(shots[0].base, NX_SUCCESS, 65536);
    ASSERT_EQ(nx_lptim_port_sample(&timer, &sample), NX_SUCCESS);
    EXPECT_FALSE(sample.running);
    EXPECT_NE(sample.domain.generation, shots[0].domain.generation);
}

TEST_F(NativeTime, IdleProgressIsNotAnImplicitClockAndShotOneCanBeReused) {
    ASSERT_EQ(nx_native_lptim_model_advance(&timer, 100), NX_SUCCESS);
    Shot(0, timer, 1);
    ASSERT_EQ(nx_lptim_port_submit(&timer, &shots[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_advance(&timer, 0), NX_SUCCESS);
    EXPECT_EQ(nx_lptim_port_service(&timer), NX_ERROR_BUSY);
    ASSERT_EQ(nx_native_lptim_model_advance(&timer, 2), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_service(&timer), NX_SUCCESS);
    Result(shots[0].base, NX_SUCCESS, 1);
    EXPECT_EQ(nx_lptim_port_submit(&timer, &shots[0]), NX_ERROR_STATE);
    Shot(0, timer, 4);
    ASSERT_EQ(nx_lptim_port_submit(&timer, &shots[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_advance(&timer, 4), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_service(&timer), NX_SUCCESS);
    Result(shots[0].base, NX_SUCCESS, 4);
}

TEST_F(NativeTime, HeldCancelKeepsBorrowAndReportsFinalSevenTicks) {
    Shot(0, timer);
    ASSERT_EQ(nx_lptim_port_submit(&timer, &shots[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_advance(&timer, 3), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_hold_withdraw(&timer, true), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_cancel(&timer, &shots[0]), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&shots[0].base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(timer_a.active, &shots[0]);
    auto request = Bytes(shots[0]);
    ASSERT_EQ(nx_native_lptim_model_advance(&timer, 4), NX_SUCCESS);
    EXPECT_EQ(Bytes(shots[0]), request);
    EXPECT_EQ(nx_lptim_port_service(&timer), NX_ERROR_BUSY);
    ASSERT_EQ(nx_lptim_port_cancel(&timer, &shots[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_hold_withdraw(&timer, false), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&shots[0].base), NX_REQUEST_QUARANTINED);
    ASSERT_EQ(nx_lptim_port_service(&timer), NX_SUCCESS);
    Result(shots[0].base, NX_ERROR_CANCELLED, 7);
    EXPECT_EQ(timer_a.active, nullptr);
    EXPECT_FALSE(timer_a.armed);
    EXPECT_FALSE(timer_a.pending);
}

TEST_F(NativeTime, HeldTimeoutKeepsFirstCauseButProgressesToFinalSevenTicks) {
    Shot(0, timer, 10, 10);
    EXPECT_CALL(source, Read()).Times(2).WillRepeatedly([this] { return now; });
    ASSERT_EQ(nx_lptim_port_submit(&timer, &shots[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_advance(&timer, 3), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_hold_withdraw(&timer, true), NX_SUCCESS);
    now = 10;
    EXPECT_EQ(nx_lptim_port_service(&timer), NX_ERROR_BUSY);
    ASSERT_EQ(nx_native_lptim_model_advance(&timer, 4), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_cancel(&timer, &shots[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_hold_withdraw(&timer, false), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_service(&timer), NX_SUCCESS);
    Result(shots[0].base, NX_ERROR_TIMEOUT, 7);
}

TEST_F(NativeTime, HeldRTCEventDoesNotOverrideEarlierCancellation) {
    Set(rtc);
    Alarm(0, rtc);
    ASSERT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_model_hold_withdraw(&rtc, true), NX_SUCCESS);
    ASSERT_EQ(nx_rtc_port_cancel(&rtc, &alarms[0]), NX_SUCCESS);
    auto request = Bytes(alarms[0]);
    ASSERT_EQ(nx_native_rtc_model_advance(&rtc, 2), NX_SUCCESS);
    EXPECT_EQ(Bytes(alarms[0]), request);
    EXPECT_EQ(nx_rtc_port_service(&rtc), NX_ERROR_BUSY);
    ASSERT_EQ(nx_native_rtc_model_hold_withdraw(&rtc, false), NX_SUCCESS);
    ASSERT_EQ(nx_rtc_port_service(&rtc), NX_SUCCESS);
    Result(alarms[0].base, NX_ERROR_CANCELLED, 0);
}

TEST_F(NativeTime, FirstObservedExpiryWinsOverPendingRTCEvent) {
    Set(rtc);
    Alarm(0, rtc, {1, 0, 0, 1}, 10);
    EXPECT_CALL(source, Read()).Times(2).WillRepeatedly([this] { return now; });
    ASSERT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_model_advance(&rtc, 1), NX_SUCCESS);
    now = 10;
    ASSERT_EQ(nx_rtc_port_service(&rtc), NX_SUCCESS);
    Result(alarms[0].base, NX_ERROR_TIMEOUT, 0);
}

TEST_F(NativeTime, DeadlineEventTieAndCancelBeforeServiceHaveDistinctCauses) {
    Shot(0, timer, 1, 10);
    Shot(1, other_timer, 1, 10);
    EXPECT_CALL(source, Read()).Times(3).WillRepeatedly([this] { return now; });
    ASSERT_EQ(nx_lptim_port_submit(&timer, &shots[0]), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_submit(&other_timer, &shots[1]), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_advance(&timer, 1), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_advance(&other_timer, 1), NX_SUCCESS);
    now = 10;
    ASSERT_EQ(nx_lptim_port_service(&timer), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_cancel(&other_timer, &shots[1]), NX_SUCCESS);
    Result(shots[0].base, NX_ERROR_TIMEOUT, 1);
    Result(shots[1].base, NX_ERROR_CANCELLED, 1);
}

TEST_F(NativeTime, HeldStopClosesAdmissionButNeverRevokesEitherLoan) {
    Set(rtc);
    Alarm(0, rtc);
    Shot(0, timer);
    ASSERT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_submit(&timer, &shots[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_model_hold_withdraw(&rtc, true), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_hold_withdraw(&timer, true), NX_SUCCESS);
    EXPECT_EQ(nx_rtc_port_stop(&rtc), NX_ERROR_BUSY);
    EXPECT_EQ(nx_lptim_port_stop(&timer), NX_ERROR_BUSY);
    EXPECT_EQ(nx_rtc_port_stop(&rtc), NX_ERROR_BUSY);
    EXPECT_EQ(nx_lptim_port_stop(&timer), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&alarms[0].base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_request_state(&shots[0].base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_rtc_port_submit(&rtc, &alarms[1]), NX_ERROR_STATE);
    EXPECT_EQ(nx_lptim_port_submit(&timer, &shots[1]), NX_ERROR_STATE);
    ASSERT_EQ(nx_native_lptim_model_advance(&timer, 7), NX_SUCCESS);
    ASSERT_EQ(nx_native_rtc_model_hold_withdraw(&rtc, false), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_hold_withdraw(&timer, false), NX_SUCCESS);
    ASSERT_EQ(nx_rtc_port_stop(&rtc), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_stop(&timer), NX_SUCCESS);
    Result(alarms[0].base, NX_ERROR_CANCELLED, 0);
    Result(shots[0].base, NX_ERROR_CANCELLED, 7);
    auto before = Bytes(timer_a);
    EXPECT_EQ(nx_native_lptim_model_advance(&timer, 3), NX_SUCCESS);
    EXPECT_EQ(Bytes(timer_a), before);
}

TEST_F(NativeTime, ExhaustionRejectsSetButNeverAbandonsLastAcceptedShot) {
    Set(rtc);
    rtc_a.generation = UINT64_MAX;
    const nx_rtc_calendar_t changed{24, 2, 1, 1, 0, 0, 0};
    auto rtc_before = Bytes(rtc_a);
    EXPECT_EQ(nx_rtc_port_set(&rtc, &changed), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(Bytes(rtc_a), rtc_before);
    timer_a.generation = UINT64_MAX - 1;
    Shot(0, timer, 1);
    ASSERT_EQ(nx_lptim_port_submit(&timer, &shots[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_lptim_model_advance(&timer, 1), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_service(&timer), NX_SUCCESS);
    Result(shots[0].base, NX_SUCCESS, 1);
    EXPECT_EQ(timer_a.generation, UINT64_MAX);
    Shot(1, timer, 1);
    auto state = Bytes(timer_a);
    auto request = Bytes(shots[1]);
    EXPECT_EQ(nx_lptim_port_submit(&timer, &shots[1]), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(Bytes(timer_a), state);
    EXPECT_EQ(Bytes(shots[1]), request);
}

TEST_F(NativeTime, WholeAndPartialProviderAliasesRejectBeforeAnyWrite) {
    Set(rtc);
    auto rtc_before = Bytes(rtc_a);
    auto timer_before = Bytes(timer_a);
    auto* rtc_overlap = reinterpret_cast<nx_rtc_request_t*>(
        reinterpret_cast<unsigned char*>(&rtc_a) + sizeof(rtc_a) - 8);
    auto* timer_overlap = reinterpret_cast<nx_lptim_request_t*>(
        reinterpret_cast<unsigned char*>(&timer_a) + 8);
    EXPECT_EQ(nx_rtc_port_submit(&rtc, rtc_overlap), NX_ERROR_INVALID);
    EXPECT_EQ(nx_lptim_port_submit(&timer, timer_overlap), NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_rtc_port_read(&rtc, reinterpret_cast<nx_rtc_snapshot_t*>(&rtc_a)),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_lptim_port_sample(
                  &timer, reinterpret_cast<nx_lptim_sample_t*>(&timer_a)),
              NX_ERROR_INVALID);
    EXPECT_EQ(Bytes(rtc_a), rtc_before);
    EXPECT_EQ(Bytes(timer_a), timer_before);
    EXPECT_EQ(nx_rtc_port_submit(
                  &rtc, reinterpret_cast<nx_rtc_request_t*>(UINTPTR_MAX - 7)),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_lptim_port_submit(
                  &timer, reinterpret_cast<nx_lptim_request_t*>(
                              reinterpret_cast<uintptr_t>(&shots[0]) + 1)),
              NX_ERROR_INVALID);
}

TEST_F(NativeTime, ActiveRequestCannotBePreparedAgainOrUsedAsOutput) {
    Set(rtc);
    Alarm(0, rtc);
    Shot(0, timer);
    ASSERT_EQ(nx_rtc_port_submit(&rtc, &alarms[0]), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_submit(&timer, &shots[0]), NX_SUCCESS);
    nx_rtc_snapshot_t snapshot{};
    nx_lptim_sample_t sample{};
    ASSERT_EQ(nx_rtc_port_read(&rtc, &snapshot), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_sample(&timer, &sample), NX_SUCCESS);
    auto alarm_before = Bytes(alarms[0]);
    auto shot_before = Bytes(shots[0]);
    const nx_rtc_alarm_t value{1, 0, 0, 2};
    EXPECT_EQ(nx_rtc_request_prepare(&alarms[0], &snapshot.domain, &value, 3),
              NX_ERROR_STATE);
    EXPECT_EQ(nx_lptim_request_prepare(&shots[0], &sample.domain, 3, 3),
              NX_ERROR_STATE);
    EXPECT_EQ(nx_rtc_port_read(
                  &rtc, reinterpret_cast<nx_rtc_snapshot_t*>(&alarms[0])),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_lptim_port_sample(
                  &timer, reinterpret_cast<nx_lptim_sample_t*>(&shots[0])),
              NX_ERROR_INVALID);
    EXPECT_EQ(Bytes(alarms[0]), alarm_before);
    EXPECT_EQ(Bytes(shots[0]), shot_before);
    ASSERT_EQ(nx_rtc_port_cancel(&rtc, &alarms[0]), NX_SUCCESS);
    ASSERT_EQ(nx_lptim_port_cancel(&timer, &shots[0]), NX_SUCCESS);
}

TEST_F(NativeTime, InvalidAndMissingMethodsDoNotWriteOutputOrEnterProvider) {
    const nx_rtc_ops_t rtc_methods{};
    const nx_lptim_ops_t timer_methods{};
    int context = 0;
    const nx_rtc_port_t empty_rtc{&rtc_methods, &context};
    const nx_lptim_port_t empty_timer{&timer_methods, &context};
    nx_rtc_snapshot_t snapshot{};
    nx_lptim_sample_t sample{};
    auto before = Bytes(snapshot);
    auto sample_before = Bytes(sample);
    EXPECT_EQ(nx_rtc_port_read(nullptr, &snapshot), NX_ERROR_INVALID);
    EXPECT_EQ(nx_rtc_port_read(&empty_rtc, &snapshot), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_lptim_port_sample(&empty_timer, &sample),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_rtc_port_service(&empty_rtc), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_lptim_port_stop(&empty_timer), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(Bytes(snapshot), before);
    EXPECT_EQ(Bytes(sample), sample_before);
    EXPECT_EQ(nx_native_rtc_model_advance(&empty_rtc, 1), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_native_lptim_model_advance(&empty_timer, 1),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_rtc_port_read(&rtc, reinterpret_cast<nx_rtc_snapshot_t*>(
                                         const_cast<nx_rtc_port_t*>(&rtc))),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_lptim_port_sample(&timer, reinterpret_cast<nx_lptim_sample_t*>(
                                               const_cast<nx_lptim_ops_t*>(
                                                   &nx_native_lptim_ops))),
              NX_ERROR_INVALID);
}

/** \brief A substitute provider receives exactly the immutable face context. */
class Alternate {
  public:
    MOCK_METHOD(nx_result_t, Read, (nx_rtc_snapshot_t*));
};

nx_result_t AlternateRead(const void* context, nx_rtc_snapshot_t* output) {
    return const_cast<Alternate*>(static_cast<const Alternate*>(context))
        ->Read(output);
}

TEST(NativeTimeDispatch, SharedReadonlyTableSupportsAnUnrelatedProvider) {
    testing::StrictMock<Alternate> alternate;
    const nx_rtc_ops_t operations{AlternateRead, nullptr, nullptr,
                                  nullptr,       nullptr, nullptr};
    const nx_rtc_port_t face{&operations, &alternate};
    nx_rtc_snapshot_t snapshot{};
    EXPECT_CALL(alternate, Read(&snapshot))
        .WillOnce(testing::Return(NX_ERROR_STATE));
    EXPECT_EQ(nx_rtc_port_read(&face, &snapshot), NX_ERROR_STATE);
}

} /* namespace */
