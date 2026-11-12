/**
 * \file            f407_rtc_test.cpp
 * \brief           Exact F407 backup provenance and alarm loan regressions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "f407_rtc_model.h"
#include "nexus/io/rtc_aux.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
extern "C" nx_stm32_system_t g_nx_stm32_system;
class F407Rtc : public testing::Test {
  protected:
    nx_stm32_rtc_state_t state{};
    nx_rtc_port_t port{&nx_stm32_rtc_ops, &state};
    nx_stm32_rtc_config_t config{};
    nx_rtc_request_t request{};
    nx_rtc_calendar_t initial{24U, 2U, 29U, 4U, 12U, 34U, 56U};
    static nx_time_us_t now(void*) {
        return g_f407_rtc_model.now++;
    }
    void SetUp() override {
        nx_f407_rtc_model_reset(true);
        config.clock = {now, nullptr};
        config.timeout_us = 100U;
        config.poll_limit = 32U;
        config.policy = NX_STM32_RTC_ADOPT_RETAINED;
        nx_request_initialize(&request.base);
    }
    void open() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_prepare(&state, &config));
    }
    void provision() {
        nx_f407_rtc_model_reset(false);
        config.policy = NX_STM32_RTC_PROVISION_EMPTY;
        config.has_initial = true;
        config.initial = initial;
    }
    void prepare(nx_time_us_t deadline = NX_DEADLINE_NEVER) {
        nx_rtc_snapshot_t s{};
        ASSERT_EQ(NX_SUCCESS, nx_rtc_port_read(&port, &s));
        nx_rtc_alarm_t alarm{1U, 0U, 0U, 0U};
        ASSERT_EQ(NX_SUCCESS, nx_rtc_request_prepare(&request, &s.domain,
                                                     &alarm, deadline));
    }
    void arm() {
        ASSERT_EQ(NX_SUCCESS, nx_rtc_port_submit(&port, &request));
        for (unsigned i = 0; i != 5U; ++i)
            (void)nx_rtc_port_service(&port);
        ASSERT_NE(0U, g_f407_rtc_model.rtc.CR & RTC_CR_ALRAE);
    }
    void drain() {
        for (unsigned i = 0;
             i != 14U && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
             ++i)
            (void)nx_rtc_port_service(&port);
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    }
    nx_result_t result() {
        nx_result_t r = NX_ERROR_STATE;
        size_t count = 0U;
        EXPECT_EQ(NX_SUCCESS, nx_request_result(&request.base, &r, &count));
        EXPECT_EQ(r == NX_SUCCESS ? 1U : 0U, count);
        return r;
    }
};
namespace {
using RtcWriteObserver =
    testing::MockFunction<void(volatile uint32_t*, uint32_t)>;
RtcWriteObserver* standby_observer;
} /* namespace */
class F407RtcStandby : public F407Rtc {
  protected:
    nx_rtc_standby_t receipt{};
    uint32_t saved_cr{};
    uint32_t saved_tafcr{};
    static constexpr uint32_t events =
        NX_RTC_EVENT_ALARM_B | NX_RTC_EVENT_WAKEUP | NX_RTC_EVENT_TIMESTAMP |
        NX_RTC_EVENT_TAMPER1;
    static constexpr uint32_t lines = (1U << 17U) | (1U << 21U) | (1U << 22U);
    static constexpr uint32_t cr_mask =
        RTC_CR_ALRAE | RTC_CR_ALRBE | RTC_CR_WUTE | RTC_CR_TSE | RTC_CR_ALRAIE |
        RTC_CR_ALRBIE | RTC_CR_WUTIE | RTC_CR_TSIE;
    void suspended() {
        config.lse_bypass = true;
        g_f407_rtc_model.rcc.BDCR |= RCC_BDCR_LSEBYP;
        g_f407_rtc_model.gpio_c.MODER = 0U;
        open();
        nx_rtc_alarm_t alarm{2U, 3U, 4U, 5U};
        ASSERT_EQ(NX_SUCCESS, nx_rtc_port_alarm_b(&port, true, &alarm));
        nx_rtc_wakeup_t wake{true, NX_RTC_WAKE_SECOND, 1U};
        ASSERT_EQ(NX_SUCCESS, nx_rtc_port_wakeup(&port, &wake));
        nx_rtc_capture_config_t capture{};
        capture.timestamp_enabled = true;
        capture.timestamp_pin = 1U;
        capture.tamper1_pin = 1U;
        capture.tamper_mask = 1U;
        ASSERT_EQ(NX_SUCCESS, nx_rtc_port_capture_configure(&port, &capture));
        ASSERT_EQ(NX_SUCCESS, nx_rtc_port_wake_route(&port, events));
        ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_suspend(&state));
        saved_cr = g_f407_rtc_model.rtc.CR;
        saved_tafcr = g_f407_rtc_model.rtc.TAFCR;
    }
    void TearDown() override {
        g_f407_rtc_model.observe_write = nullptr;
        standby_observer = nullptr;
    }
};
TEST_F(F407RtcStandby,
       PrepareDisablesSourcesPreservingRoutesBackupAndCalendar) {
    suspended();
    auto& m = g_f407_rtc_model;
    m.rtc.BKP0R = 0x12345678U;
    const auto time = m.rtc.TR;
    const auto date = m.rtc.DR;
    const auto prer = m.rtc.PRER;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_standby_prepare(&port, 0U, 8U, &receipt));
    EXPECT_EQ(&state, receipt.owner);
    EXPECT_NE(0U, receipt.epoch);
    EXPECT_EQ(0U, m.rtc.CR & cr_mask);
    EXPECT_EQ(0U, m.rtc.TAFCR &
                      (RTC_TAFCR_TAMP1E | RTC_TAFCR_TAMP2E | RTC_TAFCR_TAMPIE));
    EXPECT_EQ(lines, m.exti.EMR);
    EXPECT_EQ(lines, m.exti.RTSR);
    EXPECT_EQ(0U, m.exti.IMR | m.exti.FTSR);
    EXPECT_EQ(time, m.rtc.TR);
    EXPECT_EQ(date, m.rtc.DR);
    EXPECT_EQ(prer, m.rtc.PRER);
    EXPECT_EQ(0x12345678U, m.rtc.BKP0R);
    EXPECT_EQ(0U, m.backup_resets);
    auto writes = m.writes;
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_standby_validate(&port, &receipt));
    EXPECT_EQ(writes, m.writes);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_close(&state));
}
TEST_F(F407RtcStandby, UnauthorizedPendingEventsAreNotSilentlyDiscarded) {
    suspended();
    auto& m = g_f407_rtc_model;
    m.rtc.ISR |= RTC_ISR_ALRBF;
    m.exti.PR |= 1U << 17U;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_rtc_port_standby_prepare(&port, 0U, 8U, &receipt));
    EXPECT_NE(0U, m.rtc.ISR & RTC_ISR_ALRBF);
    EXPECT_NE(0U, m.exti.PR & (1U << 17U));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_standby_restore(&port, &receipt));
    EXPECT_EQ(saved_cr, m.rtc.CR);
}
TEST_F(F407RtcStandby,
       DisableClearReenableOrderingAndCommitPreservesNewEvents) {
    suspended();
    auto& m = g_f407_rtc_model;
    const uint32_t flags =
        RTC_ISR_ALRBF | RTC_ISR_WUTF | RTC_ISR_TSF | RTC_ISR_TAMP1F;
    m.rtc.ISR |= flags;
    m.exti.PR = lines;
    testing::NiceMock<RtcWriteObserver> observer;
    standby_observer = &observer;
    m.observe_write = [](volatile uint32_t* p, uint32_t value) {
        standby_observer->Call(p, value);
    };
    EXPECT_CALL(observer, Call(testing::_, testing::_))
        .Times(testing::AnyNumber());
    {
        testing::InSequence order;
        EXPECT_CALL(observer, Call(&m.rtc.CR, saved_cr & ~cr_mask));
        EXPECT_CALL(observer, Call(&m.rtc.TAFCR,
                                   saved_tafcr & ~uint32_t{RTC_TAFCR_TAMP1E |
                                                           RTC_TAFCR_TAMP2E |
                                                           RTC_TAFCR_TAMPIE}));
        EXPECT_CALL(observer,
                    Call(&m.rtc.ISR, testing::Truly([flags](uint32_t value) {
                        return (value & flags) == 0U;
                    })));
        EXPECT_CALL(observer, Call(&m.exti.PR, lines));
        EXPECT_CALL(observer, Call(&m.rtc.TAFCR, saved_tafcr));
        EXPECT_CALL(observer, Call(&m.rtc.CR, saved_cr)).RetiresOnSaturation();
    }
    ASSERT_EQ(NX_SUCCESS,
              nx_rtc_port_standby_prepare(&port, events, 8U, &receipt));
    EXPECT_EQ(0U, m.rtc.ISR & flags);
    EXPECT_EQ(0U, m.exti.PR & lines);
    m.rtc.ISR |= RTC_ISR_TSF | RTC_ISR_TSOVF;
    m.exti.PR |= 1U << 21U;
    m.masked = true;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_standby_commit(&port, &receipt));
    EXPECT_EQ(saved_cr, m.rtc.CR);
    EXPECT_EQ(saved_tafcr, m.rtc.TAFCR);
    EXPECT_EQ(RTC_ISR_TSF | RTC_ISR_TSOVF,
              m.rtc.ISR & (RTC_ISR_TSF | RTC_ISR_TSOVF));
    EXPECT_NE(0U, m.exti.PR & (1U << 21U));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_standby_restore(&port, &receipt));
    EXPECT_EQ(nullptr, receipt.owner);
    EXPECT_EQ(0U, receipt.epoch);
    EXPECT_NE(0U, m.rtc.ISR & RTC_ISR_TSF);
    EXPECT_NE(0U, m.exti.PR & (1U << 21U));
}
TEST_F(F407RtcStandby, PreparedReceiptUsesIndependentClockWithoutOldCoreReads) {
    suspended();
    config.clock.read = [](void*) -> nx_time_us_t {
        ADD_FAILURE() << "old Core clock used";
        return NX_DEADLINE_NEVER;
    };
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_standby_prepare(&port, 0U, 8U, &receipt));
    g_nx_stm32_system.started = false;
    g_f407_rtc_model.now = NX_DEADLINE_NEVER;
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_standby_validate(&port, &receipt));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_standby_restore(&port, &receipt));
    EXPECT_EQ(saved_cr, g_f407_rtc_model.rtc.CR);
}
TEST_F(F407RtcStandby, FailedDisableReadbackRetainsReceiptForExplicitRestore) {
    suspended();
    auto& m = g_f407_rtc_model;
    m.hold_write = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_rtc_port_standby_prepare(&port, 0U, 8U, &receipt));
    EXPECT_EQ(&state, receipt.owner);
    EXPECT_NE(0U, nx_stm32_rtc_effects(&state));
    m.hold_write = false;
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_standby_restore(&port, &receipt));
    EXPECT_EQ(saved_cr, m.rtc.CR);
    EXPECT_EQ(nullptr, receipt.owner);
}
TEST_F(F407RtcStandby,
       FixedPollExhaustionRetainsDisabledSourcesWithoutDeadline) {
    suspended();
    auto& m = g_f407_rtc_model;
    m.hold_standby_flags = true;
    config.clock.read = [](void*) -> nx_time_us_t {
        ADD_FAILURE() << "deadline read in poll";
        return 0U;
    };
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_rtc_port_standby_prepare(&port, 0U, 3U, &receipt));
    EXPECT_EQ(&state, receipt.owner);
    EXPECT_EQ(0U, m.rtc.CR & cr_mask);
    m.hold_standby_flags = false;
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_standby_restore(&port, &receipt));
}
TEST_F(F407RtcStandby,
       CommitFailureCannotReleaseReceiptAndRestoreKeepsWakeFlags) {
    suspended();
    auto& m = g_f407_rtc_model;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_standby_prepare(&port, 0U, 8U, &receipt));
    m.masked = true;
    m.hold_write = true;
    EXPECT_EQ(NX_ERROR_IO, nx_rtc_port_standby_commit(&port, &receipt));
    EXPECT_EQ(&state, receipt.owner);
    m.rtc.ISR |= RTC_ISR_WUTF;
    m.exti.PR |= 1U << 22U;
    m.hold_write = false;
    g_nx_stm32_system.started = false;
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_standby_restore(&port, &receipt));
    EXPECT_EQ(nullptr, receipt.owner);
    EXPECT_NE(0U, m.rtc.ISR & RTC_ISR_WUTF);
    EXPECT_NE(0U, m.exti.PR & (1U << 22U));
    m.masked = false;
    g_nx_stm32_system.started = true;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_resume(&state));
}
TEST_F(F407RtcStandby, CopiesStaleEpochsAndUnmaskedCommitHaveNoEffects) {
    suspended();
    auto& m = g_f407_rtc_model;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_standby_prepare(&port, 0U, 8U, &receipt));
    const auto writes = m.writes;
    auto copy = receipt;
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_standby_validate(&port, &copy));
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_standby_restore(&port, &copy));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_rtc_port_standby_commit(&port, &receipt));
    ++receipt.epoch;
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_standby_validate(&port, &receipt));
    --receipt.epoch;
    EXPECT_EQ(writes, m.writes);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_resume(&state));
}
TEST_F(F407RtcStandby, AutonomousClockOrRouteDriftRejectsReadonlyValidation) {
    suspended();
    auto& m = g_f407_rtc_model;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_standby_prepare(&port, 0U, 8U, &receipt));
    const auto backup = m.rcc.BDCR;
    const auto writes = m.writes;
    m.rcc.BDCR = (m.rcc.BDCR & ~uint32_t{RCC_BDCR_RTCSEL}) | RCC_BDCR_RTCSEL;
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_standby_validate(&port, &receipt));
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_standby_restore(&port, &receipt));
    m.rcc.BDCR = backup;
    m.exti.EMR = 0U;
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_standby_validate(&port, &receipt));
    EXPECT_EQ(writes, m.writes);
}
TEST_F(F407RtcStandby, PendingExtiWithoutRtcFlagStillNeedsExplicitDiscard) {
    suspended();
    auto& m = g_f407_rtc_model;
    m.exti.PR = 1U << 22U;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_rtc_port_standby_prepare(&port, 0U, 8U, &receipt));
    EXPECT_NE(0U, m.exti.PR & (1U << 22U));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_standby_restore(&port, &receipt));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_standby_prepare(
                              &port, NX_RTC_EVENT_WAKEUP, 8U, &receipt));
    EXPECT_EQ(0U, m.exti.PR & (1U << 22U));
}
TEST_F(F407RtcStandby, LateUnauthorizedEventSurvivesPrepareFailureAndRestore) {
    suspended();
    auto& m = g_f407_rtc_model;
    m.rtc.ISR |= RTC_ISR_WUTF;
    m.exti.PR = 1U << 22U;
    m.observe_write = [](volatile uint32_t* p, uint32_t) {
        if (p == &g_f407_rtc_model.exti.PR) {
            g_f407_rtc_model.rtc.ISR |= RTC_ISR_TSF;
            g_f407_rtc_model.exti.PR |= 1U << 21U;
        }
    };
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_standby_prepare(
                                 &port, NX_RTC_EVENT_WAKEUP, 8U, &receipt));
    EXPECT_EQ(&state, receipt.owner);
    EXPECT_NE(0U, m.rtc.ISR & RTC_ISR_TSF);
    EXPECT_NE(0U, m.exti.PR & (1U << 21U));
    m.observe_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_standby_restore(&port, &receipt));
    EXPECT_NE(0U, m.rtc.ISR & RTC_ISR_TSF);
    EXPECT_NE(0U, m.exti.PR & (1U << 21U));
}
TEST_F(F407RtcStandby,
       ContextBoundsAliasesAndEpochExhaustionRejectBeforeEffects) {
    open();
    auto& m = g_f407_rtc_model;
    auto writes = m.writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_rtc_port_standby_prepare(&port, 0U, 8U, &receipt));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_rtc_port_standby_prepare(&port, 128U, 8U, &receipt));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_rtc_port_standby_prepare(&port, 0U, 0U, &receipt));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_rtc_port_standby_prepare(
                  &port, 0U, 8U, reinterpret_cast<nx_rtc_standby_t*>(&state)));
    EXPECT_EQ(writes, m.writes);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_suspend(&state));
    state.standby_epoch = UINT64_MAX;
    EXPECT_EQ(NX_ERROR_EXHAUSTED,
              nx_rtc_port_standby_prepare(&port, 0U, 8U, &receipt));
    EXPECT_EQ(writes, m.writes);
    EXPECT_EQ(nullptr, receipt.owner);
}

TEST_F(F407RtcStandby, CommitRejectsBasepriEvenWhenPrimaskIsSet) {
    suspended();
    auto& m = g_f407_rtc_model;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_standby_prepare(&port, 0U, 8U, &receipt));
    m.masked = true;
    m.basepri = 0x50U;
    const auto writes = m.writes;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_rtc_port_standby_commit(&port, &receipt));
    EXPECT_EQ(writes, m.writes);
    EXPECT_EQ(&state, receipt.owner);
    EXPECT_EQ(0U, m.rtc.CR & cr_mask);
}
TEST_F(F407RtcStandby, CommitRejectsFaultmaskEvenWhenPrimaskIsSet) {
    suspended();
    auto& m = g_f407_rtc_model;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_standby_prepare(&port, 0U, 8U, &receipt));
    m.masked = true;
    m.faultmask = 1U;
    const auto writes = m.writes;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_rtc_port_standby_commit(&port, &receipt));
    EXPECT_EQ(writes, m.writes);
    EXPECT_EQ(&state, receipt.owner);
    EXPECT_EQ(0U, m.rtc.CR & cr_mask);
}

TEST_F(F407RtcStandby, ReenablingEdgeTamperChecksPadAndRetainsFailedReceipt) {
    suspended();
    auto& m = g_f407_rtc_model;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_standby_prepare(&port, 0U, 8U, &receipt));
    const auto reads = m.capture_pad_reads;
    m.gpio_c.IDR = 1U << 13U;
    m.masked = true;
    EXPECT_EQ(NX_ERROR_IO, nx_rtc_port_standby_commit(&port, &receipt));
    EXPECT_EQ(&state, receipt.owner);
    EXPECT_TRUE(state.suspended);
    EXPECT_NE(0U, m.rtc.TAFCR & RTC_TAFCR_TAMP1E);
    EXPECT_EQ(reads + 1U, m.capture_pad_reads);
    m.masked = false;
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_backup_write(&port, 0U, 42U));
    m.masked = true;
    EXPECT_EQ(NX_ERROR_IO, nx_rtc_port_standby_restore(&port, &receipt));
    EXPECT_EQ(&state, receipt.owner);
    m.gpio_c.IDR = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_standby_restore(&port, &receipt));
    EXPECT_EQ(nullptr, receipt.owner);
    EXPECT_EQ(0U, m.capture_unclocked_reads);
}

TEST_F(F407Rtc, AuxiliaryAlarmBUsesOwnWriteFlagAndPreservesAlarmA) {
    open();
    prepare();
    arm();
    const auto alarm_a = g_f407_rtc_model.rtc.ALRMAR;
    nx_rtc_alarm_t pattern{2U, 3U, 4U, 5U};
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_alarm_b(&port, true, &pattern));
    EXPECT_EQ(0x02030405U, g_f407_rtc_model.rtc.ALRMBR);
    EXPECT_NE(0U, g_f407_rtc_model.rtc.CR & RTC_CR_ALRBE);
    EXPECT_NE(0U, g_f407_rtc_model.rtc.CR & RTC_CR_ALRAE);
    EXPECT_EQ(alarm_a, g_f407_rtc_model.rtc.ALRMAR);
    g_f407_rtc_model.rtc.ISR |= RTC_ISR_ALRBF;
    uint32_t events = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_events(&port, &events));
    EXPECT_EQ(NX_RTC_EVENT_ALARM_B, events);
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_acknowledge(&port, NX_RTC_EVENT_ALARM_B));
    EXPECT_EQ(0U, g_f407_rtc_model.rtc.ISR & RTC_ISR_ALRBF);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
}
TEST_F(F407Rtc, WakeupRejectsForbiddenFastZeroBeforeWritesAndUsesWutwf) {
    open();
    const auto writes = g_f407_rtc_model.writes;
    nx_rtc_wakeup_t wake{true, NX_RTC_WAKE_DIV2, 0U};
    EXPECT_EQ(NX_ERROR_INVALID, nx_rtc_port_wakeup(&port, &wake));
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
    wake = {true, NX_RTC_WAKE_SECOND_EXTENDED, 12U};
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_wakeup(&port, &wake));
    EXPECT_EQ(12U, g_f407_rtc_model.rtc.WUTR);
    EXPECT_EQ(RTC_CR_WUTE | 6U,
              g_f407_rtc_model.rtc.CR & (RTC_CR_WUTE | RTC_CR_WUCKSEL));
    g_f407_rtc_model.rtc.ISR |= RTC_ISR_WUTF | RTC_ISR_TAMP1F;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_acknowledge(&port, NX_RTC_EVENT_WAKEUP));
    EXPECT_EQ(0U, g_f407_rtc_model.rtc.ISR & RTC_ISR_WUTF);
    EXPECT_NE(0U, g_f407_rtc_model.rtc.ISR & RTC_ISR_TAMP1F);
}
TEST_F(F407Rtc, TimestampRetainsRawSubsecondsAndOverflowWithoutInventingYear) {
    config.lse_bypass = true;
    g_f407_rtc_model.rcc.BDCR |= RCC_BDCR_LSEBYP;
    open();
    nx_rtc_capture_config_t capture{};
    capture.timestamp_enabled = true;
    capture.timestamp_pin = capture.tamper1_pin = 1U;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_capture_configure(&port, &capture));
    nx_rtc_capture_t output{};
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_capture_read(&port, &output));
    g_f407_rtc_model.rtc.TSTR = 0x00123456U;
    g_f407_rtc_model.rtc.TSDR = 0x00008229U;
    g_f407_rtc_model.rtc.TSSSR = 127U;
    g_f407_rtc_model.rtc.ISR |= RTC_ISR_TSF | RTC_ISR_TSOVF;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_capture_read(&port, &output));
    EXPECT_EQ(2U, output.month);
    EXPECT_EQ(29U, output.day);
    EXPECT_EQ(4U, output.weekday);
    EXPECT_EQ(12U, output.hour);
    EXPECT_EQ(34U, output.minute);
    EXPECT_EQ(56U, output.second);
    EXPECT_EQ(127U, output.subseconds);
    EXPECT_EQ(256U, output.subsecond_divisor);
    EXPECT_TRUE(output.overflow);
    EXPECT_EQ(&state, output.domain.identity);
    EXPECT_NE(0U, g_f407_rtc_model.rtc.ISR & RTC_ISR_TSF);
    const auto saved = output;
    g_f407_rtc_model.rtc.TSTR = 0x002a0000U;
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_capture_read(&port, &output));
    EXPECT_EQ(0, std::memcmp(&saved, &output, sizeof(output)));
}
TEST_F(F407Rtc, BackupWordBoundsTamperAndSramLoanRetainRealResponsibility) {
    open();
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_backup_write(&port, 19U, 0x12345678U));
    uint32_t value = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_backup_read(&port, 19U, &value));
    EXPECT_EQ(0x12345678U, value);
    const auto writes = g_f407_rtc_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_rtc_port_backup_write(&port, 20U, 1U));
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
    nx_memory_window_lease_t lease{};
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_sram_acquire(&port, &lease));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_sram_prepare(&port, true));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_sram_acquire(&port, &lease));
    EXPECT_EQ(4096U, lease.bytes);
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_stop(&port));
    auto copied = lease;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_rtc_port_sram_release(&port, &copied, copied.epoch));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_sram_release(&port, &lease, lease.epoch));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
    EXPECT_EQ(0U, g_f407_rtc_model.rcc.AHB1ENR & RCC_AHB1ENR_BKPSRAMEN);
    EXPECT_EQ(0U, g_f407_rtc_model.power.CSR & PWR_CSR_BRE);
    EXPECT_EQ(0U, g_f407_rtc_model.backup_resets);
}
TEST_F(F407Rtc, CalibrationValidatesWindowBeforeWritesAndKeepsCoreSeparate) {
    open();
    const auto generation = state.generation;
    nx_rtc_calibration_t calibration{NX_RTC_CALIBRATION_SMOOTH, 3U, 8U, true,
                                     false};
    const auto writes = g_f407_rtc_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_rtc_port_calibrate(&port, &calibration));
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
    calibration.pulses = 4U;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_calibrate(&port, &calibration));
    EXPECT_EQ(RTC_CALR_CALP | RTC_CALR_CALW8 | 4U, g_f407_rtc_model.rtc.CALR);
    EXPECT_EQ(generation, state.generation);
    nx_rtc_snapshot_t snapshot{};
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_read(&port, &snapshot));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
    EXPECT_EQ(0U, g_f407_rtc_model.rtc.CALR);
}
TEST_F(F407Rtc, AuxiliaryFailureQuarantinesAlarmWithoutInventingCalendarTaint) {
    open();
    prepare();
    arm();
    nx_rtc_alarm_t pattern{2U, 3U, 4U, 5U};
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_alarm_b(&port, true, &pattern));
    g_f407_rtc_model.hold_alarm_b = true;
    EXPECT_EQ(NX_ERROR_IO, nx_rtc_port_alarm_b(&port, false, &pattern));
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    EXPECT_FALSE(state.tainted);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_close(&state));
    EXPECT_EQ(&request, state.active);
    g_f407_rtc_model.hold_alarm_b = false;
    for (unsigned i = 0; i != 20U && state.opened; ++i)
        (void)nx_stm32_rtc_close(&state);
    ASSERT_FALSE(state.opened);
    EXPECT_EQ(NX_ERROR_IO, result());
    EXPECT_EQ(0U, g_f407_rtc_model.illegal_writes);
}
TEST_F(F407Rtc, CoarseCalibrationWaitsForSmoothWithdrawalBeforeApplying) {
    config.allow_set = true;
    open();
    g_f407_rtc_model.hold_recalibration = true;
    nx_rtc_calibration_t coarse{NX_RTC_CALIBRATION_COARSE, 2U, 0U, false, true};
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_rtc_port_calibrate(&port, &coarse));
    EXPECT_EQ(0U, g_f407_rtc_model.rtc.CALIBR);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_close(&state));
    g_f407_rtc_model.hold_recalibration = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
    EXPECT_EQ(0U, g_f407_rtc_model.backup_resets);
}
TEST_F(F407Rtc, ExplicitWakeEventRoutesPreserveForeignExtiAndWithdrawOnClose) {
    open();
    g_f407_rtc_model.exti.EMR = 1U << 4U;
    g_f407_rtc_model.exti.RTSR = 1U << 4U;
    ASSERT_EQ(NX_SUCCESS,
              nx_rtc_port_wake_route(&port, NX_RTC_EVENT_ALARM_A |
                                                NX_RTC_EVENT_WAKEUP |
                                                NX_RTC_EVENT_TIMESTAMP));
    EXPECT_EQ((1U << 4U) | (1U << 17U) | (1U << 21U) | (1U << 22U),
              g_f407_rtc_model.exti.EMR);
    EXPECT_EQ(0U, g_f407_rtc_model.exti.IMR);
    EXPECT_EQ(0U, g_f407_rtc_model.irq_enabled[0] |
                      g_f407_rtc_model.irq_enabled[1] |
                      g_f407_rtc_model.irq_enabled[2]);
    prepare();
    arm();
    g_f407_rtc_model.rtc.ISR |= RTC_ISR_ALRAF;
    drain();
    EXPECT_EQ(NX_SUCCESS, result());
    ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
    EXPECT_EQ(1U << 4U, g_f407_rtc_model.exti.EMR);
    EXPECT_EQ(1U << 4U, g_f407_rtc_model.exti.RTSR);
}
TEST_F(F407Rtc, UnsupportedPackagePadAndTamperTwoRejectWithoutEffect) {
    open();
    nx_rtc_capture_config_t capture_config{};
    capture_config.timestamp_pin = capture_config.tamper1_pin = 1U;
    capture_config.tamper_mask = 2U;
    const auto writes = g_f407_rtc_model.writes;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_rtc_port_capture_configure(&port, &capture_config));
    capture_config.tamper_mask = 0U;
    capture_config.timestamp_pin = 2U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_rtc_port_capture_configure(&port, &capture_config));
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
}
TEST_F(F407Rtc, SramPowerFailureRetainsEffectsAndPreownedPowerSurvivesClose) {
    open();
    g_f407_rtc_model.hold_regulator = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_rtc_port_sram_prepare(&port, true));
    EXPECT_NE(0U, state.sram_effects);
    nx_memory_window_lease_t lease{};
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_sram_acquire(&port, &lease));
    g_f407_rtc_model.hold_sram_gate_disable = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_close(&state));
    EXPECT_NE(0U, state.sram_effects);
    g_f407_rtc_model.hold_sram_gate_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
    EXPECT_EQ(0U, state.sram_effects);
    g_f407_rtc_model.hold_regulator = false;
    g_f407_rtc_model.rcc.AHB1ENR |= RCC_AHB1ENR_BKPSRAMEN;
    g_f407_rtc_model.power.CSR |= PWR_CSR_BRE | PWR_CSR_BRR;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_prepare(&state, &config));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_sram_prepare(&port, true));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
    EXPECT_NE(0U, g_f407_rtc_model.rcc.AHB1ENR & RCC_AHB1ENR_BKPSRAMEN);
    EXPECT_NE(0U, g_f407_rtc_model.power.CSR & PWR_CSR_BRE);
}
TEST_F(F407Rtc, ProvisionCanTransferOnlyClosedJoinedAutonomousResponsibility) {
    provision();
    open();
    nx_rtc_backup_handoff_t handoff{};
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_backup_handoff(&port, &handoff));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_stop(&port));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_close(&state));
    const auto before = g_f407_rtc_model.writes;
    const auto backup = g_f407_rtc_model.rcc.BDCR;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_backup_handoff(&port, &handoff));
    EXPECT_EQ(32768U, handoff.oscillator_hz);
    EXPECT_EQ(0xbfU, handoff.resources);
    EXPECT_EQ(0, std::memcmp(&initial, &handoff.calendar, sizeof(initial)));
    EXPECT_EQ(backup, g_f407_rtc_model.rcc.BDCR);
    EXPECT_EQ(before, g_f407_rtc_model.writes);
    EXPECT_EQ(0U, nx_stm32_rtc_effects(&state));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_backup_handoff(&port, &handoff));
}
TEST_F(F407Rtc, AdoptIsReadOnlyAndCleanCloseLeavesAutonomousTime) {
    const auto rtc = g_f407_rtc_model.rtc;
    const auto backup = g_f407_rtc_model.rcc.BDCR;
    open();
    EXPECT_EQ(0U, g_f407_rtc_model.writes);
    EXPECT_EQ(0, std::memcmp(&rtc, &g_f407_rtc_model.rtc, sizeof(rtc)));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
    EXPECT_EQ(backup, g_f407_rtc_model.rcc.BDCR);
    EXPECT_EQ(0U, nx_stm32_rtc_effects(&state));
    EXPECT_EQ(0U, g_f407_rtc_model.writes);
}
TEST_F(F407Rtc, SuspendPreservesWakeAndInvalidatesAnOldCoreDeadlineToken) {
    open();
    prepare(2000U);
    nx_rtc_wakeup_t wake{true, NX_RTC_WAKE_SECOND, 1U};
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_wakeup(&port, &wake));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_wake_route(&port, NX_RTC_EVENT_WAKEUP));
    g_f407_rtc_model.rtc.ISR |= RTC_ISR_WUTF;
    const auto rtc = g_f407_rtc_model.rtc;
    const auto exti = g_f407_rtc_model.exti;
    const auto generation = state.generation;
    const auto writes = g_f407_rtc_model.writes;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_suspend(&state));
    EXPECT_TRUE(state.suspended);
    nx_rtc_snapshot_t snapshot{};
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_read(&port, &snapshot));
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_stop(&port));
    g_f407_rtc_model.now = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_resume(&state));
    EXPECT_FALSE(state.suspended);
    EXPECT_EQ(generation + 1U, state.generation);
    EXPECT_EQ(0, std::memcmp(&rtc, &g_f407_rtc_model.rtc, sizeof(rtc)));
    EXPECT_EQ(0, std::memcmp(&exti, &g_f407_rtc_model.exti, sizeof(exti)));
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_submit(&port, &request));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_read(&port, &snapshot));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
}
TEST_F(F407Rtc, SuspendRejectsLiveLoansAndResumeFailureRetainsWakeOwnership) {
    open();
    prepare();
    arm();
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_suspend(&state));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_cancel(&port, &request));
    drain();
    nx_memory_window_lease_t lease{};
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_sram_prepare(&port, true));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_sram_acquire(&port, &lease));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_suspend(&state));
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_sram_release(&port, &lease, lease.epoch));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_suspend(&state));
    const auto generation = state.generation;
    g_f407_rtc_model.rcc.BDCR &= ~uint32_t{RCC_BDCR_RTCEN};
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_rtc_resume(&state));
    EXPECT_TRUE(state.suspended);
    EXPECT_EQ(generation, state.generation);
    g_f407_rtc_model.rcc.BDCR |= RCC_BDCR_RTCEN;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_resume(&state));
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_rtc_resume(&state));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
}
TEST_F(F407Rtc,
       ProvisionSuspendRetainsEffectsAndExhaustionRejectsBeforeWrites) {
    provision();
    open();
    const auto effects = state.effects;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_suspend(&state));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_resume(&state));
    EXPECT_EQ(effects, state.effects);
    state.generation = UINT64_MAX;
    const auto writes = g_f407_rtc_model.writes;
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_stm32_rtc_suspend(&state));
    EXPECT_FALSE(state.suspended);
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
}
TEST_F(F407Rtc, LostBackupRegulatorCannotAdmitAClaimedRetainedSramLoan) {
    open();
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_sram_prepare(&port, true));
    g_f407_rtc_model.power.CSR &= ~uint32_t{PWR_CSR_BRE | PWR_CSR_BRR};
    nx_memory_window_lease_t lease{};
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_sram_acquire(&port, &lease));
    if (lease.owner != nullptr) {
        EXPECT_EQ(NX_SUCCESS,
                  nx_rtc_port_sram_release(&port, &lease, lease.epoch));
    }
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
}
TEST_F(F407Rtc, ForeignClockCalendarOrInterruptRejectBeforeEffects) {
    for (unsigned condition = 0U; condition != 7U; ++condition) {
        nx_f407_rtc_model_reset(true);
        if (condition == 0U)
            g_f407_rtc_model.rcc.BDCR |= RCC_BDCR_LSEBYP;
        if (condition == 1U)
            g_f407_rtc_model.rtc.CR |= RTC_CR_ALRBIE;
        if (condition == 2U)
            g_f407_rtc_model.rtc.PRER = 0U;
        if (condition == 3U)
            g_f407_rtc_model.power.CR = 0U;
        if (condition == 4U)
            g_f407_rtc_model.irq_busy = true;
        if (condition == 5U)
            g_f407_rtc_model.exti.IMR = 1U << 17U;
        if (condition == 6U)
            g_f407_rtc_model.rtc.TAFCR = 1U;
        EXPECT_NE(NX_SUCCESS, nx_stm32_rtc_prepare(&state, &config));
        EXPECT_EQ(0U, g_f407_rtc_model.writes);
        EXPECT_FALSE(state.opened);
    }
}
TEST_F(F407Rtc, ProvisionRequiresExplicitValidCalendarBeforeAnyEffect) {
    provision();
    config.has_initial = false;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_rtc_prepare(&state, &config));
    config.has_initial = true;
    config.initial.year = 23U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_EQ(0U, g_f407_rtc_model.writes);
}
TEST_F(F407Rtc, ProvisionRejectsForeignBackupWithoutResettingIt) {
    provision();
    g_f407_rtc_model.rtc.DR = 0x00240101U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_EQ(0U, g_f407_rtc_model.writes);
    EXPECT_EQ(0U, g_f407_rtc_model.backup_resets);
}
TEST_F(F407Rtc, ExplicitProvisionWritesBothPrescalersAndRetainsClockReceipt) {
    provision();
    open();
    EXPECT_EQ(2U, g_f407_rtc_model.prer_writes);
    EXPECT_EQ(255U, g_f407_rtc_model.prer_values[0] & RTC_PRER_PREDIV_S);
    EXPECT_EQ(0x007f00ffU, g_f407_rtc_model.rtc.PRER);
    nx_rtc_snapshot_t s{};
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_read(&port, &s));
    EXPECT_EQ(0, std::memcmp(&initial, &s.calendar, sizeof(initial)));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_stop(&port));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_close(&state));
    EXPECT_NE(0U, nx_stm32_rtc_effects(&state));
    EXPECT_NE(0U, g_f407_rtc_model.rcc.BDCR & RCC_BDCR_RTCEN);
    EXPECT_EQ(0U, g_f407_rtc_model.backup_resets);
    EXPECT_EQ(0U, g_f407_rtc_model.illegal_writes);
    EXPECT_TRUE(g_f407_rtc_model.locked);
}
TEST_F(F407Rtc, LseWaitIsBoundedAndRetainsOnlyOwnedEffects) {
    provision();
    g_f407_rtc_model.hold_lse = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_LE(g_f407_rtc_model.now, 101U);
    EXPECT_NE(0U, nx_stm32_rtc_effects(&state));
    EXPECT_EQ(0U, g_f407_rtc_model.rcc.BDCR & RCC_BDCR_RTCSEL);
    EXPECT_EQ(0U, g_f407_rtc_model.backup_resets);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_close(&state));
}
TEST_F(F407Rtc, InitSynchronizationTimeoutLocksWprAndRetainsOwnership) {
    provision();
    g_f407_rtc_model.hold_init = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_TRUE(g_f407_rtc_model.locked);
    EXPECT_NE(0U, nx_stm32_rtc_effects(&state));
    EXPECT_FALSE(state.opened);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_close(&state));
}
TEST_F(F407Rtc, ReadRejectsIncoherentAndMalformedBcdWithoutPublishing) {
    open();
    nx_rtc_snapshot_t out{};
    std::memset(&out, 0x5a, sizeof(out));
    const auto before = out;
    g_f407_rtc_model.incoherent = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_read(&port, &out));
    EXPECT_EQ(0, std::memcmp(&before, &out, sizeof(out)));
    g_f407_rtc_model.incoherent = false;
    g_f407_rtc_model.rtc.TR = 0x0000006aU;
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_read(&port, &out));
    EXPECT_EQ(0, std::memcmp(&before, &out, sizeof(out)));
}
TEST_F(F407Rtc, YearRolloverAndLeapDatePreserveTheSettingDomain) {
    open();
    nx_rtc_snapshot_t before{}, after{};
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_read(&port, &before));
    nx_rtc_calendar_t c{0U, 2U, 29U, 2U, 0U, 0U, 0U};
    nx_f407_rtc_model_calendar(&c);
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_read(&port, &after));
    EXPECT_EQ(before.domain.generation, after.domain.generation);
    c.year = 1U;
    nx_f407_rtc_model_calendar(&c);
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_read(&port, &after));
}
TEST_F(F407Rtc, SetIsExplicitRejectsLoansAndAdvancesDomainOnlyOnSuccess) {
    open();
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_rtc_port_set(&port, &initial));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
    config.allow_set = true;
    open();
    const auto generation = state.generation;
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_set(&port, &initial));
    EXPECT_EQ(generation + 1U, state.generation);
    prepare();
    arm();
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_set(&port, &initial));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_cancel(&port, &request));
    drain();
}
TEST_F(F407Rtc, SetFailureTaintsAdmissionAndNeverPublishesNewCalendar) {
    config.allow_set = true;
    open();
    g_f407_rtc_model.hold_write = true;
    EXPECT_EQ(NX_ERROR_IO, nx_rtc_port_set(&port, &initial));
    EXPECT_TRUE(state.tainted);
    nx_rtc_snapshot_t s{};
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_read(&port, &s));
    EXPECT_TRUE(g_f407_rtc_model.locked);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_close(&state));
}
TEST_F(F407Rtc, ExpiredAndStaleAlarmRejectWithoutRequestOrHardwareEffect) {
    open();
    prepare(0U);
    auto before = g_f407_rtc_model.writes;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_rtc_port_submit(&port, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(before, g_f407_rtc_model.writes);
    request.base.deadline = NX_DEADLINE_NEVER;
    ++request.domain.generation;
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_submit(&port, &request));
    EXPECT_EQ(before, g_f407_rtc_model.writes);
}
TEST_F(F407Rtc, AlarmEventRestoresFieldsAndDetachesBeforeSettlement) {
    g_f407_rtc_model.rtc.ALRMAR = 0x12345678U;
    g_f407_rtc_model.rtc.ALRMASSR = 0x0200000aU;
    open();
    prepare();
    arm();
    g_f407_rtc_model.rtc.ISR |= RTC_ISR_ALRAF;
    drain();
    EXPECT_EQ(NX_SUCCESS, result());
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(0x12345678U, g_f407_rtc_model.rtc.ALRMAR);
    EXPECT_EQ(0x0200000aU, g_f407_rtc_model.rtc.ALRMASSR);
    EXPECT_EQ(0U, g_f407_rtc_model.illegal_writes);
    EXPECT_TRUE(g_f407_rtc_model.locked);
}
TEST_F(F407Rtc, CancellationWinsUnobservedEventAndRetainsLoanUntilOffProof) {
    open();
    prepare();
    arm();
    g_f407_rtc_model.rtc.ISR |= RTC_ISR_ALRAF;
    g_f407_rtc_model.hold_alarm = true;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_cancel(&port, &request));
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    g_f407_rtc_model.hold_alarm = false;
    drain();
    EXPECT_EQ(NX_ERROR_CANCELLED, result());
}
TEST_F(F407Rtc, FirstObservationDeadlineWinsPendingEvent) {
    open();
    prepare(1000U);
    arm();
    g_f407_rtc_model.now = 1000U;
    g_f407_rtc_model.rtc.ISR |= RTC_ISR_ALRAF;
    drain();
    EXPECT_EQ(NX_ERROR_TIMEOUT, result());
}
TEST_F(F407Rtc, FailedFlagClearRetainsLoanAndFirstCauseUntilRecovery) {
    open();
    prepare();
    arm();
    g_f407_rtc_model.rtc.ISR |= RTC_ISR_ALRAF;
    g_f407_rtc_model.hold_clear = true;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_cancel(&port, &request));
    for (unsigned i = 0; i < 8U; ++i)
        (void)nx_rtc_port_service(&port);
    EXPECT_EQ(&request, state.active);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    g_f407_rtc_model.hold_clear = false;
    drain();
    EXPECT_EQ(NX_ERROR_CANCELLED, result());
}
TEST_F(F407Rtc, StopDrainsAndClosesWithoutChangingCalendarOrClock) {
    open();
    prepare();
    arm();
    const auto tr = g_f407_rtc_model.rtc.TR;
    const auto dr = g_f407_rtc_model.rtc.DR;
    const auto backup = g_f407_rtc_model.rcc.BDCR;
    for (unsigned i = 0; i < 15U && state.opened; ++i)
        (void)nx_rtc_port_stop(&port);
    EXPECT_FALSE(state.opened);
    EXPECT_EQ(NX_ERROR_CANCELLED, result());
    EXPECT_EQ(tr, g_f407_rtc_model.rtc.TR);
    EXPECT_EQ(dr, g_f407_rtc_model.rtc.DR);
    EXPECT_EQ(backup, g_f407_rtc_model.rcc.BDCR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
}
TEST_F(F407Rtc, ContextAndKnownAliasesRejectBeforeEffects) {
    g_f407_rtc_model.masked = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_EQ(0U, g_f407_rtc_model.writes);
    g_f407_rtc_model.masked = false;
    open();
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_rtc_port_read(&port, reinterpret_cast<nx_rtc_snapshot_t*>(&state)));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_rtc_port_set(
                  &port, reinterpret_cast<const nx_rtc_calendar_t*>(&config)));
    EXPECT_EQ(0U, g_f407_rtc_model.writes);
}
TEST_F(F407Rtc, GenerationExhaustionRejectsBeforeBackupEffects) {
    state.generation = UINT64_MAX;
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_EQ(0U, g_f407_rtc_model.writes);
}
TEST_F(F407Rtc, SetValidatesBeforeEffectsAndRejectsExhaustedGeneration) {
    config.allow_set = true;
    open();
    const auto writes = g_f407_rtc_model.writes;
    auto invalid = initial;
    invalid.year = 23U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_rtc_port_set(&port, &invalid));
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
    state.generation = UINT64_MAX;
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_rtc_port_set(&port, &initial));
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
}
TEST_F(F407Rtc, ReopenAfterAdoptCloseDoesNotRepeatTheSettingDomain) {
    open();
    auto generation = state.generation;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_EQ(generation + 1U, state.generation);
    EXPECT_EQ(0U, g_f407_rtc_model.writes);
}
TEST_F(F407Rtc, ForeignClockChangeQuarantinesUntilExplicitRecovery) {
    open();
    prepare();
    arm();
    g_f407_rtc_model.rcc.BDCR &= ~uint32_t{RCC_BDCR_RTCEN};
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    EXPECT_TRUE(state.tainted);
    EXPECT_EQ(&request, state.active);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    g_f407_rtc_model.rcc.BDCR |= RCC_BDCR_RTCEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    EXPECT_EQ(&request, state.active);
}
TEST_F(F407Rtc, RequestAliasAndAlreadyMatchingPatternRejectBeforeAdmission) {
    open();
    const auto writes = g_f407_rtc_model.writes;
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_rtc_port_submit(&port, reinterpret_cast<nx_rtc_request_t*>(&state)));
    prepare();
    request.alarm = {31U, 23U, 59U, 58U};
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_submit(&port, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
}
TEST_F(F407Rtc, AllContextRejectionsPreserveRegistersAndLoan) {
    open();
    prepare();
    arm();
    const auto writes = g_f407_rtc_model.writes;
    g_f407_rtc_model.in_isr = true;
    nx_rtc_snapshot_t s{};
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_rtc_port_read(&port, &s));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_rtc_port_set(&port, &initial));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_rtc_port_service(&port));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_rtc_port_stop(&port));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_rtc_port_cancel(&port, &request));
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
    EXPECT_EQ(&request, state.active);
}
TEST_F(F407Rtc, FiniteClockExhaustionRejectsBeforeAnyColdEffect) {
    provision();
    g_f407_rtc_model.now = NX_DEADLINE_NEVER - 1U;
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_EQ(0U, g_f407_rtc_model.writes);
    EXPECT_EQ(0U, nx_stm32_rtc_effects(&state));
}
TEST_F(F407Rtc, StalledCoreClockStillHasAFiniteColdPollBudget) {
    provision();
    config.clock.read = [](void*) -> nx_time_us_t { return 0U; };
    config.poll_limit = 3U;
    g_f407_rtc_model.hold_lse = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_FALSE(state.opened);
    EXPECT_EQ(0U, g_f407_rtc_model.backup_resets);
}
TEST_F(F407Rtc, ProvisionDoesNotInventReadyFlagWhenLseNeverStarted) {
    provision();
    g_f407_rtc_model.hold_lse = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_EQ(0U, g_f407_rtc_model.rcc.BDCR & RCC_BDCR_LSERDY);
    EXPECT_EQ(0U, g_f407_rtc_model.rtc.TR);
}
TEST_F(F407Rtc, InitEntryFailureAbortsInitAndRetainsTaintedOwnership) {
    config.allow_set = true;
    open();
    g_f407_rtc_model.hold_init = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_rtc_port_set(&port, &initial));
    EXPECT_EQ(0U, g_f407_rtc_model.rtc.ISR & RTC_ISR_INIT);
    EXPECT_TRUE(g_f407_rtc_model.locked);
    EXPECT_TRUE(state.tainted);
}
TEST_F(F407Rtc, ConfigurationCannotBorrowModeledMmioAsImmutableStorage) {
    auto* mmio_config = reinterpret_cast<nx_stm32_rtc_config_t*>(
        const_cast<uint32_t*>(&g_f407_rtc_model.rtc.BKP0R));
    static_assert(sizeof(config) <= 20U * sizeof(uint32_t));
    std::memcpy(mmio_config, &config, sizeof(config));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_rtc_prepare(&state, mmio_config));
    EXPECT_EQ(0U, g_f407_rtc_model.writes);
    EXPECT_FALSE(state.opened);
}

TEST_F(F407Rtc, EveryBackupVectorMustBeQuietBeforeAdoption) {
    for (unsigned irq = 0U; irq < 3U; ++irq) {
        for (unsigned flag = 0U; flag < 3U; ++flag) {
            nx_f407_rtc_model_reset(true);
            if (flag == 0U)
                g_f407_rtc_model.irq_enabled[irq] = 1U;
            if (flag == 1U)
                g_f407_rtc_model.irq_pending[irq] = 1U;
            if (flag == 2U)
                g_f407_rtc_model.irq_active[irq] = 1U;
            EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_prepare(&state, &config));
            EXPECT_FALSE(state.opened);
            EXPECT_EQ(0U, g_f407_rtc_model.writes);
            state = {};
        }
    }
}

TEST_F(F407Rtc, CalendarReloadMustBeObservedBeforeSuccessfulSet) {
    config.allow_set = true;
    open();
    g_f407_rtc_model.hold_reload = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_rtc_port_set(&port, &initial));
    EXPECT_TRUE(state.tainted);
    EXPECT_TRUE(g_f407_rtc_model.locked);
    EXPECT_EQ(0U, g_f407_rtc_model.rtc.ISR & RTC_ISR_INIT);
}

TEST_F(F407Rtc, CalendarAndCoarseInitReentryRequireObservedShadowSync) {
    config.allow_set = true;
    open();
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_set(&port, &initial));
    ++initial.second;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_set(&port, &initial));
    nx_rtc_calibration_t coarse{NX_RTC_CALIBRATION_COARSE, 2U, 0U, false, true};
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_calibrate(&port, &coarse));
    EXPECT_EQ(0U, g_f407_rtc_model.unsynchronized_init);
    EXPECT_GE(g_f407_rtc_model.synchronizations, 3U);
    EXPECT_NE(0U, g_f407_rtc_model.rtc.CR & RTC_CR_BYPSHAD);
    EXPECT_EQ(0U, g_f407_rtc_model.illegal_writes);
}

TEST_F(F407Rtc, ShadowSyncTimeoutNeverEntersInitOrPublishesNewDomain) {
    config.allow_set = true;
    open();
    const auto generation = state.generation;
    const auto time = g_f407_rtc_model.rtc.TR;
    const auto date = g_f407_rtc_model.rtc.DR;
    g_f407_rtc_model.hold_synchronization = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_rtc_port_set(&port, &initial));
    EXPECT_EQ(generation, state.generation);
    EXPECT_EQ(time, g_f407_rtc_model.rtc.TR);
    EXPECT_EQ(date, g_f407_rtc_model.rtc.DR);
    EXPECT_EQ(0U, g_f407_rtc_model.rtc.ISR & RTC_ISR_INIT);
    EXPECT_NE(0U, g_f407_rtc_model.rtc.CR & RTC_CR_BYPSHAD);
    EXPECT_TRUE(state.tainted);
}

TEST_F(F407Rtc, FallingEdgeTamperRejectsBeforeEffects) {
    config.lse_bypass = true;
    g_f407_rtc_model.rcc.BDCR |= RCC_BDCR_LSEBYP;
    open();
    nx_rtc_capture_config_t capture{};
    capture.timestamp_pin = capture.tamper1_pin = 1U;
    capture.tamper_mask = 1U;
    capture.trigger_mask = 1U;
    const auto writes = g_f407_rtc_model.writes;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_rtc_port_capture_configure(&port, &capture));
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
    EXPECT_EQ(0U, g_f407_rtc_model.rtc.TAFCR);
}

TEST_F(F407Rtc, RisingEdgeTamperRejectsActiveInputAndBlocksBackupWrites) {
    config.lse_bypass = true;
    auto& m = g_f407_rtc_model;
    m.rcc.BDCR |= RCC_BDCR_LSEBYP;
    m.gpio_c.MODER = 0U;
    open();
    m.gpio_c.IDR = 1U << 13U;
    m.rtc.BKP0R = 0x12345678U;
    nx_rtc_capture_config_t capture{};
    capture.timestamp_pin = capture.tamper1_pin = 1U;
    capture.tamper_mask = 1U;
    EXPECT_EQ(NX_ERROR_IO, nx_rtc_port_capture_configure(&port, &capture));
    EXPECT_TRUE(state.auxiliary_faulted);
    EXPECT_NE(0U, m.rtc.TAFCR & RTC_TAFCR_TAMP1E);
    EXPECT_EQ(1U, m.capture_pad_reads);
    EXPECT_EQ(0U, m.capture_unclocked_reads);
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_backup_write(&port, 0U, 42U));
    EXPECT_EQ(0x12345678U, m.rtc.BKP0R);
}

TEST_F(F407Rtc, RisingEdgeTamperObservesAnInputTransitionDuringEnable) {
    config.lse_bypass = true;
    auto& m = g_f407_rtc_model;
    m.rcc.BDCR |= RCC_BDCR_LSEBYP;
    m.gpio_c.MODER = 0U;
    open();
    RtcWriteObserver observer;
    standby_observer = &observer;
    m.observe_write = [](volatile uint32_t* p, uint32_t v) {
        standby_observer->Call(p, v);
    };
    EXPECT_CALL(observer, Call(testing::_, testing::_))
        .WillRepeatedly(testing::Invoke([](volatile uint32_t* p, uint32_t v) {
            auto& model = g_f407_rtc_model;
            if (p == &model.rtc.TAFCR && (v & RTC_TAFCR_TAMP1E))
                model.gpio_c.IDR |= 1U << 13U;
        }));
    nx_rtc_capture_config_t capture{};
    capture.timestamp_pin = capture.tamper1_pin = 1U;
    capture.tamper_mask = 1U;
    EXPECT_EQ(NX_ERROR_IO, nx_rtc_port_capture_configure(&port, &capture));
    EXPECT_TRUE(state.auxiliary_faulted);
    EXPECT_EQ(1U, m.capture_pad_reads);
    m.observe_write = nullptr;
    standby_observer = nullptr;
}

TEST_F(F407Rtc, RisingEdgeTamperReadsInactiveInputWithItsTemporaryGate) {
    config.lse_bypass = true;
    auto& m = g_f407_rtc_model;
    m.rcc.BDCR |= RCC_BDCR_LSEBYP;
    m.gpio_c.MODER = 0U;
    open();
    m.rcc.AHB1ENR &= ~uint32_t{RCC_AHB1ENR_GPIOCEN};
    m.rcc.AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    nx_rtc_capture_config_t capture{};
    capture.timestamp_pin = capture.tamper1_pin = 1U;
    capture.tamper_mask = 1U;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_capture_configure(&port, &capture));
    EXPECT_FALSE(state.auxiliary_faulted);
    EXPECT_EQ(1U, m.capture_pad_reads);
    EXPECT_EQ(0U, m.capture_unclocked_reads);
    EXPECT_EQ(RCC_AHB1ENR_GPIOAEN, m.rcc.AHB1ENR);
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_backup_write(&port, 0U, 42U));
    EXPECT_EQ(42U, m.rtc.BKP0R);
}

TEST_F(F407Rtc, AnalogPadCannotProveAnInactiveDigitalTamperInput) {
    config.lse_bypass = true;
    auto& m = g_f407_rtc_model;
    m.rcc.BDCR |= RCC_BDCR_LSEBYP;
    open();
    m.gpio_c.MODER |= 3U << 26U;
    nx_rtc_capture_config_t capture{};
    capture.timestamp_pin = capture.tamper1_pin = 1U;
    capture.tamper_mask = 1U;
    EXPECT_EQ(NX_ERROR_IO, nx_rtc_port_capture_configure(&port, &capture));
    EXPECT_TRUE(state.auxiliary_faulted);
    EXPECT_EQ(0U, m.capture_pad_reads);
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_backup_write(&port, 0U, 42U));
}

TEST_F(F407Rtc, ProvisionInspectsForeignOscillatorPadsBeforeBackupEffects) {
    for (unsigned pin : {14U, 15U}) {
        for (unsigned conflict = 0U; conflict < 3U; ++conflict) {
            state = {};
            provision();
            if (conflict == 0U)
                g_f407_rtc_model.gpio_c.MODER = 1U << (pin * 2U);
            if (conflict == 1U)
                g_f407_rtc_model.gpio_c.LCKR = 1U << pin;
            if (conflict == 2U)
                g_f407_rtc_model.gpio_c.PUPDR = 1U << (pin * 2U);
            EXPECT_NE(NX_SUCCESS, nx_stm32_rtc_prepare(&state, &config));
            EXPECT_EQ(0U, g_f407_rtc_model.rcc.BDCR);
            EXPECT_EQ(0U, g_f407_rtc_model.rcc.AHB1ENR & RCC_AHB1ENR_GPIOCEN);
            EXPECT_EQ(0U, nx_stm32_rtc_effects(&state));
        }
    }
}
TEST_F(F407Rtc, ProvisionRestoresOnlyItsTemporaryGpioInspectionClock) {
    provision();
    g_f407_rtc_model.rcc.AHB1ENR = RCC_AHB1ENR_GPIODEN;
    open();
    EXPECT_EQ(RCC_AHB1ENR_GPIODEN, g_f407_rtc_model.rcc.AHB1ENR);
}
TEST_F(F407Rtc, ProvisionKeepsPreownedGpioClockAndAnalogPadConfiguration) {
    provision();
    g_f407_rtc_model.rcc.AHB1ENR = RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN;
    g_f407_rtc_model.gpio_c.MODER = 15U << 28U;
    open();
    EXPECT_EQ(RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN,
              g_f407_rtc_model.rcc.AHB1ENR);
    EXPECT_EQ((15U << 28U) | (3U << 26U), g_f407_rtc_model.gpio_c.MODER);
}
TEST_F(F407Rtc, FailedGpioClockReadbackRetainsInspectionResponsibility) {
    provision();
    g_f407_rtc_model.hold_gpio_clock_enable = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_NE(0U, nx_stm32_rtc_effects(&state));
    EXPECT_EQ(0U, g_f407_rtc_model.rcc.BDCR);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
    EXPECT_EQ(0U, nx_stm32_rtc_effects(&state));
}
TEST_F(F407Rtc, FailedGpioClockRestoreBlocksBackupAndRetainsBusyCleanup) {
    provision();
    g_f407_rtc_model.hold_gpio_clock_restore = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_NE(0U, nx_stm32_rtc_effects(&state));
    EXPECT_EQ(0U, g_f407_rtc_model.rcc.BDCR);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_close(&state));
    g_f407_rtc_model.hold_gpio_clock_restore = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
    EXPECT_EQ(0U, nx_stm32_rtc_effects(&state));
}

TEST_F(F407Rtc, ConfiguredClockCallbackBindsOnlyTheExactSystemOwner) {
    g_f407_rtc_model.now = 1234U;
    EXPECT_EQ(1234U, nx_stm32_rtc_clock_read(&g_nx_stm32_system));
    EXPECT_EQ(1U, g_f407_rtc_model.clock_reads);
    EXPECT_EQ(0U, nx_stm32_rtc_clock_read(nullptr));
    EXPECT_EQ(0U, nx_stm32_rtc_clock_read(&state));
    EXPECT_EQ(1U, g_f407_rtc_model.clock_reads);
}

TEST_F(F407Rtc, CrystalProvisionQuietsPc13BeforeStartingLse) {
    provision();
    auto& model = g_f407_rtc_model;
    testing::MockFunction<void(volatile uint32_t*, uint32_t)> observer;
    unsigned starts = 0U;
    EXPECT_CALL(observer, Call(testing::_, testing::_))
        .WillRepeatedly(
            testing::Invoke([&](volatile uint32_t* reg, uint32_t value) {
                if (reg == &model.rcc.BDCR && (value & RCC_BDCR_LSEON)) {
                    ++starts;
                    EXPECT_EQ(3U, (model.gpio_c.MODER >> 26U) & 3U);
                }
            }));
    standby_observer = &observer;
    model.observe_write = [](volatile uint32_t* reg, uint32_t value) {
        standby_observer->Call(reg, value);
    };
    const auto status = nx_stm32_rtc_prepare(&state, &config);
    model.observe_write = nullptr;
    standby_observer = nullptr;
    EXPECT_EQ(NX_SUCCESS, status);
    EXPECT_GT(starts, 0U);
    EXPECT_EQ(3U, (model.gpio_c.MODER >> 26U) & 3U);
    EXPECT_EQ(0U, (model.gpio_c.PUPDR >> 26U) & 3U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_close(&state));
    nx_rtc_backup_handoff_t receipt{};
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_backup_handoff(&port, &receipt));
    EXPECT_EQ(3U, (model.gpio_c.MODER >> 26U) & 3U);
}

TEST_F(F407Rtc, CrystalProvisionRejectsForeignPc13BeforeBackupEffects) {
    provision();
    auto& model = g_f407_rtc_model;
    model.gpio_c.MODER = 2U << 26U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_EQ(0U, model.rcc.BDCR);
    EXPECT_EQ(0U, model.power.CR);
    EXPECT_EQ(2U, (model.gpio_c.MODER >> 26U) & 3U);
}

TEST_F(F407Rtc, CrystalAdoptionRejectsForeignPc13BeforeCalendarWrites) {
    auto& model = g_f407_rtc_model;
    model.gpio_c.MODER = 1U << 26U;
    const auto calendar = model.rtc.TR;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_EQ(calendar, model.rtc.TR);
    EXPECT_EQ(1U, (model.gpio_c.MODER >> 26U) & 3U);
    EXPECT_FALSE(state.opened);
}

TEST_F(F407Rtc, CrystalCaptureRejectsPc13TimestampAndTamperWithoutEffects) {
    open();
    nx_rtc_capture_config_t capture{};
    capture.timestamp_pin = capture.tamper1_pin = 1U;
    capture.timestamp_enabled = true;
    const auto writes = g_f407_rtc_model.writes;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_rtc_port_capture_configure(&port, &capture));
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
    capture.timestamp_enabled = false;
    capture.tamper_mask = 1U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_rtc_port_capture_configure(&port, &capture));
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
}

TEST_F(F407Rtc, ExplicitBypassRetainsPc13CaptureWithoutCrystalNoiseConflict) {
    config.lse_bypass = true;
    g_f407_rtc_model.rcc.BDCR |= RCC_BDCR_LSEBYP;
    g_f407_rtc_model.gpio_c.MODER = 2U << 26U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_prepare(&state, &config));
    nx_rtc_capture_config_t capture{};
    capture.timestamp_pin = capture.tamper1_pin = 1U;
    capture.timestamp_enabled = true;
    capture.tamper_mask = 1U;
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_capture_configure(&port, &capture));
    EXPECT_NE(0U, g_f407_rtc_model.rtc.CR & RTC_CR_TSE);
    EXPECT_NE(0U, g_f407_rtc_model.rtc.TAFCR & RTC_TAFCR_TAMP1E);
    EXPECT_EQ(2U, (g_f407_rtc_model.gpio_c.MODER >> 26U) & 3U);
}

TEST_F(F407Rtc, BypassProvisionRequiresAndPreservesExternalSourceIdentity) {
    provision();
    config.lse_bypass = true;
    g_f407_rtc_model.gpio_c.MODER = 1U << 26U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_NE(0U, g_f407_rtc_model.rcc.BDCR & RCC_BDCR_LSEBYP);
    EXPECT_EQ(1U, (g_f407_rtc_model.gpio_c.MODER >> 26U) & 3U);
    EXPECT_EQ(0U, state.effects & NX_RTC_BACKUP_QUIET_PC13);
}

TEST_F(F407Rtc, CrystalAdoptionRequiresAnAlreadyQuietDigitalInput) {
    g_f407_rtc_model.gpio_c.MODER = 0U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_EQ(0U, state.effects);
    EXPECT_FALSE(state.opened);
    EXPECT_EQ(0U, g_f407_rtc_model.writes);
}

TEST_F(F407Rtc, RetainedQuietPadInspectionRestoresOnlyItsTemporaryGate) {
    auto& model = g_f407_rtc_model;
    model.rcc.AHB1ENR = RCC_AHB1ENR_GPIODEN;
    open();
    EXPECT_EQ(RCC_AHB1ENR_GPIODEN, model.rcc.AHB1ENR);
    EXPECT_EQ(3U, (model.gpio_c.MODER >> 26U) & 3U);
    EXPECT_EQ(0U, state.effects);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_rtc_close(&state));
    EXPECT_EQ(RCC_AHB1ENR_GPIODEN, model.rcc.AHB1ENR);
}

TEST_F(F407Rtc, AuthoredBypassCannotAdoptAnObservedCrystalSource) {
    config.lse_bypass = true;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_rtc_prepare(&state, &config));
    EXPECT_EQ(0U, g_f407_rtc_model.writes);
    EXPECT_EQ(0U, state.effects);
    EXPECT_FALSE(state.opened);
}
