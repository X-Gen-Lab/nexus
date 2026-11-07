/**
 * \file            f407_rtc_test.cpp
 * \brief           Exact F407 backup provenance and alarm loan regressions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "f407_rtc_model.h"
#include <cstring>
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
    EXPECT_EQ(15U << 28U, g_f407_rtc_model.gpio_c.MODER);
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
