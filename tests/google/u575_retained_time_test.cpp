/**
 * \file            u575_retained_time_test.cpp
 * \brief           Real independent RTC clock and terminal failure regressions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/stm32u575_retained_time.h"
#include "nexus/core/fault.h"
#include "retained_time.h"
#include "u575_rtc_model.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>
#include <unistd.h>
using testing::StrictMock;
class FaultObserver {
  public:
    MOCK_METHOD(void, observe, (nx_fault_domain_t, nx_result_t));
};
static FaultObserver* observer;
static std::atomic<bool> observed_fault;
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char*, unsigned) {
    if (observer != nullptr)
        observer->observe(domain, reason);
    else
        _exit(91);
}
class U575RetainedTime : public testing::Test {
  protected:
    nx_stm32u575_retained_time_storage_t storage{};
    nx_stm32u575_retained_time_config_t config{NX_U575_RETAINED_TIME_RTC, 86400,
                                               nullptr};
    nx_clock_t clock{};
    void SetUp() override {
        nx_u575_rtc_model_reset();
        g_nx_stm32u575_system.started = false;
        g_u575_rtc_model.power.DBPR = 0U;
        observer = nullptr;
    }
    void open() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_initialize(
                                  &storage, &config, &clock));
        ASSERT_NE(nullptr, clock.read);
        ASSERT_EQ(&storage, clock.context);
    }
    nx_stm32u575_retained_time_snapshot_t sample() {
        nx_stm32u575_retained_time_snapshot_t out{};
        EXPECT_EQ(NX_SUCCESS,
                  nx_stm32u575_retained_time_sample(&storage, &out));
        return out;
    }
    nx_stm32u575_retained_time_owner_t owner() {
        nx_stm32u575_retained_time_owner_t out{};
        std::memcpy(&out, &storage, sizeof(out));
        return out;
    }
    void inject(const nx_stm32u575_retained_time_owner_t& value) {
        std::memcpy(&storage, &value, sizeof(value));
    }
};
TEST_F(U575RetainedTime,
       InitializationReadsOnlyAndDoesNotDependOnCoreOrWriteAccess) {
    RTC_TypeDef before = g_u575_rtc_model.rtc;
    open();
    auto out = sample();
    EXPECT_EQ(&storage, out.identity);
    EXPECT_EQ(1U, out.generation);
    EXPECT_EQ(0U, out.now);
    EXPECT_EQ(0U, out.ticks);
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
    EXPECT_TRUE(g_u575_rtc_model.wpr_locked);
    EXPECT_EQ(0, std::memcmp(&before, &g_u575_rtc_model.rtc, sizeof(before)));
}
TEST_F(U575RetainedTime, SubsecondQuartersCarryWithoutRepeatedRoundingLoss) {
    open();
    const uint64_t expected[] = {3906, 7812, 11718, 15625};
    for (unsigned n = 0; n < 4; ++n) {
        g_u575_rtc_model.rtc.SSR = 254U - n;
        auto out = sample();
        EXPECT_EQ(n + 1U, out.ticks);
        EXPECT_EQ(expected[n], out.now);
    }
}
TEST_F(U575RetainedTime, Year99To00WrapPreservesElapsedDomain) {
    open();
    auto first = sample();
    nx_rtc_calendar_t date{0, 1, 1, 6, 0, 0, 0};
    nx_u575_rtc_model_calendar(&date);
    g_u575_rtc_model.rtc.ICSR = 0U;
    auto second = sample();
    EXPECT_EQ(2000000U, second.now);
    EXPECT_EQ(512U, second.ticks);
    EXPECT_EQ(first.generation, second.generation);
}
TEST_F(U575RetainedTime, LeapDayAndMidnightExtendTheSameTickDomain) {
    nx_rtc_calendar_t initial{24, 2, 28, 3, 23, 59, 59};
    nx_u575_rtc_model_calendar(&initial);
    open();
    nx_rtc_calendar_t leap{24, 2, 29, 4, 0, 0, 0};
    nx_u575_rtc_model_calendar(&leap);
    EXPECT_EQ(1000000U, sample().now);
}
TEST_F(U575RetainedTime, BoundedCoherenceFailurePreservesOwnerAndOutput) {
    open();
    auto before = storage;
    auto out = sample();
    out.now = 77U;
    g_u575_rtc_model.unstable_calendar = true;
    auto reads = g_u575_rtc_model.calendar_reads;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_retained_time_sample(&storage, &out));
    EXPECT_LE(g_u575_rtc_model.calendar_reads - reads, 48U);
    EXPECT_EQ(77U, out.now);
    EXPECT_EQ(0, std::memcmp(&before, &storage, sizeof(storage)));
    g_u575_rtc_model.unstable_calendar = false;
    EXPECT_EQ(0U, sample().now);
}
TEST_F(U575RetainedTime, BackwardCalendarTaintsAndNeverPublishesStaleSnapshot) {
    open();
    auto out = sample();
    out.now = 77U;
    nx_rtc_calendar_t past{99, 12, 31, 5, 23, 59, 57};
    nx_u575_rtc_model_calendar(&past);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_sample(&storage, &out));
    EXPECT_EQ(77U, out.now);
    nx_rtc_calendar_t restored{99, 12, 31, 5, 23, 59, 58};
    nx_u575_rtc_model_calendar(&restored);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_sample(&storage, &out));
}
TEST_F(U575RetainedTime,
       ExactObservationGapAllowedButLargerGapInvalidatesEpoch) {
    config.max_gap_seconds = 1;
    open();
    nx_rtc_calendar_t next{99, 12, 31, 5, 23, 59, 59};
    nx_u575_rtc_model_calendar(&next);
    EXPECT_EQ(1000000U, sample().now);
    nx_rtc_calendar_t later{0, 1, 1, 6, 0, 0, 1};
    nx_u575_rtc_model_calendar(&later);
    nx_stm32u575_retained_time_snapshot_t out{};
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_sample(&storage, &out));
}
TEST_F(U575RetainedTime,
       ForeignSourceMutationCannotBeRepairedByBitRestoration) {
    open();
    auto out = sample();
    g_u575_rtc_model.rcc.BDCR &= ~static_cast<uint32_t>(RCC_BDCR_LSERDY);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_sample(&storage, &out));
    g_u575_rtc_model.rcc.BDCR |= RCC_BDCR_LSERDY;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_sample(&storage, &out));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575RetainedTime, RejectedInitializationPreservesOwnerAndOutput) {
    auto before = storage;
    nx_clock_t result{reinterpret_cast<nx_time_us_t (*)(void*)>(uintptr_t{1}),
                      &config};
    auto original = result;
    config.max_gap_seconds = 0;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_retained_time_initialize(
                                    &storage, &config, &result));
    EXPECT_EQ(0, std::memcmp(&storage, &before, sizeof(storage)));
    EXPECT_EQ(original.read, result.read);
    EXPECT_EQ(original.context, result.context);
    config.max_gap_seconds = 86401;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_retained_time_initialize(
                                    &storage, &config, &result));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575RetainedTime,
       ExistingAlarmAndWrongSecurityAreRejectedWithoutEffects) {
    g_u575_rtc_model.rtc.CR |= RTC_CR_ALRAE;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_retained_time_initialize(&storage, &config, &clock));
    g_u575_rtc_model.rtc.CR = RTC_CR_BYPSHAD;
    g_u575_rtc_model.flash.OPTR = FLASH_OPTR_TZEN;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32u575_retained_time_initialize(&storage, &config, &clock));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575RetainedTime, OutputAliasingCannotCorruptTheLiveClock) {
    open();
    auto before = storage;
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_stm32u575_retained_time_sample(
            &storage, reinterpret_cast<nx_stm32u575_retained_time_snapshot_t*>(
                          &storage)));
    EXPECT_EQ(0, std::memcmp(&storage, &before, sizeof(storage)));
}
TEST_F(U575RetainedTime, OwnerStorageCannotAliasKnownRtcRegisters) {
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_retained_time_initialize(
                  reinterpret_cast<nx_stm32u575_retained_time_storage_t*>(
                      &g_u575_rtc_model.rtc),
                  &config, &clock));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575RetainedTime, MissingApbGateRejectsBeforeRtcRegisterAccess) {
    g_u575_rtc_model.rcc.APB3ENR = 0U;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_initialize(&storage, &config, &clock));
    EXPECT_EQ(0U, g_u575_rtc_model.rtc_reads);
}
TEST_F(U575RetainedTime, DelegationDoesNotAdoptForeignWakeStateAtConstruction) {
    config.wake_owner = &config;
    g_u575_rtc_model.rtc.CR |= RTC_CR_WUTE | RTC_CR_WUTIE;
    g_u575_rtc_model.rtc.SR |= RTC_SR_WUTF;
    g_u575_rtc_model.irq_enabled = true;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_retained_time_initialize(&storage, &config, &clock));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575RetainedTime,
       StopAndReopenReserveNonwrappingGenerationAndInvalidateOldBinding) {
    config.wake_owner = &config;
    open();
    auto first = sample();
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_validate_wake(
                              &storage, &config, first.generation));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_stop(&storage));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_stop(&storage));
    open();
    auto second = sample();
    EXPECT_GT(second.generation, first.generation);
    EXPECT_EQ(0U, second.now);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_retained_time_validate_wake(
                                  &storage, &config, first.generation));
    auto value = owner();
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_stop(&storage));
    value.opened = false;
    value.generation = UINT64_MAX;
    inject(value);
    EXPECT_EQ(NX_ERROR_EXHAUSTED,
              nx_stm32u575_retained_time_initialize(&storage, &config, &clock));
}
TEST_F(U575RetainedTime, ExplicitWutOwnerCanChangeOnlyItsSubresource) {
    config.wake_owner = &config;
    open();
    auto first = sample();
    g_u575_rtc_model.rtc.CR |= RTC_CR_WUTE | RTC_CR_WUTIE | RTC_CR_WUCKSEL_2;
    g_u575_rtc_model.rtc.SR |= RTC_SR_WUTF;
    g_u575_rtc_model.irq_enabled = true;
    g_u575_rtc_model.irq_pending = true;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_validate_wake(
                              &storage, &config, first.generation));
    EXPECT_EQ(0U, sample().now);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_retained_time_validate_wake(
                                  &storage, &clock, first.generation));
    g_u575_rtc_model.rtc.CR |= RTC_CR_ALRAIE;
    nx_stm32u575_retained_time_snapshot_t out{};
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_sample(&storage, &out));
}
TEST_F(U575RetainedTime, StandaloneClockRejectsUndelegatedWakeOwnerChanges) {
    open();
    g_u575_rtc_model.rtc.CR |= RTC_CR_WUTE;
    nx_stm32u575_retained_time_snapshot_t out{};
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_sample(&storage, &out));
}
TEST_F(U575RetainedTime,
       ExhaustionNeverReturnsReservedNeverOrWrapsElapsedTime) {
    open();
    auto value = owner();
    value.ticks = UINT64_MAX / 1000000U * 256U;
    inject(value);
    nx_stm32u575_retained_time_snapshot_t out{};
    out.now = 77;
    nx_rtc_calendar_t next{0, 1, 1, 6, 0, 1, 0};
    nx_u575_rtc_model_calendar(&next);
    EXPECT_EQ(NX_ERROR_EXHAUSTED,
              nx_stm32u575_retained_time_sample(&storage, &out));
    EXPECT_EQ(77U, out.now);
}
TEST_F(U575RetainedTime, ClockCallbackReportsFaultAndCannotReturnStaleTime) {
    open();
    EXPECT_EXIT(
        {
            StrictMock<FaultObserver> fault;
            observer = &fault;
            EXPECT_CALL(fault, observe(NX_FAULT_PLATFORM, NX_ERROR_STATE))
                .WillOnce([](nx_fault_domain_t, nx_result_t) { _exit(17); });
            g_u575_rtc_model.rtc.PRER = 0U;
            (void)clock.read(clock.context);
            _exit(19);
        },
        testing::ExitedWithCode(17), "");
}
TEST_F(U575RetainedTime,
       SamplesPreserveIncomingMaskWithoutPretendingIrqSupport) {
    open();
    auto guard = nx_arch_irq_save();
    EXPECT_EQ(0U, sample().now);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(guard);
    EXPECT_FALSE(nx_arch_irq_is_masked());
}
TEST_F(U575RetainedTime, ReturningFaultObserverCannotResumeDeadlineConsumers) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    open();
    EXPECT_EXIT(
        {
            observed_fault.store(false, std::memory_order_relaxed);
            /* TSan defers SIGALRM while the terminal fault loop executes.
             * A child-only watchdog preserves the nonreturn assertion. */
            std::thread([] {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                _exit(observed_fault.load(std::memory_order_acquire) ? 17 : 18);
            }).detach();
            StrictMock<FaultObserver> fault;
            observer = &fault;
            EXPECT_CALL(fault, observe(NX_FAULT_PLATFORM, NX_ERROR_STATE))
                .WillOnce([](nx_fault_domain_t, nx_result_t) {
                    observed_fault.store(nx_arch_irq_is_masked(),
                                         std::memory_order_release);
                });
            g_u575_rtc_model.rtc.PRER = 0U;
            (void)clock.read(clock.context);
            _exit(19);
        },
        testing::ExitedWithCode(17), "");
}

TEST_F(U575RetainedTime, ForbiddenLowLseDriveCannotEstablishContinuity) {
    for (uint32_t drive : {0U, static_cast<uint32_t>(RCC_BDCR_LSEDRV_0)}) {
        g_u575_rtc_model.rcc.BDCR = (g_u575_rtc_model.rcc.BDCR &
                                     ~static_cast<uint32_t>(RCC_BDCR_LSEDRV)) |
                                    drive;
        EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_retained_time_initialize(
                                      &storage, &config, &clock));
        EXPECT_EQ(0U, g_u575_rtc_model.writes);
    }
}

TEST_F(U575RetainedTime, BypassIsOutsideExactRetainedCrystalProfile) {
    g_u575_rtc_model.rcc.BDCR |= RCC_BDCR_LSEBYP;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_initialize(&storage, &config, &clock));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575RetainedTime, WrappedWakeIdentityCannotEstablishDelegation) {
    config.wake_owner = reinterpret_cast<const void*>(UINTPTR_MAX);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_retained_time_initialize(&storage, &config, &clock));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575RetainedTime, DriveDriftPermanentlyInvalidatesAnAcceptedEpoch) {
    open();
    auto out = sample();
    g_u575_rtc_model.rcc.BDCR &= ~static_cast<uint32_t>(RCC_BDCR_LSEDRV);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_sample(&storage, &out));
    g_u575_rtc_model.rcc.BDCR |= RCC_BDCR_LSEDRV_1;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_sample(&storage, &out));
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}

TEST_F(U575RetainedTime, SlowSelectedMsisRejectsBeforeCalendarReads) {
    g_u575_rtc_model.rcc.CSR = 0U;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_initialize(&storage, &config, &clock));
    EXPECT_EQ(0U, g_u575_rtc_model.calendar_reads);
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575RetainedTime, ApbDividerDriftTaintsBeforeReadingCalendar) {
    open();
    auto out = sample();
    out.now = 73U;
    g_u575_rtc_model.calendar_reads = 0U;
    g_u575_rtc_model.rcc.CFGR2 = RCC_CFGR2_HPRE;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_sample(&storage, &out));
    EXPECT_EQ(73U, out.now);
    EXPECT_EQ(0U, g_u575_rtc_model.calendar_reads);
    g_u575_rtc_model.rcc.CFGR2 = 0U;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_sample(&storage, &out));
}

TEST_F(U575RetainedTime, AlarmCopyOptInRequiresBoundOwnerAndColdRegisters) {
    auto before = storage;
    g_u575_rtc_model.rtc.ALRMAR = 0U;
    g_u575_rtc_model.rtc.ALRMASSR = 0U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_retained_time_initialize_alarm_a_copy(
                  &storage, &config, &clock));
    config.wake_owner = &config;
    g_u575_rtc_model.rtc.ALRMAR = RTC_ALRMAR_MSK1;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_retained_time_initialize_alarm_a_copy(
                                 &storage, &config, &clock));
    g_u575_rtc_model.rtc.ALRMAR = 0U;
    g_u575_rtc_model.rtc.ALRMASSR = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_retained_time_initialize_alarm_a_copy(
                                 &storage, &config, &clock));
    EXPECT_EQ(0, std::memcmp(&storage, &before, sizeof(storage)));
    EXPECT_EQ(nullptr, clock.read);
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575RetainedTime, AlarmCopyAllowsOnlyFixedCompareAndTheSameWakeOwner) {
    config.wake_owner = &config;
    g_u575_rtc_model.rtc.ALRMAR = 0U;
    g_u575_rtc_model.rtc.ALRMASSR = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_initialize_alarm_a_copy(
                              &storage, &config, &clock));
    auto first = sample();
    g_u575_rtc_model.rtc.ALRMAR =
        RTC_ALRMAR_MSK1 | RTC_ALRMAR_MSK2 | RTC_ALRMAR_MSK3 | RTC_ALRMAR_MSK4;
    g_u575_rtc_model.rtc.CR |= RTC_CR_ALRAE | RTC_CR_WUTE | RTC_CR_WUTIE;
    g_u575_rtc_model.rtc.SR |= RTC_SR_ALRAF | RTC_SR_WUTF;
    g_u575_rtc_model.irq_enabled = true;
    g_u575_rtc_model.irq_pending = true;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_validate_alarm_a_copy(
                              &storage, &config, first.generation));
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_retained_time_validate_alarm_a_copy(
                                  &storage, &clock, first.generation));
    EXPECT_EQ(first.generation, sample().generation);
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
TEST_F(U575RetainedTime, OrdinaryWutClockCannotAuthorizeOrUpgradeAlarmCopy) {
    config.wake_owner = &config;
    open();
    auto first = sample();
    auto before = storage;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_retained_time_validate_alarm_a_copy(
                                  &storage, &config, first.generation));
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_initialize_alarm_a_copy(
                  &storage, &config, &clock));
    EXPECT_EQ(0, std::memcmp(&storage, &before, sizeof(storage)));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_validate_wake(
                              &storage, &config, first.generation));
}
TEST_F(U575RetainedTime, AlarmCopyProfileDriftPermanentlyTaintsTheEpoch) {
    for (unsigned variant = 0U; variant < 4U; ++variant) {
        SCOPED_TRACE(variant);
        nx_u575_rtc_model_reset();
        nx_stm32u575_retained_time_storage_t local{};
        nx_clock_t face{};
        config.wake_owner = &config;
        g_u575_rtc_model.rtc.ALRMAR = 0U;
        g_u575_rtc_model.rtc.ALRMASSR = 0U;
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32u575_retained_time_initialize_alarm_a_copy(
                      &local, &config, &face));
        g_u575_rtc_model.rtc.ALRMAR = RTC_ALRMAR_MSK1 | RTC_ALRMAR_MSK2 |
                                      RTC_ALRMAR_MSK3 | RTC_ALRMAR_MSK4;
        g_u575_rtc_model.rtc.CR |= RTC_CR_ALRAE;
        if (variant == 0U)
            g_u575_rtc_model.rtc.CR |= RTC_CR_ALRAIE;
        else if (variant == 1U)
            g_u575_rtc_model.rtc.CR |= RTC_CR_ALRAFCLR;
        else if (variant == 2U)
            g_u575_rtc_model.rtc.ALRMAR &=
                ~static_cast<uint32_t>(RTC_ALRMAR_MSK1);
        else
            g_u575_rtc_model.rtc.ALRMASSR = 1U;
        nx_stm32u575_retained_time_snapshot_t out{};
        out.now = 77U;
        EXPECT_EQ(NX_ERROR_STATE,
                  nx_stm32u575_retained_time_sample(&local, &out));
        EXPECT_EQ(77U, out.now);
        g_u575_rtc_model.rtc.CR = RTC_CR_BYPSHAD;
        g_u575_rtc_model.rtc.ALRMAR = 0U;
        g_u575_rtc_model.rtc.ALRMASSR = 0U;
        EXPECT_EQ(NX_ERROR_STATE,
                  nx_stm32u575_retained_time_sample(&local, &out));
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_stop(&local));
    }
}
TEST_F(U575RetainedTime, LegacyReopenDropsAlarmCopyPermissionAndOldGeneration) {
    config.wake_owner = &config;
    g_u575_rtc_model.rtc.ALRMAR = 0U;
    g_u575_rtc_model.rtc.ALRMASSR = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_initialize_alarm_a_copy(
                              &storage, &config, &clock));
    auto first = sample();
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_stop(&storage));
    open();
    auto next = sample();
    EXPECT_GT(next.generation, first.generation);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_retained_time_validate_alarm_a_copy(
                                  &storage, &config, next.generation));
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_retained_time_validate_alarm_a_copy(
                                  &storage, &config, first.generation));
    EXPECT_EQ(next.generation, sample().generation);
}
TEST_F(U575RetainedTime, AlarmCopyAdmissionRejectsAliasAndUnsafeAccessFirst) {
    config.wake_owner = &config;
    g_u575_rtc_model.rtc.ALRMAR = 0U;
    g_u575_rtc_model.rtc.ALRMASSR = 0U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_retained_time_initialize_alarm_a_copy(
                  reinterpret_cast<nx_stm32u575_retained_time_storage_t*>(
                      &g_u575_rtc_model.rtc),
                  &config, &clock));
    g_u575_rtc_model.rcc.APB3ENR = 0U;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_initialize_alarm_a_copy(
                  &storage, &config, &clock));
    EXPECT_EQ(0U, g_u575_rtc_model.rtc_reads);
    EXPECT_EQ(0U, g_u575_rtc_model.writes);
}
