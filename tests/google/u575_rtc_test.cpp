/**
 * \file            u575_rtc_test.cpp
 * \brief           Real retained-calendar provider ownership regressions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "u575_rtc_model.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <unistd.h>
using testing::Return;
using testing::StrictMock;
class Clock {
  public:
    MOCK_METHOD(nx_time_us_t, read, ());
    static nx_time_us_t sample(void* raw) {
        return static_cast<Clock*>(raw)->read();
    }
};
class U575Rtc : public testing::Test {
  protected:
    nx_stm32u575_rtc_state_t state{};
    nx_rtc_port_t port{&nx_stm32u575_rtc_ops, &state};
    nx_rtc_request_t request{};
    StrictMock<Clock> clock;
    nx_clock_t face{Clock::sample, &clock};
    void SetUp() override {
        nx_u575_rtc_model_reset();
        state.registers = &g_u575_rtc_model.rtc;
        state.power = &g_u575_rtc_model.power;
        nx_request_initialize(&request.base);
    }
    void open() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_rtc_initialize(&state, &face));
    }
    void prepare(nx_time_us_t deadline = NX_DEADLINE_NEVER) {
        nx_rtc_snapshot_t snapshot{};
        ASSERT_EQ(NX_SUCCESS, nx_rtc_port_read(&port, &snapshot));
        nx_rtc_alarm_t alarm{1, 0, 0, 0};
        ASSERT_EQ(NX_SUCCESS, nx_rtc_request_prepare(&request, &snapshot.domain,
                                                     &alarm, deadline));
    }
    void clock_steps(unsigned maximum = 20U) {
        EXPECT_CALL(clock, read())
            .Times(testing::AtMost(maximum))
            .WillRepeatedly([] { return g_u575_rtc_model.now; });
    }
    void arm() {
        ASSERT_EQ(NX_SUCCESS, nx_rtc_port_submit(&port, &request));
        clock_steps();
        for (unsigned i = 0; i < 6U; ++i) {
            EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
            nx_u575_rtc_model_step(100U);
        }
        ASSERT_TRUE(g_u575_rtc_model.source_running);
    }
    void drain() {
        for (unsigned i = 0;
             i < 12U && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
             ++i) {
            (void)nx_rtc_port_service(&port);
            nx_u575_rtc_model_step(100U);
        }
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    }
};
TEST_F(U575Rtc, AdmissionReadsOnlyAndPreservesRetainedRegisters) {
    RTC_TypeDef original = g_u575_rtc_model.rtc;
    open();
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
    EXPECT_EQ(0,
              std::memcmp(&original, &g_u575_rtc_model.rtc, sizeof(original)));
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_rtc_initialize(&state, &face));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_stop(&port));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575Rtc,
       PreflightBeforeSystemStartRequiresPreparationAndRejectsLiveAlarm) {
    g_nx_stm32u575_system.started = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_rtc_preflight());
    g_u575_rtc_model.rtc.CR |= RTC_CR_ALRAE;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_rtc_preflight());
    g_u575_rtc_model.rtc.CR = RTC_CR_BYPSHAD;
    g_u575_rtc_model.rcc.APB3ENR = 0U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_rtc_preflight());
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575Rtc, RejectsForeignConfigurationWithoutWrites) {
    g_u575_rtc_model.rtc.CR |= RTC_CR_ALRBIE;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_rtc_initialize(&state, &face));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(U575Rtc, PreparedClockPowerAndQuietVectorAreRequired) {
    g_u575_rtc_model.power.DBPR = 0U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_rtc_initialize(&state, &face));
    g_u575_rtc_model.power.DBPR = PWR_DBPR_DBP;
    g_u575_rtc_model.irq_pending = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_rtc_initialize(&state, &face));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575Rtc, StableBypassReadRetriesAndNeverPublishesIncoherentCalendar) {
    open();
    g_u575_rtc_model.calendar_race_reads = 6U;
    nx_rtc_snapshot_t snapshot{};
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_read(&port, &snapshot));
    EXPECT_EQ(99U, snapshot.calendar.year);
    EXPECT_EQ(31U, snapshot.calendar.day);
    snapshot.calendar.year = 42;
    g_u575_rtc_model.unstable_calendar = true;
    auto reads = g_u575_rtc_model.calendar_reads;
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_read(&port, &snapshot));
    EXPECT_LE(g_u575_rtc_model.calendar_reads - reads, 48U);
    EXPECT_EQ(42U, snapshot.calendar.year);
}
TEST_F(U575Rtc, NaturalYearWrapPreservesDomainAndRetainedYearZeroCanReopen) {
    open();
    nx_rtc_snapshot_t before{}, after{};
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_read(&port, &before));
    nx_rtc_calendar_t wrapped{0, 1, 1, 6, 0, 0, 0};
    nx_u575_rtc_model_calendar(&wrapped);
    g_u575_rtc_model.rtc.ICSR = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_read(&port, &after));
    EXPECT_EQ(0U, after.calendar.year);
    EXPECT_EQ(before.domain.generation, after.domain.generation);
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_stop(&port));
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_rtc_initialize(&state, &face));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_read(&port, &after));
    EXPECT_GT(after.domain.generation, before.domain.generation);
}
TEST_F(U575Rtc, InvalidBcdAndAliasedOutputPreserveStorage) {
    open();
    nx_rtc_snapshot_t snapshot{};
    snapshot.calendar.year = 42;
    g_u575_rtc_model.rtc.TR = 0x2a0000U;
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_read(&port, &snapshot));
    EXPECT_EQ(42U, snapshot.calendar.year);
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_rtc_port_read(&port, reinterpret_cast<nx_rtc_snapshot_t*>(&state)));
}
TEST_F(U575Rtc, SettingRetainedCalendarIsExplicitlyUnsupported) {
    open();
    nx_rtc_calendar_t calendar{24, 2, 29, 4, 12, 0, 0};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_rtc_port_set(&port, &calendar));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575Rtc, RejectionHasNoClockReadLoanOrHardwareEffects) {
    open();
    prepare();
    ++request.domain.generation;
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_submit(&port, &request));
    --request.domain.generation;
    request.alarm = {31, 23, 59, 58};
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_submit(&port, &request));
    request.alarm = {1, 0, 0, 0};
    request.base.deadline = 10;
    EXPECT_CALL(clock, read()).WillOnce(Return(10));
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_rtc_port_submit(&port, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575Rtc, AlarmCompletesAfterActualSourceDrainAndRestoresAlarmRegisters) {
    open();
    const auto alarm = g_u575_rtc_model.rtc.ALRMAR;
    const auto subseconds = g_u575_rtc_model.rtc.ALRMASSR;
    prepare();
    arm();
    EXPECT_EQ(0x01000000U, g_u575_rtc_model.rtc.ALRMAR);
    EXPECT_EQ(0U, g_u575_rtc_model.rtc.ALRMASSR);
    EXPECT_EQ(0U, g_u575_rtc_model.rtc.CR & RTC_CR_ALRAIE);
    nx_rtc_calendar_t wrapped{0, 1, 1, 6, 0, 0, 0};
    nx_u575_rtc_model_calendar(&wrapped);
    drain();
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(1U, request.base.transferred);
    EXPECT_FALSE(g_u575_rtc_model.source_running);
    EXPECT_EQ(0U, g_u575_rtc_model.rtc.SR);
    EXPECT_EQ(alarm, g_u575_rtc_model.rtc.ALRMAR);
    EXPECT_EQ(subseconds, g_u575_rtc_model.rtc.ALRMASSR);
    EXPECT_EQ(0U, g_u575_rtc_model.forbidden_writes);
}
TEST_F(U575Rtc, InitialDisabledReadbackStillWaitsBeforeAlarmProgramming) {
    open();
    prepare();
    g_u575_rtc_model.source_running = true;
    g_u575_rtc_model.disable_at = 63U;
    arm();
    EXPECT_EQ(0U, g_u575_rtc_model.forbidden_writes);
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_cancel(&port, &request));
    drain();
}
TEST_F(U575Rtc, ProtectedHardwarePhasesUnlockBrieflyAndAlwaysRelock) {
    open();
    prepare();
    EXPECT_TRUE(g_u575_rtc_model.wpr_locked);
    EXPECT_EQ(0U, g_u575_rtc_model.wpr_unlocks);
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_submit(&port, &request));
    clock_steps(40U);
    for (unsigned i = 0; i < 6U; ++i) {
        (void)nx_rtc_port_service(&port);
        EXPECT_TRUE(g_u575_rtc_model.wpr_locked);
        nx_u575_rtc_model_step(100U);
    }
    EXPECT_TRUE(g_u575_rtc_model.source_running);
    EXPECT_GT(g_u575_rtc_model.wpr_unlocks, 0U);
    EXPECT_EQ(g_u575_rtc_model.wpr_unlocks, g_u575_rtc_model.wpr_locks);
    g_u575_rtc_model.hold_unlock = true;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_cancel(&port, &request));
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    EXPECT_TRUE(g_u575_rtc_model.wpr_locked);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    g_u575_rtc_model.hold_unlock = false;
    drain();
    EXPECT_TRUE(g_u575_rtc_model.wpr_locked);
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
}
TEST_F(U575Rtc, ArmSynchronizationDeadlineOverflowRetainsLoanAndFailsClosed) {
    open();
    prepare();
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_submit(&port, &request));
    bool overflow = false;
    EXPECT_CALL(clock, read()).WillRepeatedly([&overflow] {
        overflow = overflow || g_u575_rtc_model.source_running;
        return overflow ? UINT64_MAX - 50U : g_u575_rtc_model.now;
    });
    for (unsigned i = 0; i < 12U; ++i) {
        EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
        nx_u575_rtc_model_step(100U);
    }
    EXPECT_TRUE(state.terminal_set);
    EXPECT_EQ(NX_ERROR_IO, state.terminal);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    EXPECT_TRUE(g_u575_rtc_model.wpr_locked);
}
TEST_F(U575Rtc, CancelRetainsLoanUntilDisableAndPendingWithdrawalAreObserved) {
    open();
    prepare();
    arm();
    g_u575_rtc_model.hold_disable = true;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_cancel(&port, &request));
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    g_u575_rtc_model.hold_disable = false;
    g_u575_rtc_model.hold_clear = true;
    g_u575_rtc_model.rtc.SR |= RTC_SR_ALRAF;
    for (unsigned i = 0; i < 5U; ++i) {
        EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
        nx_u575_rtc_model_step(100U);
    }
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    g_u575_rtc_model.hold_clear = false;
    drain();
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}
TEST_F(U575Rtc, DisableReadbackDoesNotBypassClockDomainSettling) {
    open();
    prepare();
    arm();
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_cancel(&port, &request));
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    EXPECT_EQ(0U, g_u575_rtc_model.rtc.CR & RTC_CR_ALRAE);
    EXPECT_TRUE(g_u575_rtc_model.source_running);
    for (unsigned i = 0; i < 4U; ++i)
        EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    nx_u575_rtc_model_step(100U);
    drain();
    EXPECT_EQ(0U, g_u575_rtc_model.forbidden_writes);
}
TEST_F(U575Rtc, FirstObservedDeadlineWinsOverAlarmAndLaterCancel) {
    open();
    prepare(1000);
    EXPECT_CALL(clock, read()).WillOnce(Return(0));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_submit(&port, &request));
    testing::Mock::VerifyAndClearExpectations(&clock);
    clock_steps(40U);
    g_u575_rtc_model.now = 1000;
    g_u575_rtc_model.rtc.SR |= RTC_SR_ALRAF;
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_cancel(&port, &request));
    drain();
    EXPECT_EQ(NX_ERROR_TIMEOUT, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}
TEST_F(U575Rtc, StopClosesAdmissionWhileFailedRestoreRetainsBorrow) {
    open();
    prepare();
    arm();
    g_u575_rtc_model.hold_restore = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_stop(&port));
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_submit(&port, &request));
    for (unsigned i = 0; i < 8U; ++i) {
        (void)nx_rtc_port_service(&port);
        nx_u575_rtc_model_step(100U);
    }
    EXPECT_EQ(&request, state.active);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    g_u575_rtc_model.hold_restore = false;
    drain();
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_stop(&port));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_stop(&port));
    EXPECT_FALSE(state.retained);
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
}
TEST_F(U575Rtc, ForeignClockMutationQuarantinesWithoutRepairingBackupDomain) {
    open();
    prepare();
    arm();
    auto writes = g_u575_rtc_model.writes;
    g_u575_rtc_model.rcc.BDCR &= ~static_cast<uint32_t>(RCC_BDCR_LSERDY);
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_stop(&port));
    EXPECT_EQ(writes, g_u575_rtc_model.writes);
    EXPECT_EQ(&request, state.active);
}
TEST_F(U575Rtc, ReopenDoesNotWrapDomainAndStaleRequestStaysUnborrowed) {
    open();
    prepare();
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_stop(&port));
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_rtc_initialize(&state, &face));
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_submit(&port, &request));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_stop(&port));
    state.generation = UINT64_MAX;
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_stm32u575_rtc_initialize(&state, &face));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
}
TEST_F(U575Rtc,
       SettlementIsFinalRequestAccessAndForeignCancelNeverReadsIdentity) {
    open();
    long bytes = sysconf(_SC_PAGESIZE);
    ASSERT_GT(bytes, 0);
    void* page =
        mmap(nullptr, static_cast<size_t>(bytes), PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void* foreign = mmap(nullptr, static_cast<size_t>(bytes), PROT_NONE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(MAP_FAILED, page);
    ASSERT_NE(MAP_FAILED, foreign);
    auto* loan = static_cast<nx_rtc_request_t*>(page);
    nx_request_initialize(&loan->base);
    nx_rtc_snapshot_t snapshot{};
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_read(&port, &snapshot));
    nx_rtc_alarm_t alarm{1, 0, 0, 0};
    ASSERT_EQ(NX_SUCCESS, nx_rtc_request_prepare(loan, &snapshot.domain, &alarm,
                                                 NX_DEADLINE_NEVER));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_submit(&port, loan));
    EXPECT_EQ(
        NX_ERROR_STATE,
        nx_rtc_port_cancel(&port, static_cast<nx_rtc_request_t*>(foreign)));
    clock_steps();
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_cancel(&port, loan));
    for (unsigned i = 0;
         i < 12U && nx_request_state(&loan->base) != NX_REQUEST_SETTLED; ++i) {
        (void)nx_rtc_port_service(&port);
        nx_u575_rtc_model_step(100U);
    }
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&loan->base));
    ASSERT_EQ(0, mprotect(page, static_cast<size_t>(bytes), PROT_NONE));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_service(&port));
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_cancel(&port, loan));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_stop(&port));
    EXPECT_EQ(0, munmap(page, static_cast<size_t>(bytes)));
    EXPECT_EQ(0, munmap(foreign, static_cast<size_t>(bytes)));
}

TEST_F(U575Rtc, LowLseDriveRejectsBeforeBackupWritesOrBorrow) {
    g_u575_rtc_model.rcc.BDCR &= ~static_cast<uint32_t>(RCC_BDCR_LSEDRV);
    auto before = state;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_rtc_preflight());
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_rtc_initialize(&state, &face));
    EXPECT_EQ(0, std::memcmp(&before, &state, sizeof(state)));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575Rtc, MediumLowLseDriveRejectsBeforeBackupWritesOrBorrow) {
    g_u575_rtc_model.rcc.BDCR =
        (g_u575_rtc_model.rcc.BDCR & ~static_cast<uint32_t>(RCC_BDCR_LSEDRV)) |
        RCC_BDCR_LSEDRV_0;
    auto before = state;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_rtc_preflight());
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_rtc_initialize(&state, &face));
    EXPECT_EQ(0, std::memcmp(&before, &state, sizeof(state)));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575Rtc, DriveMutationQuarantinesAcceptedLoanWithoutReprogrammingLse) {
    open();
    prepare();
    arm();
    auto writes = g_u575_rtc_model.writes;
    g_u575_rtc_model.rcc.BDCR &= ~static_cast<uint32_t>(RCC_BDCR_LSEDRV);
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_stop(&port));
    EXPECT_EQ(writes, g_u575_rtc_model.writes);
}

TEST_F(U575Rtc, BypassedLseIsOutsideExactCrystalReference) {
    g_u575_rtc_model.rcc.BDCR |= RCC_BDCR_LSEBYP;
    auto before = state;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_rtc_preflight());
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_rtc_initialize(&state, &face));
    EXPECT_EQ(0, std::memcmp(&before, &state, sizeof(state)));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}

TEST_F(U575Rtc, UnqualifiedApbProfileRejectsBeforeCalendarTuple) {
    open();
    nx_rtc_snapshot_t out{};
    out.calendar.year = 73U;
    g_u575_rtc_model.calendar_reads = 0U;
    g_u575_rtc_model.rcc.CFGR2 = RCC_CFGR2_HPRE;
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_read(&port, &out));
    EXPECT_EQ(73U, out.calendar.year);
    EXPECT_EQ(0U, g_u575_rtc_model.calendar_reads);
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}

TEST_F(U575Rtc, ColdPrepareBindsReviewedResourcesWithoutResettingGeneration) {
    state = {};
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_rtc_prepare(&state));
    EXPECT_EQ(&g_u575_rtc_model.rtc, state.registers);
    EXPECT_EQ(&g_u575_rtc_model.power, state.power);
    const auto generation = state.generation;
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_stop(&port));
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_rtc_prepare(&state));
    EXPECT_GT(state.generation, generation);
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_stop(&port));
}
