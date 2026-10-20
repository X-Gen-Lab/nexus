/** \file test.cpp
 * \brief Real retained-clock, Stop2 and finite receiver compound contracts.
 * \author Nexus Team
 * \version 1.0.0
 * \date 2026-10-20
 * \copyright Copyright (c) 2026 Nexus Team
 */
#include "model.h"
#include "nexus/arch/arch.h"
#include "nexus/components/stm32u575_stop2_copy.h"
#include "stm32u575_lpdma_copy_internal.h"
#include "stm32u575_system.h"
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>
extern "C" {
nx_stm32u575_system_t g_nx_stm32u575_system;
uint32_t nx_u575_lpdma_copy_bus_read(uintptr_t address);
void nx_u575_lpdma_copy_bus_write(uintptr_t address, uint32_t value);
}
namespace {
#define A(value) reinterpret_cast<uintptr_t>(&(value))
constexpr uint32_t rtc_irq = 1U << RTC_IRQn;
constexpr uint32_t dma_irq = 1U << (114U % 32U);
constexpr uint32_t sleep_bits =
    RCC_AHB3SMENR_LPDMA1SMEN | RCC_AHB3SMENR_SRAM4SMEN;
constexpr uint32_t auto_bits = RCC_SRDAMR_LPDMA1AMEN | RCC_SRDAMR_SRAM4AMEN;
constexpr uint32_t alarm_mask =
    RTC_ALRMAR_MSK1 | RTC_ALRMAR_MSK2 | RTC_ALRMAR_MSK3 | RTC_ALRMAR_MSK4;
nx_stm32u575_retained_time_storage_t clock_storage{};
nx_clock_t clock_face{};
unsigned waits, key, protected_refusals, forbidden_accesses;
bool locked, started, hold_alarm_disable, hold_wut_disable, hold_alarm_clear;
bool hold_rtc_irq, bad_wake_profile, drift_after_wait, early_alarm, escape_only;
bool half_copy, late_alarm, final_irq_race, bad_rtc_gate, bad_dma_reset;
bool hold_alarm_program, dma_error;
std::vector<uintptr_t> writes;
bool peripheral(uintptr_t address) {
    return (address >= RTC_BASE_NS &&
            address < RTC_BASE_NS + sizeof(RTC_TypeDef)) ||
           (address >= LPDMA1_BASE_NS && address < LPDMA1_BASE_NS + 0x400U);
}
void account(uintptr_t address) {
    if (waits != 0U && peripheral(address) &&
        ((bad_wake_profile && model.reg(&RCC->CFGR2) != 0U) ||
         (model.reg(&RCC->APB3ENR) & RCC_APB3ENR_RTCAPBEN) == 0U ||
         (model.reg(&RCC->AHB3RSTR) & RCC_AHB3RSTR_LPDMA1RST) != 0U))
        ++forbidden_accesses;
}
void alarm() {
    if ((model.reg(&RTC->CR) & RTC_CR_ALRAE) == 0U ||
        (model.reg(&RTC->SR) & RTC_SR_ALRAF) != 0U)
        return;
    model.reg(&RTC->SR) |= RTC_SR_ALRAF;
    model.trigger();
    if (half_copy)
        model.transfer(3U);
    else {
        model.finish_block();
        model.finish_block();
    }
}
class U575Stop2Copy : public testing::Test {
  protected:
    nx_stm32u575_lpdma_copy_storage_t storage{};
    nx_stm32u575_lpdma_copy_t copy{};
    nx_stm32u575_lpdma_copy_request_t request{};
    nx_clock_barrier_t barrier{};
    nx_arch_irq_state_t mask{};
    unsigned char* source =
        reinterpret_cast<unsigned char*>(SRAM4_BASE_NS + 0x100U);
    unsigned char* destination =
        reinterpret_cast<unsigned char*>(SRAM4_BASE_NS + 0x200U);
    nx_stm32u575_lpdma_copy_workspace_t* workspace =
        reinterpret_cast<nx_stm32u575_lpdma_copy_workspace_t*>(SRAM4_BASE_NS +
                                                               0x300U);
    static void SetUpTestSuite() {
        ASSERT_EQ(reinterpret_cast<void*>(SRAM4_BASE_NS),
                  mmap(reinterpret_cast<void*>(SRAM4_BASE_NS), SRAM4_SIZE,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1,
                       0));
    }
    static void TearDownTestSuite() {
        EXPECT_EQ(0,
                  munmap(reinterpret_cast<void*>(SRAM4_BASE_NS), SRAM4_SIZE));
    }
    void SetUp() override {
        model.reset();
        writes.clear();
        waits = key = protected_refusals = forbidden_accesses = 0U;
        locked = true;
        started = hold_alarm_disable = hold_wut_disable = hold_alarm_clear =
            false;
        hold_rtc_irq = bad_wake_profile = drift_after_wait = early_alarm =
            escape_only = false;
        half_copy = late_alarm = final_irq_race = bad_rtc_gate = bad_dma_reset =
            false;
        hold_alarm_program = dma_error = false;
        g_nx_stm32u575_system = {};
        model.reg(&RCC->BDCR) = RCC_BDCR_LSEON | RCC_BDCR_LSERDY |
                                RCC_BDCR_LSEDRV_1 | RCC_BDCR_RTCSEL_0 |
                                RCC_BDCR_RTCEN;
        model.reg(&RCC->AHB1ENR) = RCC_AHB1ENR_FLASHEN | RCC_AHB1ENR_SRAM1EN;
        model.reg(&PWR->DBPR) = PWR_DBPR_DBP;
        model.reg(&FLASH->NSCR) = FLASH_NSCR_LOCK;
        model.values[FLASHSIZE_BASE] = 2048U;
        model.reg(&RTC->PRER) = 0x007f00ffU;
        model.reg(&RTC->ICSR) = RTC_ICSR_WUTWF;
        model.reg(&RTC->WUTR) = 41U;
        model.reg(&RTC->SSR) = 255U;
        model.reg(&RTC->DR) = 0x0000c101U;
        std::memset(reinterpret_cast<void*>(SRAM4_BASE_NS), 0xa5, SRAM4_SIZE);
        for (unsigned n = 0U; n < 32U; ++n)
            source[n] = static_cast<unsigned char>(n + 1U);
        nx_stm32u575_retained_time_config_t config{
            NX_U575_RETAINED_TIME_RTC, 86400U, nx_stm32u575_stop2_identity()};
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32u575_retained_time_initialize_alarm_a_copy(
                      &clock_storage, &config, &clock_face));
        static bool initialized;
        if (!initialized) {
            ASSERT_EQ(NX_SUCCESS,
                      nx_stm32u575_stop2_initialize(4U, &clock_storage));
            initialized = true;
        }
        nx_stm32u575_lpdma_copy_config_t binding{&clock_storage,
                                                 nx_stm32u575_stop2_identity()};
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_initialize(
                                  &storage, &binding, &copy));
        nx_stm32u575_retained_time_snapshot_t snapshot{};
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32u575_retained_time_sample(&clock_storage, &snapshot));
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_request_prepare(
                                  &request, &snapshot, source, destination, 17U,
                                  workspace, NX_DEADLINE_NEVER));
        ASSERT_EQ(NX_SUCCESS,
                  nx_clock_domain_close(nx_stm32u575_stop2_domain(), &barrier));
        mask = nx_arch_irq_save();
        writes.clear();
    }
    void TearDown() override {
        hold_alarm_disable = hold_wut_disable = hold_alarm_clear =
            hold_rtc_irq = false;
        final_irq_race = hold_alarm_program = false;
        model.hold_suspend = model.hold_reset = model.hold_clear =
            model.hold_irq = false;
        model.link_pending = false;
        model.reg(&RCC->CFGR2) = 0U;
        model.reg(&RCC->APB3ENR) = RCC_APB3ENR_RTCAPBEN;
        model.reg(&RCC->AHB3RSTR) = 0U;
        auto recovered = nx_stm32u575_stop2_recover(barrier, 8U);
        EXPECT_EQ(0U, recovered.remaining_effects);
        EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_stop(&copy, 8U));
        nx_arch_irq_restore(mask);
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_stop(&clock_storage));
        ASSERT_EQ(NX_SUCCESS, nx_clock_domain_reopen(barrier));
    }
    nx_stm32u575_stop2_copy_result_t enter(uint32_t ticks = 4096U) {
        return nx_stm32u575_stop2_copy_enter(barrier, &copy, &request, ticks,
                                             8U);
    }
    void settled(nx_result_t expected) {
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        EXPECT_EQ(expected, request.base.result);
        EXPECT_EQ(0U, model.reg(&RTC->SR));
        EXPECT_EQ(RTC_CR_BYPSHAD, model.reg(&RTC->CR));
        EXPECT_EQ(0U, model.reg(&RTC->ALRMAR));
        EXPECT_EQ(0U, model.reg(&RTC->ALRMASSR));
        EXPECT_EQ(0U, model.reg(&NVIC->ISER[0]) | model.reg(&NVIC->ISPR[0]) |
                          model.reg(&NVIC->ISER[3]) |
                          model.reg(&NVIC->ISPR[3]));
        EXPECT_EQ(0U, model.reg(&RCC->APB3SMENR));
        EXPECT_EQ(0U, model.reg(&RCC->SRDAMR));
        EXPECT_EQ(0U, model.reg(&RCC->AHB3SMENR));
        EXPECT_EQ(0U, protected_refusals);
        EXPECT_TRUE(locked);
        EXPECT_EQ(1U, nx_arch_irq_masks().primask);
    }
};
TEST_F(U575Stop2Copy, RealOwnersCopyOddLengthAndRestoreEveryOwnedField) {
    auto r = enter();
    ASSERT_TRUE(r.accepted);
    EXPECT_EQ(NX_SUCCESS, r.transition.primary);
    EXPECT_EQ(NX_SUCCESS, r.transition.cleanup);
    EXPECT_EQ(0U, r.transition.remaining_effects);
    EXPECT_TRUE(r.dma_wake);
    EXPECT_TRUE(r.transition.wait_executed);
    EXPECT_EQ(1U, waits);
    EXPECT_EQ(17U, request.base.transferred);
    EXPECT_EQ(0, std::memcmp(source, destination, 17U));
    EXPECT_EQ(0xa5U, destination[17]);
    EXPECT_EQ(r.transition.before.generation, r.transition.after.generation);
    EXPECT_EQ(1000000U, r.transition.after.now - r.transition.before.now);
    settled(NX_SUCCESS);
}
TEST_F(U575Stop2Copy, AcceptedRtcSetupFailureSettlesOnlyAfterTheFullDrain) {
    hold_alarm_program = true;
    auto result = enter();
    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(NX_ERROR_IO, result.transition.primary);
    EXPECT_EQ(NX_SUCCESS, result.transition.cleanup);
    EXPECT_EQ(0U, result.transition.remaining_effects);
    EXPECT_FALSE(result.transition.wait_executed);
    EXPECT_EQ(0U, waits);
    EXPECT_EQ(0U, request.base.transferred);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Copy,
       DmaErrorIsTheRequestResultEvenWhenTheWakeBoundarySucceeded) {
    dma_error = true;
    auto result = enter();
    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(NX_SUCCESS, result.transition.primary);
    EXPECT_EQ(NX_SUCCESS, result.transition.cleanup);
    EXPECT_EQ(0U, result.transition.remaining_effects);
    EXPECT_TRUE(result.dma_wake);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Copy, EarlyAlarmCompletionIsNeverClearedBeforeWfi) {
    early_alarm = true;
    auto r = enter();
    ASSERT_TRUE(r.accepted);
    EXPECT_TRUE(r.dma_wake);
    EXPECT_EQ(NX_SUCCESS, r.transition.cleanup);
    EXPECT_EQ(17U, request.base.transferred);
    settled(NX_SUCCESS);
}
TEST_F(U575Stop2Copy, EscapeWutCancelsIncompleteReceiverAndResetsMemorizedHit) {
    escape_only = true;
    auto r = enter();
    ASSERT_TRUE(r.accepted);
    EXPECT_TRUE(r.transition.wake_flag);
    EXPECT_EQ(NX_ERROR_TIMEOUT, r.transition.primary);
    EXPECT_EQ(0U, r.transition.remaining_effects);
    EXPECT_EQ(0U, request.base.transferred);
    EXPECT_GE(model.resets, 1U);
    settled(NX_ERROR_TIMEOUT);
}
TEST_F(U575Stop2Copy,
       PartialDataAndPendingLinkRemainBorrowedUntilSuspendedReset) {
    half_copy = escape_only = true;
    model.link_pending = true;
    auto r = enter();
    ASSERT_TRUE(r.accepted);
    EXPECT_NE(0U, r.transition.remaining_effects);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_STATE, enter().transition.primary);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_stop2_enter(barrier, 2U, 8U).primary);
    model.link_pending = false;
    auto recovered = nx_stm32u575_stop2_recover(barrier, 8U);
    EXPECT_EQ(0U, recovered.remaining_effects);
    EXPECT_EQ(1U, waits);
    settled(NX_ERROR_TIMEOUT);
}
TEST_F(U575Stop2Copy, AlarmWithdrawalFailureCannotResetOrSettleDma) {
    hold_alarm_disable = true;
    auto r = enter();
    ASSERT_TRUE(r.accepted);
    EXPECT_NE(0U, r.transition.remaining_effects);
    EXPECT_EQ(0U, model.resets);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    hold_alarm_disable = false;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 8U).remaining_effects);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Copy, LateAlarmFlagMustBeObservedQuietBeforeDmaReset) {
    late_alarm = hold_alarm_clear = true;
    auto r = enter();
    ASSERT_TRUE(r.accepted);
    EXPECT_NE(0U, r.transition.remaining_effects);
    EXPECT_EQ(0U, model.resets);
    hold_alarm_clear = false;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 8U).remaining_effects);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Copy,
       BadWakeProfileTouchesNeitherRtcNorDmaUntilExternalRestore) {
    bad_wake_profile = true;
    auto r = enter();
    ASSERT_TRUE(r.accepted);
    EXPECT_EQ(NX_ERROR_IO, r.transition.primary);
    EXPECT_NE(0U, r.transition.remaining_effects);
    EXPECT_EQ(0U, forbidden_accesses);
    EXPECT_EQ(0U, model.reg(&NVIC->ISER[0]) | model.reg(&NVIC->ISER[3]));
    auto retried = nx_stm32u575_stop2_recover(barrier, 8U);
    EXPECT_NE(0U, retried.remaining_effects);
    EXPECT_EQ(0U, forbidden_accesses);
    model.reg(&RCC->CFGR2) = 0U;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 8U).remaining_effects);
    EXPECT_EQ(1U, waits);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Copy, LostRtcAccessGateQuarantinesWithoutAnyPeripheralAccess) {
    bad_rtc_gate = true;
    auto result = enter();
    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(NX_ERROR_IO, result.transition.primary);
    EXPECT_NE(0U, result.transition.remaining_effects);
    EXPECT_EQ(0U, forbidden_accesses);
    model.reg(&RCC->APB3ENR) = RCC_APB3ENR_RTCAPBEN;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 8U).remaining_effects);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Copy,
       ObservedExternalDmaResetPermanentlyRetainsTheUnprovedLoan) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_EXIT(
        {
            bad_dma_reset = true;
            auto result = enter();
            bool held = result.accepted &&
                        result.transition.remaining_effects != 0U &&
                        forbidden_accesses == 0U;
            /* Reset may erase the channel without proving its previous master
             * transaction. Quiet reset values cannot manufacture a final-access
             * proof. */
            model.reg(&RCC->AHB3RSTR) = 0U;
            model.reg(&LPDMA1_Channel0->CCR) = 0U;
            model.reg(&LPDMA1_Channel0->CSR) = DMA_CSR_IDLEF;
            model.reg(&LPDMA1_Channel0->CLLR) = 0U;
            auto recovered = nx_stm32u575_stop2_recover(barrier, 8U);
            held = held && recovered.remaining_effects != 0U && waits == 1U &&
                   nx_request_state(&request.base) != NX_REQUEST_SETTLED &&
                   enter().transition.primary == NX_ERROR_STATE &&
                   nx_stm32u575_stop2_enter(barrier, 2U, 8U).primary ==
                       NX_ERROR_STATE;
            std::_Exit(held ? 0 : 1);
        },
        testing::ExitedWithCode(0), "");
}
TEST_F(U575Stop2Copy, TaintedClockCanSettleErrorAfterIndependentHardwareDrain) {
    drift_after_wait = true;
    auto r = enter();
    ASSERT_TRUE(r.accepted);
    EXPECT_EQ(NX_ERROR_STATE, r.transition.primary);
    EXPECT_EQ(NX_SUCCESS, r.transition.cleanup);
    EXPECT_EQ(0U, r.transition.remaining_effects);
    nx_stm32u575_retained_time_snapshot_t out{};
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_retained_time_sample(&clock_storage, &out));
    settled(NX_ERROR_STATE);
}
TEST_F(U575Stop2Copy,
       FinalReleaseFailureRetainsVisibleLoanAndRecoverRetriesWithoutWfi) {
    final_irq_race = true;
    auto r = enter();
    ASSERT_TRUE(r.accepted);
    EXPECT_NE(0U, r.transition.remaining_effects);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_STATE, enter().transition.primary);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_stop2_enter(barrier, 2U, 8U).primary);
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 8U).remaining_effects);
    EXPECT_EQ(1U, waits);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Copy, DmaResetFailureRetainsEveryReferenceAndOriginalGates) {
    model.hold_reset = true;
    auto r = enter();
    ASSERT_TRUE(r.accepted);
    EXPECT_NE(0U, r.transition.remaining_effects);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(auto_bits | RCC_SRDAMR_RTCAPBAMEN, model.reg(&RCC->SRDAMR));
    model.hold_reset = false;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 8U).remaining_effects);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Copy, WrongClockGenerationAndForeignFaceRejectBeforeAdmission) {
    ++request.clock_generation;
    auto r = enter();
    EXPECT_FALSE(r.accepted);
    EXPECT_EQ(NX_ERROR_STATE, r.transition.primary);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_TRUE(writes.empty());
    --request.clock_generation;
    auto foreign = copy;
    foreign.ops = nullptr;
    r = nx_stm32u575_stop2_copy_enter(barrier, &foreign, &request, 4096U, 8U);
    EXPECT_FALSE(r.accepted);
    EXPECT_EQ(NX_ERROR_INVALID, r.transition.primary);
    EXPECT_TRUE(writes.empty());
}
TEST_F(U575Stop2Copy,
       RunningSystemAndUnjoinedLeaseRejectWithoutWorkspaceWrite) {
    auto before = *workspace;
    g_nx_stm32u575_system.started = true;
    auto r = enter();
    EXPECT_FALSE(r.accepted);
    EXPECT_EQ(NX_ERROR_BUSY, r.transition.primary);
    g_nx_stm32u575_system.started = false;
    nx_arch_irq_restore(mask);
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_reopen(barrier));
    nx_clock_lease_t lease{};
    ASSERT_EQ(NX_SUCCESS,
              nx_clock_domain_acquire(nx_stm32u575_stop2_domain(), &lease));
    ASSERT_EQ(NX_SUCCESS,
              nx_clock_domain_close(nx_stm32u575_stop2_domain(), &barrier));
    mask = nx_arch_irq_save();
    EXPECT_EQ(NX_ERROR_BUSY, enter().transition.primary);
    ASSERT_EQ(NX_SUCCESS,
              nx_clock_domain_release(nx_stm32u575_stop2_domain(), &lease));
    EXPECT_EQ(0, std::memcmp(&before, workspace, sizeof(before)));
    EXPECT_TRUE(writes.empty());
}
TEST_F(U575Stop2Copy, RejectsShortEscapeAndRange1BeforeAnyHardwareEffect) {
    EXPECT_EQ(NX_ERROR_INVALID, enter(4095U).transition.primary);
    model.reg(&PWR->VOSR) |= PWR_VOSR_VOS;
    auto r = enter();
    EXPECT_FALSE(r.accepted);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, r.transition.primary);
    EXPECT_TRUE(writes.empty());
    EXPECT_EQ(0U, waits);
}
} /* namespace */
extern "C" uint32_t nx_u575_stop2_control(void) {
    return 0U;
}
extern "C" uint32_t nx_u575_lpdma_copy_read(uintptr_t address) {
    account(address);
    return nx_u575_lpdma_copy_bus_read(address);
}
extern "C" void nx_u575_lpdma_copy_write(uintptr_t address, uint32_t value) {
    account(address);
    writes.push_back(address);
    nx_u575_lpdma_copy_bus_write(address, value);
}
extern "C" uint32_t nx_u575_stop2_read(uintptr_t address) {
    account(address);
    return model.values[address];
}
extern "C" void nx_u575_stop2_write(uintptr_t address, uint32_t value) {
    account(address);
    writes.push_back(address);
    auto& current = model.values[address];
    if (address == A(NVIC->ISER[3]) || address == A(NVIC->ICER[3]) ||
        address == A(NVIC->ICPR[3])) {
        if (address == A(NVIC->ICPR[3]) && started &&
            (model.reg(&RTC->CR) & RTC_CR_ALRAE) != 0U) {
            ADD_FAILURE() << "DMA pending cleared after producer arm";
        }
        nx_u575_lpdma_copy_bus_write(address, value);
        return;
    }
    if (address == A(RTC->WPR)) {
        if (value == 0xcaU)
            key = 1U;
        else if (key == 1U && value == 0x53U)
            locked = false;
        else {
            locked = true;
            key = 0U;
        }
        return;
    }
    if ((address == A(RTC->CR) || address == A(RTC->WUTR) ||
         address == A(RTC->ALRMAR) || address == A(RTC->ALRMASSR)) &&
        locked) {
        ++protected_refusals;
        return;
    }
    if (address == A(RTC->ALRMAR) && value == alarm_mask && hold_alarm_program)
        return;
    if (address == A(RTC->ALRMAR) || address == A(RTC->ALRMASSR)) {
        if ((model.reg(&RTC->CR) & RTC_CR_ALRAE) != 0U) {
            ++protected_refusals;
            return;
        }
    }
    if (address == A(RTC->CR)) {
        if ((hold_alarm_disable && (current & RTC_CR_ALRAE) != 0U &&
             (value & RTC_CR_ALRAE) == 0U) ||
            (hold_wut_disable && (current & RTC_CR_WUTE) != 0U &&
             (value & RTC_CR_WUTE) == 0U))
            return;
        bool disabling =
            (current & RTC_CR_ALRAE) != 0U && (value & RTC_CR_ALRAE) == 0U;
        bool enabling =
            (current & RTC_CR_ALRAE) == 0U && (value & RTC_CR_ALRAE) != 0U;
        current = value;
        model.reg(&RTC->ICSR) =
            (value & RTC_CR_WUTE) == 0U ? RTC_ICSR_WUTWF : 0U;
        if (disabling && late_alarm)
            model.reg(&RTC->SR) |= RTC_SR_ALRAF;
        if (enabling) {
            started = true;
            EXPECT_EQ(alarm_mask, model.reg(&RTC->ALRMAR));
            EXPECT_EQ(0U, model.reg(&RTC->ALRMASSR));
            EXPECT_EQ(0U, value & (RTC_CR_ALRAIE | RTC_CR_ALRAFCLR));
            EXPECT_NE(0U, model.reg(&LPDMA1_Channel0->CCR) & DMA_CCR_EN);
            if (early_alarm)
                alarm();
        }
        return;
    }
    if (address == A(RTC->SCR)) {
        if (started && (model.reg(&RTC->CR) & RTC_CR_ALRAE) != 0U)
            ADD_FAILURE() << "RTC producer flag cleared after arm";
        uint32_t clear = hold_alarm_clear
                             ? value & ~static_cast<uint32_t>(RTC_SCR_CALRAF)
                             : value;
        model.reg(&RTC->SR) &= ~clear;
        return;
    }
    if (address == A(NVIC->ISER[0])) {
        current |= value;
        return;
    }
    if (address == A(NVIC->ICER[0])) {
        model.reg(&NVIC->ISER[0]) &= ~value;
        return;
    }
    if (address == A(NVIC->ICPR[0])) {
        if (started && (model.reg(&RTC->CR) & RTC_CR_ALRAE) != 0U)
            ADD_FAILURE() << "RTC pending cleared after arm";
        if (!hold_rtc_irq)
            model.reg(&NVIC->ISPR[0]) &= ~value;
        return;
    }
    if (address == A(RCC->CR)) {
        current = value | RCC_CR_MSISRDY;
        if ((value & RCC_CR_MSIKON) != 0U)
            current |= RCC_CR_MSIKRDY;
        else
            current &= ~static_cast<uint32_t>(RCC_CR_MSIKRDY);
        return;
    }
    current = value;
    if (address == A(RCC->SRDAMR) && started && model.resets != 0U &&
        (value & RCC_SRDAMR_RTCAPBAMEN) == 0U && final_irq_race) {
        final_irq_race = false;
        model.reg(&NVIC->ISPR[0]) |= rtc_irq;
    }
}
extern "C" void nx_u575_stop2_wait(void) {
    ++waits;
    EXPECT_EQ(1U, nx_arch_irq_masks().primask);
    EXPECT_EQ(sleep_bits, model.reg(&RCC->AHB3SMENR) & sleep_bits);
    EXPECT_EQ(auto_bits | RCC_SRDAMR_RTCAPBAMEN, model.reg(&RCC->SRDAMR));
    EXPECT_EQ(RCC_APB3SMENR_RTCAPBSMEN, model.reg(&RCC->APB3SMENR));
    EXPECT_NE(0U, model.reg(&SCB->SCR) & SCB_SCR_SLEEPDEEP_Msk);
    EXPECT_NE(0U, model.reg(&RTC->CR) & RTC_CR_ALRAE);
    if (early_alarm) {
        EXPECT_NE(0U, model.reg(&NVIC->ISPR[3]) & dma_irq);
    }
    if (!escape_only || half_copy)
        alarm();
    if (dma_error)
        model.reg(&LPDMA1_Channel0->CSR) |= DMA_CSR_TOF;
    if (escape_only) {
        model.reg(&RTC->SR) |= RTC_SR_WUTF;
        model.reg(&NVIC->ISPR[0]) |= rtc_irq;
    }
    model.reg(&RCC->CR) |= RCC_CR_MSIKON | RCC_CR_MSIKRDY;
    model.reg(&RTC->TR) = escape_only ? 2U : 1U;
    if (bad_wake_profile)
        model.reg(&RCC->CFGR2) = RCC_CFGR2_HPRE;
    if (drift_after_wait)
        model.reg(&RTC->PRER) = 0U;
    if (bad_rtc_gate)
        model.reg(&RCC->APB3ENR) = 0U;
    if (bad_dma_reset)
        model.reg(&RCC->AHB3RSTR) = RCC_AHB3RSTR_LPDMA1RST;
}
