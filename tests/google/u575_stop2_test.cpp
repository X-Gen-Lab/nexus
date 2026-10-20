/**
 * \file            u575_stop2_test.cpp
 * \brief           External MMIO and instruction contracts for exact Stop2
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/stm32u575_stop2.h"
#include "nexus/core/fault.h"
#include "stm32u575_stop2_hw.h"
#include "stm32u575_system.h"
#include <cstdlib>
#include <gtest/gtest.h>
#include <map>
#include <vector>
extern "C" {
nx_stm32u575_system_t g_nx_stm32u575_system;
}
namespace {
#define A(reg) reinterpret_cast<uintptr_t>(&(reg))
std::map<uintptr_t, uint32_t> regs;
std::vector<uintptr_t> writes;
nx_stm32u575_retained_time_storage_t clock_storage{};
nx_clock_barrier_t barrier{};
nx_arch_irq_state_t mask{};
uint32_t control_value, waits, sample_calls, protected_refusals;
uint64_t generation;
bool hold_wut_disable, hold_wut_ready, hold_flag_clear, hold_irq_clear;
bool hold_restore, source_fault, wrong_binding, change_generation;
bool unexpected_wake, corrupt_wake_clock, protected_locked;
uint32_t key;
bool unstable_calendar, drift_after_wait, late_flag, corrupt_wake_msik;
uint32_t subsecond_reads, calendar_reads_after_wait, rtc_accesses_after_wait;
#ifdef NX_U575_STOP2_COMBINED
nx_clock_t clock_face{};
#endif
uint32_t get(uintptr_t address) {
    return regs[address];
}
void put(uintptr_t address, uint32_t value) {
    regs[address] = value;
}
class U575Stop2 : public ::testing::Test {
  protected:
    void SetUp() override {
        regs.clear();
        writes.clear();
        control_value = waits = sample_calls = protected_refusals = key = 0;
        generation = 7;
        hold_wut_disable = hold_wut_ready = hold_flag_clear = false;
        hold_irq_clear = hold_restore = source_fault = wrong_binding = false;
        change_generation = unexpected_wake = corrupt_wake_clock = false;
        protected_locked = true;
        unstable_calendar = drift_after_wait = late_flag = corrupt_wake_msik =
            false;
        subsecond_reads = calendar_reads_after_wait = rtc_accesses_after_wait =
            0;
        put(A(RCC->BDCR), RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_RTCSEL_0 |
                              RCC_BDCR_RTCEN | RCC_BDCR_LSEDRV_1);
        put(A(RTC->PRER), 0x007f00ffU);
        put(A(RTC->SSR), 255U);
        put(A(RTC->DR), 0x0000c101U);
        g_nx_stm32u575_system = {};
        put(A(RCC->CR), RCC_CR_MSISON | RCC_CR_MSISRDY);
        put(A(RCC->CSR),
            (4U << RCC_CSR_MSISSRANGE_Pos) | (4U << RCC_CSR_MSIKSRANGE_Pos));
        put(A(RCC->AHB1ENR), RCC_AHB1ENR_FLASHEN | RCC_AHB1ENR_SRAM1EN);
        put(A(RCC->AHB3ENR), RCC_AHB3ENR_PWREN);
        put(A(RCC->APB3ENR), RCC_APB3ENR_RTCAPBEN);
        put(A(PWR->DBPR), PWR_DBPR_DBP);
        put(A(PWR->VOSR), PWR_VOSR_VOSRDY);
        put(A(FLASH->NSCR), FLASH_NSCR_LOCK);
        put(FLASHSIZE_BASE, 2048U);
        put(A(RTC->CR), RTC_CR_BYPSHAD);
        put(A(RTC->ICSR), RTC_ICSR_WUTWF);
        put(A(RTC->WUTR), 41U);
#ifdef NX_U575_STOP2_COMBINED
        nx_stm32u575_retained_time_config_t config = {
            NX_U575_RETAINED_TIME_RTC, 86400, nx_stm32u575_stop2_identity()};
        ASSERT_EQ(nx_stm32u575_retained_time_initialize(&clock_storage, &config,
                                                        &clock_face),
                  NX_SUCCESS);
#endif
        static bool initialized = false;
        if (!initialized) {
            ASSERT_EQ(nx_stm32u575_stop2_initialize(4, &clock_storage),
                      NX_SUCCESS);
            initialized = true;
        }
        ASSERT_EQ(nx_clock_domain_close(nx_stm32u575_stop2_domain(), &barrier),
                  NX_SUCCESS);
        mask = nx_arch_irq_save();
    }
    void TearDown() override {
        hold_wut_disable = hold_wut_ready = hold_flag_clear = false;
        hold_irq_clear = hold_restore = false;
        put(A(RCC->CFGR1), 0U);
        put(A(RCC->CSR),
            (4U << RCC_CSR_MSISSRANGE_Pos) | (4U << RCC_CSR_MSIKSRANGE_Pos));
        put(A(RCC->ICSCR1), 0U);
        (void)nx_stm32u575_stop2_recover(barrier, 8);
        nx_arch_irq_restore(mask);
#ifdef NX_U575_STOP2_COMBINED
        ASSERT_EQ(nx_stm32u575_retained_time_stop(&clock_storage), NX_SUCCESS);
#endif
        ASSERT_EQ(nx_clock_domain_reopen(barrier), NX_SUCCESS);
    }
    nx_stm32u575_stop2_result_t enter(uint32_t ticks = 2048) {
        return nx_stm32u575_stop2_enter(barrier, ticks, 8);
    }
};
#ifndef NX_U575_STOP2_COMBINED
TEST_F(U575Stop2, ArmsRealWutAndDrainsBeforeReturn) {
    auto r = enter();
    EXPECT_EQ(r.primary, NX_SUCCESS);
    EXPECT_EQ(r.cleanup, NX_SUCCESS);
    EXPECT_EQ(r.remaining_effects, 0U);
    EXPECT_TRUE(r.wait_executed);
    EXPECT_TRUE(r.wake_flag);
    EXPECT_EQ(waits, 1U);
    EXPECT_EQ(get(A(RTC->CR)), RTC_CR_BYPSHAD);
    EXPECT_EQ(get(A(RTC->WUTR)), 41U);
    EXPECT_EQ(get(A(RTC->SR)), 0U);
    EXPECT_EQ(get(A(NVIC->ISER[0])), 0U);
    EXPECT_EQ(get(A(NVIC->ISPR[0])), 0U);
    EXPECT_EQ(get(A(SCB->SCR)), 0U);
    EXPECT_EQ(get(A(PWR->CR1)), 0U);
    EXPECT_EQ(get(A(RCC->SRDAMR)), 0U);
    EXPECT_EQ(get(A(RCC->APB3SMENR)), 0U);
    EXPECT_TRUE(protected_locked);
    EXPECT_EQ(protected_refusals, 0U);
    EXPECT_EQ(nx_arch_irq_masks().primask, 1U);
    EXPECT_EQ(r.before.identity, &clock_storage);
    EXPECT_EQ(r.after.generation, r.before.generation);
    EXPECT_GE(r.after.now, r.before.now);
}
TEST_F(U575Stop2, RestoresExistingSleepGateAndMsiKernelState) {
    put(A(RCC->APB3SMENR), RCC_APB3SMENR_RTCAPBSMEN);
    put(A(RCC->CR),
        RCC_CR_MSISON | RCC_CR_MSISRDY | RCC_CR_MSIKON | RCC_CR_MSIKRDY);
    auto r = enter(65536);
    EXPECT_EQ(r.primary, NX_SUCCESS);
    EXPECT_EQ(get(A(RCC->APB3SMENR)), RCC_APB3SMENR_RTCAPBSMEN);
    EXPECT_NE(get(A(RCC->CR)) & RCC_CR_MSIKON, 0U);
}
TEST_F(U575Stop2, RejectsOutOfRangeWithoutWrites) {
    EXPECT_EQ(enter(1).primary, NX_ERROR_INVALID);
    EXPECT_EQ(enter(65537).primary, NX_ERROR_INVALID);
    EXPECT_TRUE(writes.empty());
    EXPECT_EQ(waits, 0U);
}
TEST_F(U575Stop2, RequiresMaskedMspThreadAndPreservesMask) {
    nx_arch_irq_restore(mask);
    EXPECT_EQ(enter().primary, NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_arch_irq_masks().primask, 0U);
    mask = nx_arch_irq_save();
    control_value = 2;
    EXPECT_EQ(enter().primary, NX_ERROR_CONTEXT);
    EXPECT_TRUE(writes.empty());
}
TEST_F(U575Stop2, RejectsForeignBarrier) {
    auto copy = barrier;
    copy.domain = nullptr;
    EXPECT_EQ(nx_stm32u575_stop2_enter(copy, 2, 8).primary, NX_ERROR_INVALID);
    EXPECT_TRUE(writes.empty());
}
TEST_F(U575Stop2, EnrolledOwnerMustActuallyDrain) {
    nx_arch_irq_restore(mask);
    ASSERT_EQ(nx_clock_domain_reopen(barrier), NX_SUCCESS);
    nx_clock_lease_t lease{};
    ASSERT_EQ(nx_clock_domain_acquire(nx_stm32u575_stop2_domain(), &lease),
              NX_SUCCESS);
    ASSERT_EQ(nx_clock_domain_close(nx_stm32u575_stop2_domain(), &barrier),
              NX_SUCCESS);
    mask = nx_arch_irq_save();
    EXPECT_EQ(enter().primary, NX_ERROR_BUSY);
    EXPECT_TRUE(writes.empty());
    ASSERT_EQ(nx_clock_domain_release(nx_stm32u575_stop2_domain(), &lease),
              NX_SUCCESS);
}
TEST_F(U575Stop2, RejectsRunningSystemAndAutonomousMaster) {
    g_nx_stm32u575_system.started = true;
    EXPECT_EQ(enter().primary, NX_ERROR_BUSY);
    g_nx_stm32u575_system.started = false;
    put(A(RCC->SRDAMR), RCC_SRDAMR_LPDMA1AMEN);
    EXPECT_EQ(enter().primary, NX_ERROR_BUSY);
    EXPECT_TRUE(writes.empty());
}
TEST_F(U575Stop2, RejectsSramPowerDownAndDebug) {
    put(A(PWR->CR2), PWR_CR2_SRAM4PDS);
    EXPECT_EQ(enter().primary, NX_ERROR_UNSUPPORTED);
    put(A(PWR->CR2), 0U);
    put(A(DBGMCU->CR), DBGMCU_CR_DBG_STOP);
    EXPECT_EQ(enter().primary, NX_ERROR_UNSUPPORTED);
    EXPECT_TRUE(writes.empty());
}
TEST_F(U575Stop2, RejectsOtherIrqAndExtiSources) {
    put(A(NVIC->ISPR[3]), 1U);
    EXPECT_EQ(enter().primary, NX_ERROR_BUSY);
    put(A(NVIC->ISPR[3]), 0U);
    put(A(EXTI->IMR1), 1U);
    EXPECT_EQ(enter().primary, NX_ERROR_BUSY);
    EXPECT_TRUE(writes.empty());
}
TEST_F(U575Stop2, RejectsPllReadyAndWrongOptionBytes) {
    put(A(RCC->CR), get(A(RCC->CR)) | RCC_CR_PLL2RDY);
    EXPECT_EQ(enter().primary, NX_ERROR_UNSUPPORTED);
    put(A(RCC->CR), RCC_CR_MSISON | RCC_CR_MSISRDY);
    put(A(FLASH->OPTR), FLASH_OPTR_TZEN);
    EXPECT_EQ(enter().primary, NX_ERROR_UNSUPPORTED);
    EXPECT_TRUE(writes.empty());
}
TEST_F(U575Stop2, RejectsForeignRtcWakeAndBinding) {
    put(A(RTC->CR), RTC_CR_BYPSHAD | RTC_CR_WUTE);
    EXPECT_EQ(enter().primary, NX_ERROR_BUSY);
    put(A(RTC->CR), RTC_CR_BYPSHAD);
    wrong_binding = true;
    EXPECT_EQ(enter().primary, NX_ERROR_STATE);
    EXPECT_TRUE(writes.empty());
}
TEST_F(U575Stop2, BoundedArmFailureRollsBackWithoutWaiting) {
    hold_wut_ready = true;
    auto r = enter();
    EXPECT_EQ(r.primary, NX_ERROR_TIMEOUT);
    EXPECT_EQ(r.cleanup, NX_ERROR_TIMEOUT);
    EXPECT_NE(r.remaining_effects, 0U);
    EXPECT_EQ(waits, 0U);
    EXPECT_TRUE(protected_locked);
    hold_wut_ready = false;
    auto recovered = nx_stm32u575_stop2_recover(barrier, 8);
    EXPECT_EQ(recovered.cleanup, NX_SUCCESS);
    EXPECT_EQ(recovered.remaining_effects, 0U);
}
TEST_F(U575Stop2, FailedWutDrainRetainsOwnershipUntilRecovery) {
    hold_wut_disable = true;
    auto r = enter();
    EXPECT_EQ(r.primary, NX_SUCCESS);
    EXPECT_EQ(r.cleanup, NX_ERROR_IO);
    EXPECT_NE(r.remaining_effects, 0U);
    size_t prior = writes.size();
    EXPECT_NE(enter(1).remaining_effects, 0U);
    EXPECT_EQ(enter().primary, NX_ERROR_STATE);
    EXPECT_EQ(writes.size(), prior);
    hold_wut_disable = false;
    auto recovered = nx_stm32u575_stop2_recover(barrier, 8);
    EXPECT_EQ(recovered.cleanup, NX_SUCCESS);
    EXPECT_EQ(recovered.remaining_effects, 0U);
    EXPECT_EQ(waits, 1U);
}
TEST_F(U575Stop2, PendingIrqCannotBeReleasedBeforeObservedClear) {
    hold_irq_clear = true;
    auto r = enter();
    EXPECT_EQ(r.cleanup, NX_ERROR_IO);
    EXPECT_NE(r.remaining_effects, 0U);
    hold_irq_clear = false;
    EXPECT_EQ(nx_stm32u575_stop2_recover(barrier, 8).remaining_effects, 0U);
}
TEST_F(U575Stop2, BadWakeClockQuarantinesRatherThanRestartingTim2) {
    corrupt_wake_clock = true;
    auto r = enter();
    EXPECT_EQ(r.primary, NX_ERROR_IO);
    EXPECT_EQ(r.cleanup, NX_ERROR_IO);
    EXPECT_NE(r.remaining_effects, 0U);
    EXPECT_EQ(r.after.generation, 0U);
    EXPECT_EQ(rtc_accesses_after_wait, 0U);
    EXPECT_FALSE(g_nx_stm32u575_system.started);
    EXPECT_EQ(get(A(RCC->APB1ENR1)), 0U);
    put(A(RCC->CFGR1), 0U);
    EXPECT_EQ(nx_stm32u575_stop2_recover(barrier, 8).remaining_effects, 0U);
}
TEST_F(U575Stop2, ChangedClockGenerationCannotProduceElapsedResult) {
    change_generation = true;
    auto r = enter();
    EXPECT_EQ(r.primary, NX_ERROR_STATE);
    EXPECT_EQ(r.cleanup, NX_SUCCESS);
    EXPECT_EQ(r.after.generation, 0U);
    EXPECT_EQ(r.remaining_effects, 0U);
}
TEST_F(U575Stop2, UnexpectedWakeStillDrainsAndReportsNoRtcFlag) {
    unexpected_wake = true;
    auto r = enter(2);
    EXPECT_EQ(r.primary, NX_ERROR_IO);
    EXPECT_EQ(r.cleanup, NX_SUCCESS);
    EXPECT_FALSE(r.wake_flag);
    EXPECT_EQ(r.remaining_effects, 0U);
}
TEST_F(U575Stop2, FailedGateRestoreRetainsRecoveryResponsibility) {
    hold_restore = true;
    auto r = enter();
    EXPECT_EQ(r.cleanup, NX_ERROR_IO);
    EXPECT_NE(r.remaining_effects, 0U);
    hold_restore = false;
    EXPECT_EQ(nx_stm32u575_stop2_recover(barrier, 8).remaining_effects, 0U);
}
TEST_F(U575Stop2,
       RejectsUnsupportedMsikRangesFromBothSelectorsBeforeHardwareEffects) {
    for (bool selected : {false, true}) {
        SCOPED_TRACE(selected);
        /* CSR code0 is reserved; ICSCR1 code0 is 48MHz (above Range4). */
        put(A(RCC->CSR), 4U << RCC_CSR_MSISSRANGE_Pos);
        put(A(RCC->ICSCR1),
            selected ? RCC_ICSCR1_MSIRGSEL | (4U << RCC_ICSCR1_MSISRANGE_Pos)
                     : 0U);
        auto result = enter();
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, result.primary);
        EXPECT_TRUE(writes.empty());
        EXPECT_EQ(0U, waits);
    }
}
TEST_F(U575Stop2, ChangedMsikWakeRangeCannotAuthorizeRtcDrain) {
    corrupt_wake_msik = true;
    auto result = enter();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_ERROR_IO, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_EQ(0U, rtc_accesses_after_wait);
    put(A(RCC->CSR),
        (4U << RCC_CSR_MSISSRANGE_Pos) | (4U << RCC_CSR_MSIKSRANGE_Pos));
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 8U).remaining_effects);
    EXPECT_EQ(1U, waits);
}
TEST_F(U575Stop2, Range1IsRejectedBeforeAnyHardwareEffect) {
    /* RM0456 10.10.4: VOS=11 is Range1; the selected profile is VOS=00. */
    put(A(PWR->VOSR), PWR_VOSR_VOS | PWR_VOSR_VOSRDY);
    auto result = enter();
    EXPECT_EQ(result.primary, NX_ERROR_UNSUPPORTED);
    EXPECT_TRUE(writes.empty());
    EXPECT_EQ(waits, 0U);
}
#else
TEST_F(U575Stop2, CombinedRealClockWutDrainAndRestoration) {
    auto r = enter();
    ASSERT_EQ(r.primary, NX_SUCCESS);
    ASSERT_EQ(r.cleanup, NX_SUCCESS);
    EXPECT_EQ(r.remaining_effects, 0U);
    EXPECT_EQ(r.before.identity, &clock_storage);
    EXPECT_EQ(r.before.generation, r.after.generation);
    EXPECT_EQ(r.after.now - r.before.now, 1000000U);
    EXPECT_EQ(get(A(RTC->CR)), RTC_CR_BYPSHAD);
    EXPECT_EQ(get(A(RTC->WUTR)), 41U);
    EXPECT_EQ(get(A(SCB->SCR)), 0U);
    EXPECT_EQ(get(A(NVIC->ISPR[0])), 0U);
    EXPECT_FALSE(g_nx_stm32u575_system.started);
    EXPECT_EQ(nx_arch_irq_masks().primask, 1U);
}
TEST_F(U575Stop2, CombinedSourceDriftTaintsClockButStillDrainsWut) {
    drift_after_wait = true;
    auto r = enter();
    EXPECT_EQ(r.primary, NX_ERROR_STATE);
    EXPECT_EQ(r.cleanup, NX_SUCCESS);
    EXPECT_EQ(r.remaining_effects, 0U);
    EXPECT_EQ(r.after.generation, 0U);
    nx_stm32u575_retained_time_snapshot_t snapshot{};
    EXPECT_EQ(nx_stm32u575_retained_time_sample(&clock_storage, &snapshot),
              NX_ERROR_STATE);
    EXPECT_EQ(get(A(RTC->SR)), 0U);
    EXPECT_EQ(get(A(NVIC->ISER[0])), 0U);
}
TEST_F(U575Stop2, CombinedIncoherentReadRejectsBeforeHardwareEffects) {
    unstable_calendar = true;
    auto r = enter();
    EXPECT_EQ(r.primary, NX_ERROR_BUSY);
    EXPECT_EQ(waits, 0U);
    EXPECT_TRUE(writes.empty());
}
TEST_F(U575Stop2, CombinedReopenUsesNewClockGeneration) {
    nx_stm32u575_retained_time_snapshot_t before{};
    ASSERT_EQ(nx_stm32u575_retained_time_sample(&clock_storage, &before),
              NX_SUCCESS);
    nx_arch_irq_restore(mask);
    ASSERT_EQ(nx_stm32u575_retained_time_stop(&clock_storage), NX_SUCCESS);
    nx_stm32u575_retained_time_config_t config = {
        NX_U575_RETAINED_TIME_RTC, 86400, nx_stm32u575_stop2_identity()};
    ASSERT_EQ(nx_stm32u575_retained_time_initialize(&clock_storage, &config,
                                                    &clock_face),
              NX_SUCCESS);
    mask = nx_arch_irq_save();
    EXPECT_EQ(
        nx_stm32u575_retained_time_validate_wake(
            &clock_storage, nx_stm32u575_stop2_identity(), before.generation),
        NX_ERROR_STATE);
    EXPECT_EQ(nx_stm32u575_retained_time_validate_wake(
                  &clock_storage, &clock_face, before.generation + 1),
              NX_ERROR_STATE);
    auto r = enter();
    EXPECT_EQ(r.primary, NX_SUCCESS);
    EXPECT_GT(r.before.generation, before.generation);
    EXPECT_EQ(r.before.generation, r.after.generation);
}
TEST_F(U575Stop2, CombinedLiveClockDoesNotAuthorizeRunningSystem) {
    g_nx_stm32u575_system.started = true;
    EXPECT_EQ(enter().primary, NX_ERROR_BUSY);
    EXPECT_TRUE(writes.empty());
    EXPECT_EQ(waits, 0U);
}
TEST_F(U575Stop2, CombinedLateWutfAndFailedDrainRetainThenRecover) {
    late_flag = true;
    hold_flag_clear = true;
    auto r = enter();
    EXPECT_EQ(r.cleanup, NX_ERROR_IO);
    EXPECT_NE(r.remaining_effects, 0U);
    EXPECT_EQ(enter().primary, NX_ERROR_STATE);
    hold_flag_clear = false;
    auto recovered = nx_stm32u575_stop2_recover(barrier, 8);
    EXPECT_EQ(recovered.cleanup, NX_SUCCESS);
    EXPECT_EQ(recovered.remaining_effects, 0U);
    EXPECT_EQ(get(A(RTC->SR)), 0U);
    EXPECT_EQ(waits, 1U);
}
TEST_F(U575Stop2, CombinedFailedClockFaceCannotReturnAfterWakeDrift) {
    drift_after_wait = true;
    ASSERT_EQ(enter().primary, NX_ERROR_STATE);
    nx_arch_irq_restore(mask);
    EXPECT_EXIT((void)clock_face.read(clock_face.context),
                ::testing::ExitedWithCode(27), "");
    mask = nx_arch_irq_save();
}
TEST_F(U575Stop2, CombinedBadWakeProfileCannotReadCalendar) {
    corrupt_wake_clock = true;
    auto r = enter();
    EXPECT_EQ(r.primary, NX_ERROR_IO);
    EXPECT_EQ(r.cleanup, NX_ERROR_IO);
    EXPECT_NE(r.remaining_effects, 0U);
    EXPECT_EQ(r.after.generation, 0U);
    EXPECT_EQ(calendar_reads_after_wait, 0U);
    EXPECT_EQ(rtc_accesses_after_wait, 0U);
    put(A(RCC->CFGR1), 0U);
    EXPECT_EQ(nx_stm32u575_stop2_recover(barrier, 8).remaining_effects, 0U);
}
TEST_F(U575Stop2, CombinedUnsafeLseDriveRejectsBeforeWutWrite) {
    put(A(RCC->BDCR),
        get(A(RCC->BDCR)) & ~static_cast<uint32_t>(RCC_BDCR_LSEDRV));
    EXPECT_EQ(enter().primary, NX_ERROR_STATE);
    EXPECT_TRUE(writes.empty());
    EXPECT_EQ(waits, 0U);
}
#endif
} /* namespace */
extern "C" uint32_t nx_u575_stop2_control(void) {
    return control_value;
}
extern "C" uint32_t nx_u575_stop2_read(uintptr_t address) {
    if (waits != 0U && address >= reinterpret_cast<uintptr_t>(RTC) &&
        address - reinterpret_cast<uintptr_t>(RTC) < sizeof(RTC_TypeDef))
        ++rtc_accesses_after_wait;
    if (waits != 0U && address == A(RTC->SSR))
        ++calendar_reads_after_wait;
    if (address == A(RTC->ICSR) && hold_wut_ready && !writes.empty())
        return 0U;
    if (address == A(RTC->SSR) && unstable_calendar)
        return get(address) + (++subsecond_reads & 1U);
    return get(address);
}
extern "C" void nx_u575_stop2_write(uintptr_t address, uint32_t value) {
    if (waits != 0U && address >= reinterpret_cast<uintptr_t>(RTC) &&
        address - reinterpret_cast<uintptr_t>(RTC) < sizeof(RTC_TypeDef))
        ++rtc_accesses_after_wait;
    writes.push_back(address);
    if (address == A(RTC->WPR)) {
        if (value == 0xca)
            key = 1;
        else if (key == 1 && value == 0x53)
            protected_locked = false;
        else {
            protected_locked = true;
            key = 0;
        }
        return;
    }
    if ((address == A(RTC->CR) || address == A(RTC->WUTR)) &&
        protected_locked) {
        ++protected_refusals;
        return;
    }
    if (address == A(RTC->CR)) {
        if (hold_wut_disable && (get(address) & RTC_CR_WUTE) &&
            !(value & RTC_CR_WUTE))
            return;
        if (late_flag && (get(address) & RTC_CR_WUTE) &&
            !(value & RTC_CR_WUTE)) {
            put(A(RTC->SR), RTC_SR_WUTF);
            put(A(NVIC->ISPR[0]), 1U << RTC_IRQn);
        }
        put(address, value);
        put(A(RTC->ICSR), value & RTC_CR_WUTE ? 0U : RTC_ICSR_WUTWF);
    } else if (address == A(RTC->SCR)) {
        if (!hold_flag_clear)
            put(A(RTC->SR), get(A(RTC->SR)) & ~value);
    } else if (address == A(NVIC->ISER[0]))
        put(address, get(address) | value);
    else if (address == A(NVIC->ICER[0]))
        put(A(NVIC->ISER[0]), get(A(NVIC->ISER[0])) & ~value);
    else if (address == A(NVIC->ICPR[0])) {
        if (!hold_irq_clear)
            put(A(NVIC->ISPR[0]), get(A(NVIC->ISPR[0])) & ~value);
    } else if (address == A(RCC->APB3SMENR) && hold_restore && value == 0U)
        return;
    else if (address == A(RCC->CR)) {
        put(address, value | RCC_CR_MSISRDY);
        if (value & RCC_CR_MSIKON)
            put(address, get(address) | RCC_CR_MSIKRDY);
        else
            put(address, get(address) & ~static_cast<uint32_t>(RCC_CR_MSIKRDY));
    } else
        put(address, value);
}
extern "C" void nx_u575_stop2_wait(void) {
    ++waits;
    EXPECT_NE(get(A(SCB->SCR)) & SCB_SCR_SLEEPDEEP_Msk, 0U);
    EXPECT_EQ(get(A(PWR->CR1)) & PWR_CR1_LPMS, PWR_CR1_LPMS_1);
    EXPECT_LE(get(A(RTC->WUTR)), 65535U);
    EXPECT_EQ(get(A(RTC->CR)) & (RTC_CR_WUTE | RTC_CR_WUTIE),
              RTC_CR_WUTE | RTC_CR_WUTIE);
    EXPECT_NE(get(A(RCC->SRDAMR)) & RCC_SRDAMR_RTCAPBAMEN, 0U);
    EXPECT_NE(get(A(RCC->APB3SMENR)) & RCC_APB3SMENR_RTCAPBSMEN, 0U);
    EXPECT_EQ(nx_arch_irq_masks().primask, 1U);
    if (!unexpected_wake) {
        put(A(RTC->SR), RTC_SR_WUTF);
        put(A(NVIC->ISPR[0]), 1U << RTC_IRQn);
    }
    put(A(RCC->CR), get(A(RCC->CR)) | RCC_CR_MSIKON | RCC_CR_MSIKRDY);
    if (corrupt_wake_clock)
        put(A(RCC->CFGR1), RCC_CFGR1_STOPWUCK);
    if (corrupt_wake_msik)
        put(A(RCC->CSR),
            get(A(RCC->CSR)) & ~static_cast<uint32_t>(RCC_CSR_MSIKSRANGE));
    /* Both counters share LSE: eight RTC/16 WUT ticks equal one
     * retained256Hz calendar tick. This models a nominal WUTR+1 wake. */
    uint32_t calendar_ticks =
        255U - get(A(RTC->SSR)) + (get(A(RTC->WUTR)) + 1U) / 8U;
    uint32_t raw_time = get(A(RTC->TR));
    auto decimal = [](uint32_t bcd) { return (bcd >> 4U) * 10U + (bcd & 15U); };
    auto bcd = [](uint32_t decimal_value) {
        return (decimal_value / 10U << 4U) | (decimal_value % 10U);
    };
    uint32_t seconds =
        decimal(raw_time & 0x7fU) + decimal((raw_time >> 8U) & 0x7fU) * 60U +
        decimal((raw_time >> 16U) & 0x3fU) * 3600U + calendar_ticks / 256U;
    put(A(RTC->SSR), 255U - calendar_ticks % 256U);
    put(A(RTC->TR), bcd(seconds % 60U) | (bcd(seconds / 60U % 60U) << 8U) |
                        (bcd(seconds / 3600U % 24U) << 16U));
    if (drift_after_wait)
        put(A(RTC->PRER), 0U);
    if (change_generation)
        ++generation;
}
#ifndef NX_U575_STOP2_COMBINED
extern "C" nx_result_t nx_stm32u575_retained_time_sample(
    nx_stm32u575_retained_time_storage_t* storage,
    nx_stm32u575_retained_time_snapshot_t* snapshot) {
    if (source_fault)
        return NX_ERROR_STATE;
    ++sample_calls;
    if (waits != 0U) {
        EXPECT_EQ(get(A(SCB->SCR)), 0U);
        EXPECT_EQ(get(A(RCC->CFGR1)), 0U);
    }
    *snapshot = {storage, generation, sample_calls * 500000ULL,
                 sample_calls * 128ULL};
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32u575_retained_time_validate_wake(
    nx_stm32u575_retained_time_storage_t* storage, const void* owner,
    uint64_t epoch) {
    return storage == &clock_storage &&
                   owner == nx_stm32u575_stop2_identity() &&
                   epoch == generation && !wrong_binding && !source_fault
               ? NX_SUCCESS
               : NX_ERROR_STATE;
}

#endif

#ifdef NX_U575_STOP2_COMBINED
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char*, unsigned) {
    std::_Exit(domain == NX_FAULT_PLATFORM && reason == NX_ERROR_STATE &&
                       nx_arch_irq_masks().primask == 1U
                   ? 27
                   : 28);
}
#endif
