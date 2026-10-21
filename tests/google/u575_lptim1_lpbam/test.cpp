/** \file test.cpp
 * \brief Real retained-clock, Stop2 and finite LPTIM1 compound contracts.
 * \author Nexus Team
 * \version 1.0.0
 * \date 2026-10-20
 * \copyright Copyright (c) 2026 Nexus Team
 */
#include "model.h"
#include "nexus/arch/arch.h"
#include "nexus/components/stm32u575_stop2_lptim1.h"
#include "stm32u575_system.h"
#include <algorithm>
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
constexpr uint32_t write_ok =
    LPTIM_ISR_CMP1OK | LPTIM_ISR_REPOK | LPTIM_ISR_ARROK;
constexpr uint32_t sleep_bits =
    RCC_AHB3SMENR_LPDMA1SMEN | RCC_AHB3SMENR_SRAM4SMEN;
constexpr uint32_t auto_bits =
    RCC_SRDAMR_LPDMA1AMEN | RCC_SRDAMR_SRAM4AMEN | RCC_SRDAMR_LPTIM1AMEN;
nx_stm32u575_retained_time_storage_t clock_storage{};
nx_clock_t clock_face{};
unsigned waits, key, protected_refusals, forbidden_accesses, ssr_reads;
unsigned timer_starts, peripheral_resets, enable_clears, alarm_writes;
bool locked, hold_ue_disable, hold_bootstrap_ok, hold_peripheral_reset;
bool bad_wake_profile, freeze_after_enable, hold_peripheral_deassert;
bool escape_only, partial_tuple, bad_dma_reset, bad_lptim_reset;
bool cancel_during_wait, pending_dier;
bool pending_hint_only, late_complete_on_withdraw;
unsigned dier_ok_losses;
bool hold_source_ready, hold_source_disable, hold_gate_disable;
const nx_stm32u575_lpdma_lptim1_t* active_receiver;
nx_stm32u575_lpdma_lptim1_request_t* active_request;
std::vector<std::pair<uintptr_t, uint32_t>> writes;
bool peripheral(uintptr_t address) {
    return (address >= RTC_BASE_NS &&
            address < RTC_BASE_NS + sizeof(RTC_TypeDef)) ||
           (address >= LPDMA1_BASE_NS && address < LPDMA1_BASE_NS + 0x400U) ||
           (address >= LPTIM1_BASE_NS &&
            address < LPTIM1_BASE_NS + sizeof(LPTIM_TypeDef));
}
void account(uintptr_t address) {
    bool lptim_reset =
        (model.reg(&RCC->APB3RSTR) & RCC_APB3RSTR_LPTIM1RST) != 0U;
    bool lptim_mmio = address >= LPTIM1_BASE_NS &&
                      address < LPTIM1_BASE_NS + sizeof(LPTIM_TypeDef);
    if (waits != 0U && peripheral(address) &&
        (model.reg(&RCC->CFGR2) != 0U ||
         (model.reg(&RCC->APB3ENR) & RCC_APB3ENR_RTCAPBEN) == 0U ||
         (model.reg(&RCC->AHB3RSTR) & RCC_AHB3RSTR_LPDMA1RST) != 0U ||
         (lptim_reset && (bad_lptim_reset || lptim_mmio))))
        ++forbidden_accesses;
}
void port_write(uintptr_t address, uint32_t value) {
    account(address);
    writes.emplace_back(address, value);
    auto& current = model.values[address];
    if ((address >= LPDMA1_BASE_NS && address < LPDMA1_BASE_NS + 0x400U) ||
        address == A(NVIC->ISER[3]) || address == A(NVIC->ICER[3]) ||
        address == A(NVIC->ICPR[3])) {
        if (address == A(LPDMA1_Channel0->CCR) &&
            (value & DMA_CCR_RESET) != 0U) {
            EXPECT_EQ(0U, model.reg(&LPTIM1->DIER) & LPTIM_DIER_UEDE)
                << "DMA reset before producer withdrawal";
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
    if (address == A(RTC->ALRMAR) || address == A(RTC->ALRMASSR) ||
        (address == A(RTC->CR) &&
         (value & (RTC_CR_ALRAE | RTC_CR_ALRAIE | RTC_CR_ALRAFCLR)) != 0U))
        ++alarm_writes;
    if ((address == A(RTC->CR) || address == A(RTC->WUTR) ||
         address == A(RTC->ALRMAR) || address == A(RTC->ALRMASSR)) &&
        locked) {
        ++protected_refusals;
        return;
    }
    if (address == A(RTC->CR)) {
        current = value;
        model.reg(&RTC->ICSR) =
            (value & RTC_CR_WUTE) == 0U ? RTC_ICSR_WUTWF : 0U;
        return;
    }
    if (address == A(RTC->SCR)) {
        EXPECT_EQ(0U, value & RTC_SCR_CALRAF);
        model.reg(&RTC->SR) &= ~value;
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
        model.reg(&NVIC->ISPR[0]) &= ~value;
        return;
    }
    if (address == A(NVIC->ICER[2])) {
        model.reg(&NVIC->ISER[2]) &= ~value;
        return;
    }
    if (address == A(NVIC->ICPR[2])) {
        model.reg(&NVIC->ISPR[2]) &= ~value;
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
    if (address == A(RCC->APB3ENR) && hold_gate_disable &&
        (current & RCC_APB3ENR_LPTIM1EN) != 0U &&
        (value & RCC_APB3ENR_LPTIM1EN) == 0U)
        return;
    if (address == A(RCC->BDCR)) {
        if (hold_source_disable && (current & RCC_BDCR_LSESYSEN) != 0U &&
            (value & RCC_BDCR_LSESYSEN) == 0U)
            return;
        current = value;
        if ((value & RCC_BDCR_LSESYSEN) != 0U && !hold_source_ready)
            current |= RCC_BDCR_LSESYSRDY;
        else
            current &= ~static_cast<uint32_t>(RCC_BDCR_LSESYSRDY);
        return;
    }
    if (address == A(RCC->APB3RSTR)) {
        if (hold_peripheral_deassert &&
            (current & RCC_APB3RSTR_LPTIM1RST) != 0U &&
            (value & RCC_APB3RSTR_LPTIM1RST) == 0U)
            return;
        if (hold_peripheral_reset && (value & RCC_APB3RSTR_LPTIM1RST) != 0U)
            return;
        current = value;
        if ((value & RCC_APB3RSTR_LPTIM1RST) != 0U) {
            ++peripheral_resets;
            if (timer_starts != 0U || !model.lptim_writes.empty()) {
                EXPECT_NE(0U, model.resets);
                EXPECT_EQ(0U, model.reg(&LPDMA1_Channel0->CCR));
                EXPECT_EQ(0U, model.reg(&LPDMA1_Channel0->CSAR));
                EXPECT_EQ(0U, model.reg(&LPDMA1_Channel0->CDAR));
                EXPECT_EQ(0U, model.reg(&LPDMA1_Channel0->CLLR));
            }
            for (uintptr_t offset = 0U; offset < sizeof(LPTIM_TypeDef);
                 offset += sizeof(uint32_t))
                model.values[LPTIM1_BASE_NS + offset] = 0U;
            model.reg(&LPTIM1->ARR) = 1U;
        }
        return;
    }
    if (address == A(LPTIM1->CR)) {
        if ((current & LPTIM_CR_ENABLE) != 0U &&
            (value & LPTIM_CR_ENABLE) == 0U)
            ++enable_clears;
        if ((value & LPTIM_CR_ENABLE) != 0U &&
            (current & LPTIM_CR_ENABLE) == 0U) {
            ssr_reads = 0U;
            /* Elapsed time before ENABLE becomes effective must not count
             * toward its two-cycle authorization delay. */
            if (freeze_after_enable)
                model.reg(&RTC->SSR) -= 4U;
        }
        if ((value & LPTIM_CR_CNTSTRT) != 0U) {
            ++timer_starts;
            EXPECT_NE(0U, model.reg(&RTC->CR) & RTC_CR_WUTE);
            EXPECT_NE(0U, model.reg(&LPDMA1_Channel0->CCR) & DMA_CCR_EN);
            EXPECT_NE(0U, model.reg(&LPTIM1->DIER) & LPTIM_DIER_UEDE);
        }
        current =
            value & ~static_cast<uint32_t>(LPTIM_CR_CNTSTRT | LPTIM_CR_SNGSTRT);
        return;
    }
    if (address == A(LPTIM1->DIER)) {
        EXPECT_NE(0U, model.reg(&LPTIM1->CR) & LPTIM_CR_ENABLE);
        if (hold_ue_disable && (current & LPTIM_DIER_UEDE) != 0U &&
            (value & LPTIM_DIER_UEDE) == 0U) {
            pending_dier = true;
            return;
        }
        if (late_complete_on_withdraw && (current & LPTIM_DIER_UEDE) != 0U &&
            (value & LPTIM_DIER_UEDE) == 0U) {
            late_complete_on_withdraw = false;
            model.lptim_event();
        }
        /* ES0499 2.17.3: conservatively model loss of every write-OK on
         * any DIER write, including unchanged zero interrupt-enable bits. */
        if ((model.reg(&LPTIM1->ISR) & write_ok) != 0U)
            ++dier_ok_losses;
        model.reg(&LPTIM1->ISR) &= ~write_ok;
        current = value;
        model.reg(&LPTIM1->ISR) |= LPTIM_ISR_DIEROK;
        return;
    }
    if (address == A(LPTIM1->ICR)) {
        model.reg(&LPTIM1->ISR) &= ~value;
        return;
    }
    if (address == A(LPTIM1->CCR1) || address == A(LPTIM1->RCR) ||
        address == A(LPTIM1->ARR)) {
        EXPECT_NE(0U, model.reg(&LPTIM1->CR) & LPTIM_CR_ENABLE);
        EXPECT_GE(ssr_reads, 4U) << "bootstrap skipped retained enable delay";
        if (hold_bootstrap_ok && address == A(LPTIM1->ARR))
            return;
        current = value;
        uint32_t flag = address == A(LPTIM1->CCR1)
                            ? LPTIM_ISR_CMP1OK
                            : (address == A(LPTIM1->RCR) ? LPTIM_ISR_REPOK
                                                         : LPTIM_ISR_ARROK);
        model.reg(&LPTIM1->ISR) |= flag;
        return;
    }
    current = value;
}
uint32_t port_read(uintptr_t address) {
    account(address);
    /* A stalled APB-to-kernel DIER write completes once its hardware hold is
     * removed. The provider must wait for this actual acknowledgement before
     * issuing another DIER write. */
    if (address == A(LPTIM1->ISR) && pending_dier && !hold_ue_disable) {
        model.reg(&LPTIM1->DIER) = 0U;
        if ((model.reg(&LPTIM1->ISR) & write_ok) != 0U)
            ++dier_ok_losses;
        model.reg(&LPTIM1->ISR) &= ~write_ok;
        model.reg(&LPTIM1->ISR) |= LPTIM_ISR_DIEROK;
        pending_dier = false;
    }
    uint32_t value = nx_u575_lpdma_copy_bus_read(address);
    /* Two equal SSR reads form one stable calendar tuple. Advance only after
     * that group, while the real driver is proving its enable delay. This is
     * deterministic modeled clock progress, never physical timing evidence. */
    if (address == A(RTC->SSR) &&
        (model.reg(&LPTIM1->CR) & LPTIM_CR_ENABLE) != 0U) {
        ++ssr_reads;
        if (!freeze_after_enable && (ssr_reads & 1U) == 0U &&
            model.reg(&RTC->SSR) != 0U)
            --model.reg(&RTC->SSR);
    }
    return value;
}
class U575Stop2Lptim1 : public testing::Test {
  protected:
    nx_stm32u575_lpdma_lptim1_storage_t storage{};
    nx_stm32u575_lpdma_lptim1_t receiver{};
    nx_stm32u575_lpdma_lptim1_request_t request{};
    nx_clock_barrier_t barrier{};
    nx_arch_irq_state_t mask{};
    nx_stm32u575_lptim1_tuple_t* tuple =
        reinterpret_cast<nx_stm32u575_lptim1_tuple_t*>(SRAM4_BASE_NS + 0x100U);
    nx_stm32u575_lpdma_lptim1_workspace_t* workspace =
        reinterpret_cast<nx_stm32u575_lpdma_lptim1_workspace_t*>(SRAM4_BASE_NS +
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
        waits = key = protected_refusals = forbidden_accesses = ssr_reads = 0U;
        timer_starts = peripheral_resets = enable_clears = alarm_writes = 0U;
        locked = true;
        hold_ue_disable = hold_bootstrap_ok = hold_peripheral_reset = false;
        bad_wake_profile = freeze_after_enable = hold_peripheral_deassert =
            false;
        escape_only = partial_tuple = bad_dma_reset = bad_lptim_reset = false;
        cancel_during_wait = pending_dier = false;
        pending_hint_only = late_complete_on_withdraw = false;
        dier_ok_losses = 0U;
        hold_source_ready = hold_source_disable = hold_gate_disable = false;
        active_receiver = &receiver;
        active_request = &request;
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
        *tuple = {37U, 0U, 2047U};
        nx_stm32u575_retained_time_config_t config{
            NX_U575_RETAINED_TIME_RTC, 86400U, nx_stm32u575_stop2_identity()};
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_initialize(
                                  &clock_storage, &config, &clock_face));
        static bool initialized;
        if (!initialized) {
            ASSERT_EQ(NX_SUCCESS,
                      nx_stm32u575_stop2_initialize(4U, &clock_storage));
            initialized = true;
        }
        nx_stm32u575_lpdma_lptim1_config_t binding{
            &clock_storage, nx_stm32u575_stop2_identity()};
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_lptim1_initialize(
                                  &storage, &binding, &receiver));
        ASSERT_EQ(0U, model.reg(&RCC->APB3ENR) & RCC_APB3ENR_LPTIM1EN);
        ASSERT_EQ(0U, model.reg(&RCC->CCIPR3));
        ASSERT_EQ(0U, model.reg(&RCC->BDCR) & RCC_BDCR_LSESYSEN);
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_lptim1_start(&receiver, 16U));
        ASSERT_EQ(0U, model.reg(&LPTIM1->CR));
        ASSERT_EQ(0U, timer_starts);
        nx_stm32u575_retained_time_snapshot_t snapshot{};
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32u575_retained_time_sample(&clock_storage, &snapshot));
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_lptim1_request_prepare(
                                  &request, &snapshot, tuple, workspace,
                                  NX_DEADLINE_NEVER));
        ASSERT_EQ(NX_SUCCESS,
                  nx_clock_domain_close(nx_stm32u575_stop2_domain(), &barrier));
        mask = nx_arch_irq_save();
        peripheral_resets = 0U;
        writes.clear();
    }
    void TearDown() override {
        hold_source_ready = hold_source_disable = hold_gate_disable = false;
        hold_ue_disable = hold_bootstrap_ok = hold_peripheral_reset = false;
        bad_wake_profile = freeze_after_enable = hold_peripheral_deassert =
            false;
        model.hold_suspend = model.hold_reset = model.hold_clear =
            model.hold_irq = model.hold_write_ok = false;
        model.link_pending = false;
        model.reg(&RCC->CFGR2) = 0U;
        model.reg(&RCC->APB3ENR) |= RCC_APB3ENR_RTCAPBEN;
        model.reg(&RCC->AHB3RSTR) = 0U;
        model.reg(&RCC->APB3RSTR) = 0U;
        auto recovered = nx_stm32u575_stop2_recover(barrier, 16U);
        EXPECT_EQ(0U, recovered.remaining_effects);
        nx_arch_irq_restore(mask);
        EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_lptim1_stop(&receiver, 16U));
        EXPECT_EQ(0U, model.reg(&RCC->APB3ENR) & RCC_APB3ENR_LPTIM1EN);
        EXPECT_EQ(0U, model.reg(&RCC->CCIPR3) & RCC_CCIPR3_LPTIM1SEL);
        EXPECT_EQ(0U, model.reg(&RCC->BDCR) & RCC_BDCR_LSESYSEN);
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_retained_time_stop(&clock_storage));
        ASSERT_EQ(NX_SUCCESS, nx_clock_domain_reopen(barrier));
    }
    void fresh_unstarted() {
        nx_arch_irq_restore(mask);
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_lptim1_stop(&receiver, 16U));
        storage = {};
        receiver = {};
        nx_stm32u575_lpdma_lptim1_config_t binding{
            &clock_storage, nx_stm32u575_stop2_identity()};
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_lptim1_initialize(
                                  &storage, &binding, &receiver));
    }
    void other_owner_busy() {
        nx_stm32u575_lpdma_lptim1_storage_t other_storage{};
        nx_stm32u575_lpdma_lptim1_t other{};
        nx_stm32u575_lpdma_lptim1_config_t binding{
            &clock_storage, nx_stm32u575_stop2_identity()};
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_lptim1_initialize(
                                     &other_storage, &binding, &other));
        EXPECT_EQ(nullptr, other.ops);
    }
    nx_stm32u575_stop2_lptim1_result_t enter(uint32_t ticks = 4096U) {
        return nx_stm32u575_stop2_lptim1_enter(barrier, &receiver, &request,
                                               ticks, 16U);
    }
    void settled(nx_result_t expected) {
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        EXPECT_EQ(expected, request.base.result);
        EXPECT_EQ(0U, model.reg(&RTC->SR));
        EXPECT_EQ(RTC_CR_BYPSHAD, model.reg(&RTC->CR));
        EXPECT_EQ(0U,
                  model.reg(&NVIC->ISER[0]) | model.reg(&NVIC->ISPR[0]) |
                      model.reg(&NVIC->ISER[2]) | model.reg(&NVIC->ISPR[2]) |
                      model.reg(&NVIC->ISER[3]) | model.reg(&NVIC->ISPR[3]));
        EXPECT_EQ(0U, model.reg(&RCC->APB3SMENR));
        EXPECT_EQ(0U, model.reg(&RCC->SRDAMR));
        EXPECT_EQ(0U, model.reg(&RCC->AHB3SMENR));
        EXPECT_EQ(0U, protected_refusals);
        EXPECT_EQ(0U, enable_clears);
        EXPECT_EQ(0U, alarm_writes);
        EXPECT_TRUE(locked);
        EXPECT_EQ(1U, nx_arch_irq_masks().primask);
    }
    void held(const nx_stm32u575_stop2_lptim1_result_t& result) {
        ASSERT_TRUE(result.accepted);
        EXPECT_NE(0U, result.transition.remaining_effects);
        EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        EXPECT_EQ(NX_ERROR_STATE, enter().transition.primary);
        EXPECT_EQ(NX_ERROR_STATE,
                  nx_stm32u575_stop2_enter(barrier, 2U, 16U).primary);
    }
};
TEST_F(U575Stop2Lptim1, FailedSourceStartRetainsClaimUntilSourceRestores) {
    fresh_unstarted();
    auto before = request;
    hold_source_ready = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_lptim1_start(&receiver, 4U));
    EXPECT_NE(0U, model.reg(&RCC->BDCR) & RCC_BDCR_LSESYSEN);
    EXPECT_EQ(0U, model.reg(&RCC->APB3ENR) & RCC_APB3ENR_LPTIM1EN);
    EXPECT_EQ(0, std::memcmp(&before, &request, sizeof(request)));
    other_owner_busy();
    hold_source_ready = false;
    hold_source_disable = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_lptim1_stop(&receiver, 4U));
    other_owner_busy();
    hold_source_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_lptim1_stop(&receiver, 4U));
    EXPECT_EQ(0U, model.reg(&RCC->BDCR) & RCC_BDCR_LSESYSEN);
    mask = nx_arch_irq_save();
}
TEST_F(U575Stop2Lptim1, FailedStartGateWithdrawalKeepsSourceAndReservation) {
    fresh_unstarted();
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_lptim1_start(&receiver, 4U));
    hold_gate_disable = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_lptim1_stop(&receiver, 4U));
    EXPECT_NE(0U, model.reg(&RCC->APB3ENR) & RCC_APB3ENR_LPTIM1EN);
    EXPECT_NE(0U, model.reg(&RCC->BDCR) & RCC_BDCR_LSESYSEN);
    other_owner_busy();
    hold_gate_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_lptim1_stop(&receiver, 4U));
    mask = nx_arch_irq_save();
}
TEST_F(U575Stop2Lptim1,
       ForeignColdStateRejectsWithoutResettingOtherTimerState) {
    fresh_unstarted();
    model.reg(&LPTIM1->RCR) = 7U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_lptim1_start(&receiver, 4U));
    other_owner_busy();
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_lptim1_stop(&receiver, 4U));
    EXPECT_EQ(7U, model.reg(&LPTIM1->RCR));
    EXPECT_EQ(0U, peripheral_resets);
    EXPECT_EQ(0U, timer_starts);
    mask = nx_arch_irq_save();
}
TEST_F(U575Stop2Lptim1, OneTupleUsesThreeSynchronizedFixedWritesAndFullJoin) {
    auto before = *tuple;
    auto result = enter();
    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(NX_SUCCESS, result.transition.primary);
    EXPECT_EQ(NX_SUCCESS, result.transition.cleanup);
    EXPECT_EQ(0U, result.transition.remaining_effects);
    EXPECT_TRUE(result.dma_wake);
    EXPECT_TRUE(result.transition.wait_executed);
    EXPECT_EQ(1U, waits);
    EXPECT_EQ(1U, timer_starts);
    EXPECT_EQ(12U, request.base.transferred);
    ASSERT_EQ(3U, model.lptim_writes.size());
    EXPECT_EQ(std::make_pair(A(LPTIM1->CCR1), tuple->ccr1),
              model.lptim_writes[0]);
    EXPECT_EQ(std::make_pair(A(LPTIM1->RCR), tuple->rcr),
              model.lptim_writes[1]);
    EXPECT_EQ(std::make_pair(A(LPTIM1->ARR), tuple->arr),
              model.lptim_writes[2]);
    EXPECT_EQ(0, std::memcmp(&before, tuple, sizeof(before)));
    EXPECT_NE(0U, peripheral_resets);
    EXPECT_EQ(1U, dier_ok_losses);
    settled(NX_SUCCESS);
}
TEST_F(U575Stop2Lptim1, ServiceBeforeAnyRequestLeavesProducerLive) {
    pending_hint_only = true;
    auto result = enter();
    held(result);
    EXPECT_EQ(NX_SUCCESS, result.transition.primary);
    EXPECT_EQ(NX_ERROR_BUSY, result.transition.cleanup);
    EXPECT_NE(0U, model.reg(&LPTIM1->DIER) & LPTIM_DIER_UEDE);
    EXPECT_EQ(0U, model.resets);
    EXPECT_TRUE(model.lptim_writes.empty());
    /* After enter returns, a normal serialized public control call must not
     * stop the unissued request. Stop2 has retained IO for explicit recovery.
     */
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_lptim1_service(&receiver, 16U));
    EXPECT_NE(0U, model.reg(&LPTIM1->DIER) & LPTIM_DIER_UEDE);
    model.lptim_event();
    EXPECT_EQ(3U, model.lptim_writes.size());
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 16U).remaining_effects);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Lptim1, TimeoutKeepsFirstFailureWhenTcArrivesAtWithdrawal) {
    escape_only = late_complete_on_withdraw = true;
    auto result = enter();
    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.transition.primary);
    EXPECT_EQ(0U, result.transition.remaining_effects);
    EXPECT_EQ(3U, model.lptim_writes.size());
    EXPECT_EQ(12U, request.base.transferred);
    EXPECT_EQ(1U, dier_ok_losses);
    EXPECT_FALSE(late_complete_on_withdraw);
    settled(NX_ERROR_TIMEOUT);
}
TEST_F(U575Stop2Lptim1, CancelKeepsFirstFailureWhenTcArrivesAtWithdrawal) {
    escape_only = cancel_during_wait = late_complete_on_withdraw = true;
    auto result = enter();
    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(0U, result.transition.remaining_effects);
    EXPECT_EQ(3U, model.lptim_writes.size());
    EXPECT_EQ(12U, request.base.transferred);
    EXPECT_EQ(1U, dier_ok_losses);
    settled(NX_ERROR_CANCELLED);
}
TEST_F(U575Stop2Lptim1, ASecondAdmissionCannotReuseFirstWriteOkProof) {
    auto first = enter();
    ASSERT_EQ(0U, first.transition.remaining_effects);
    settled(NX_SUCCESS);
    nx_stm32u575_retained_time_snapshot_t snapshot{};
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32u575_retained_time_sample(&clock_storage, &snapshot));
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_lptim1_request_prepare(
                  &request, &snapshot, tuple, workspace, NX_DEADLINE_NEVER));
    waits = timer_starts = peripheral_resets = 0U;
    model.lptim_writes.clear();
    model.hold_write_ok = true;
    auto second = enter();
    held(second);
    EXPECT_NE(0U, model.reg(&LPTIM1->DIER) & LPTIM_DIER_UEDE);
    EXPECT_EQ(3U, model.lptim_writes.size());
    EXPECT_EQ(0U, peripheral_resets);
    EXPECT_NE(0U, model.reg(&LPTIM1->DIER) & LPTIM_DIER_UEDE);
    model.hold_write_ok = false;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 16U).remaining_effects);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Lptim1, IgnoredBootstrapArrWaitsForObservedResetBeforeRelease) {
    hold_bootstrap_ok = hold_peripheral_reset = true;
    auto result = enter();
    held(result);
    EXPECT_EQ(0U, waits);
    EXPECT_TRUE(model.lptim_writes.empty());
    hold_bootstrap_ok = hold_peripheral_reset = false;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 16U).remaining_effects);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Lptim1, PreEnableElapsedTimeCannotAuthorizeFrozenBootstrap) {
    freeze_after_enable = true;
    auto result = enter();
    ASSERT_TRUE(result.accepted);
    EXPECT_FALSE(result.transition.wait_executed);
    EXPECT_EQ(0U, result.transition.remaining_effects);
    EXPECT_EQ(0U, waits);
    EXPECT_TRUE(model.lptim_writes.empty());
    EXPECT_NE(NX_SUCCESS, request.base.result);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    for (const auto& write : writes)
        EXPECT_TRUE(
            write.first != A(LPTIM1->CCR1) && write.first != A(LPTIM1->RCR) &&
            write.first != A(LPTIM1->ARR) && write.first != A(LPTIM1->DIER));
    EXPECT_EQ(0U, enable_clears);
    EXPECT_NE(0U, peripheral_resets);
}
TEST_F(U575Stop2Lptim1, OwnedResetDeassertFailureRemainsRecoverable) {
    hold_peripheral_deassert = true;
    auto result = enter();
    held(result);
    EXPECT_NE(0U, model.reg(&RCC->APB3RSTR) & RCC_APB3RSTR_LPTIM1RST);
    EXPECT_EQ(0U, forbidden_accesses);
    hold_peripheral_deassert = false;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 16U).remaining_effects);
    EXPECT_EQ(0U, forbidden_accesses);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Lptim1, MissingPostDmaWriteOkRetainsEveryBorrowedSpan) {
    model.hold_write_ok = true;
    auto result = enter();
    held(result);
    EXPECT_EQ(3U, model.lptim_writes.size());
    EXPECT_EQ(0U, model.resets);
    EXPECT_EQ(0U, peripheral_resets);
    EXPECT_NE(0U, model.reg(&LPTIM1->DIER) & LPTIM_DIER_UEDE);
    /* Failure to prove kernel receipt latches IO, not a clock timeout.
     * A second public service may abort safely even if OK never appears,
     * while only the same concrete Stop2 owner may publish SETTLED. */
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_lptim1_service(&receiver, 16U));
    EXPECT_EQ(0U, model.reg(&LPTIM1->DIER) & LPTIM_DIER_UEDE);
    EXPECT_NE(0U, model.resets);
    EXPECT_NE(0U, peripheral_resets);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    model.hold_write_ok = false;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 16U).remaining_effects);
    EXPECT_EQ(1U, waits);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Lptim1, HeldProducerWithdrawalPreventsAnyDmaReset) {
    hold_ue_disable = true;
    auto result = enter();
    held(result);
    EXPECT_EQ(0U, model.resets);
    EXPECT_EQ(0U, peripheral_resets);
    hold_ue_disable = false;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 16U).remaining_effects);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Lptim1, IgnoredDmaResetRetainsLoanAndOriginalClaims) {
    model.hold_reset = true;
    auto result = enter();
    held(result);
    EXPECT_EQ(0U, peripheral_resets);
    EXPECT_EQ(auto_bits | RCC_SRDAMR_RTCAPBAMEN, model.reg(&RCC->SRDAMR));
    model.hold_reset = false;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 16U).remaining_effects);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Lptim1, UnknownWakeProfileTouchesNoPeripheralUntilRepair) {
    bad_wake_profile = true;
    auto result = enter();
    held(result);
    EXPECT_EQ(0U, forbidden_accesses);
    EXPECT_NE(0U, nx_stm32u575_stop2_recover(barrier, 16U).remaining_effects);
    EXPECT_EQ(0U, forbidden_accesses);
    model.reg(&RCC->CFGR2) = 0U;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 16U).remaining_effects);
    EXPECT_EQ(1U, waits);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Lptim1, EscapeBeforeAnUpdateSafelySettlesWithoutWriteOkFlags) {
    escape_only = true;
    auto result = enter();
    ASSERT_TRUE(result.accepted);
    EXPECT_TRUE(result.transition.wake_flag);
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.transition.primary);
    EXPECT_EQ(0U, result.transition.remaining_effects);
    EXPECT_TRUE(model.lptim_writes.empty());
    EXPECT_EQ(0U, request.base.transferred);
    settled(NX_ERROR_TIMEOUT);
}
TEST_F(U575Stop2Lptim1, PartialTupleRemainsBorrowedUntilSuspendedMasterJoins) {
    partial_tuple = escape_only = true;
    model.hold_suspend = true;
    auto result = enter();
    held(result);
    EXPECT_EQ(1U, model.lptim_writes.size());
    EXPECT_EQ(0U, model.resets);
    model.hold_suspend = false;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 16U).remaining_effects);
    EXPECT_EQ(1U, waits);
    settled(NX_ERROR_TIMEOUT);
}
TEST_F(U575Stop2Lptim1, PublicCancelNeverRevokesThePartialTupleLoan) {
    partial_tuple = escape_only = cancel_during_wait = true;
    model.hold_suspend = true;
    auto result = enter();
    held(result);
    EXPECT_EQ(1U, model.lptim_writes.size());
    EXPECT_EQ(0U, model.resets);
    model.hold_suspend = false;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 16U).remaining_effects);
    settled(NX_ERROR_CANCELLED);
}
TEST_F(U575Stop2Lptim1, LateDmaPendingCannotReleaseTheCompoundLoan) {
    model.hold_irq = true;
    auto result = enter();
    held(result);
    EXPECT_NE(0U, model.reg(&NVIC->ISPR[3]) & dma_irq);
    model.hold_irq = false;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 16U).remaining_effects);
    settled(NX_ERROR_IO);
}
TEST_F(U575Stop2Lptim1, InvalidTupleAndAliasedSpansPreserveEveryRequestByte) {
    auto* invalid =
        reinterpret_cast<nx_stm32u575_lptim1_tuple_t*>(SRAM4_BASE_NS + 0x700U);
    nx_stm32u575_retained_time_snapshot_t snapshot{};
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32u575_retained_time_sample(&clock_storage, &snapshot));
    for (const auto& value : {nx_stm32u575_lptim1_tuple_t{37U, 1U, 2047U},
                              nx_stm32u575_lptim1_tuple_t{37U, 0U, 1022U},
                              nx_stm32u575_lptim1_tuple_t{2048U, 0U, 2047U},
                              nx_stm32u575_lptim1_tuple_t{2047U, 0U, 2047U}}) {
        *invalid = value;
        nx_stm32u575_lpdma_lptim1_request_t candidate{};
        nx_request_initialize(&candidate.base);
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_lptim1_request_prepare(
                                  &candidate, &snapshot, invalid, workspace,
                                  NX_DEADLINE_NEVER));
        auto before = candidate;
        auto rejected = nx_stm32u575_stop2_lptim1_enter(barrier, &receiver,
                                                        &candidate, 4096U, 16U);
        EXPECT_FALSE(rejected.accepted);
        EXPECT_EQ(NX_ERROR_INVALID, rejected.transition.primary);
        EXPECT_EQ(0, std::memcmp(&before, &candidate, sizeof(candidate)));
    }
    *invalid = {37U, 0U, 2047U};
    auto before = request;
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_stm32u575_lpdma_lptim1_request_prepare(
            &request, &snapshot, invalid,
            reinterpret_cast<nx_stm32u575_lpdma_lptim1_workspace_t*>(invalid),
            NX_DEADLINE_NEVER));
    EXPECT_EQ(0, std::memcmp(&before, &request, sizeof(request)));
    EXPECT_TRUE(writes.empty());
}
TEST_F(U575Stop2Lptim1, StopBusyKeepsPhysicalClaimThroughTheCompoundJoin) {
    partial_tuple = escape_only = true;
    model.hold_suspend = true;
    auto result = enter();
    held(result);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_lptim1_stop(&receiver, 16U));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    model.hold_suspend = false;
    EXPECT_EQ(0U, nx_stm32u575_stop2_recover(barrier, 16U).remaining_effects);
    settled(NX_ERROR_TIMEOUT);
    EXPECT_NE(0U, model.reg(&RCC->APB3ENR) & RCC_APB3ENR_LPTIM1EN);
    auto ordinary = nx_stm32u575_stop2_enter(barrier, 2U, 16U);
    EXPECT_EQ(NX_ERROR_BUSY, ordinary.primary);
    EXPECT_FALSE(ordinary.wait_executed);
    nx_arch_irq_restore(mask);
    nx_stm32u575_lpdma_lptim1_storage_t other_storage{};
    nx_stm32u575_lpdma_lptim1_t other{};
    nx_stm32u575_lpdma_lptim1_config_t binding{&clock_storage,
                                               nx_stm32u575_stop2_identity()};
    auto before = writes.size();
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_lptim1_initialize(
                                 &other_storage, &binding, &other));
    EXPECT_EQ(nullptr, other.ops);
    EXPECT_EQ(nullptr, other.context);
    EXPECT_EQ(before, writes.size());
    mask = nx_arch_irq_save();
    /* TearDown's joined public stop must then withdraw the retained public
     * gate/source claim; subsequent cases construct a fresh owner. */
}
TEST_F(U575Stop2Lptim1, ObservedExternalResetPermanentlyQuarantinesLoan) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    for (bool producer_reset : {false, true})
        EXPECT_EXIT(
            {
                bad_dma_reset = !producer_reset;
                bad_lptim_reset = producer_reset;
                auto result = enter();
                bool retained = result.accepted &&
                                result.transition.remaining_effects != 0U &&
                                forbidden_accesses == 0U;
                model.reg(&RCC->AHB3RSTR) = 0U;
                model.reg(&RCC->APB3RSTR) = 0U;
                model.reg(&LPDMA1_Channel0->CCR) = 0U;
                model.reg(&LPDMA1_Channel0->CSR) = DMA_CSR_IDLEF;
                model.reg(&LPDMA1_Channel0->CLLR) = 0U;
                model.reg(&LPTIM1->CR) = 0U;
                model.reg(&LPTIM1->DIER) = 0U;
                retained =
                    retained &&
                    nx_stm32u575_stop2_recover(barrier, 16U)
                            .remaining_effects != 0U &&
                    nx_request_state(&request.base) != NX_REQUEST_SETTLED &&
                    waits == 1U;
                std::_Exit(retained ? 0 : 1);
            },
            testing::ExitedWithCode(0), "");
}
} /* namespace */
extern "C" uint32_t nx_u575_stop2_control(void) {
    return 0U;
}
extern "C" uint32_t nx_u575_lpdma_copy_read(uintptr_t address) {
    return port_read(address);
}
extern "C" void nx_u575_lpdma_copy_write(uintptr_t address, uint32_t value) {
    port_write(address, value);
}
extern "C" uint32_t nx_u575_stop2_read(uintptr_t address) {
    return port_read(address);
}
extern "C" void nx_u575_stop2_write(uintptr_t address, uint32_t value) {
    port_write(address, value);
}
extern "C" void nx_u575_lptim1_dma_write(uintptr_t address, uint32_t value) {
    (void)address;
    (void)value;
    EXPECT_NE(0U, model.reg(&LPTIM1->CR) & LPTIM_CR_ENABLE);
    EXPECT_NE(0U, model.reg(&LPTIM1->DIER) & LPTIM_DIER_UEDE);
    EXPECT_EQ(LPTIM_CFGR_PRELOAD,
              model.reg(&LPTIM1->CFGR) & LPTIM_CFGR_PRELOAD);
}
extern "C" void nx_u575_stop2_wait(void) {
    ++waits;
    EXPECT_EQ(1U, nx_arch_irq_masks().primask);
    EXPECT_EQ(sleep_bits, model.reg(&RCC->AHB3SMENR) & sleep_bits);
    EXPECT_EQ(auto_bits | RCC_SRDAMR_RTCAPBAMEN, model.reg(&RCC->SRDAMR));
    EXPECT_EQ(RCC_APB3SMENR_RTCAPBSMEN | RCC_APB3SMENR_LPTIM1SMEN,
              model.reg(&RCC->APB3SMENR));
    EXPECT_NE(0U, model.reg(&SCB->SCR) & SCB_SCR_SLEEPDEEP_Msk);
    EXPECT_EQ(0U, model.reg(&RTC->CR) & RTC_CR_ALRAE);
    EXPECT_EQ(1U, timer_starts);
    EXPECT_EQ(0U, model.reg(&LPTIM1->ISR) & write_ok);
    if (partial_tuple) {
        model.reg(&LPTIM1->ISR) |= LPTIM_ISR_UE;
        model.finish_block();
    } else if (!escape_only && !pending_hint_only)
        model.lptim_event();
    if (pending_hint_only)
        model.reg(&NVIC->ISPR[3]) |= dma_irq;
    if (cancel_during_wait) {
        EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_lptim1_cancel(active_receiver,
                                                               active_request));
    }
    if (escape_only) {
        model.reg(&RTC->SR) |= RTC_SR_WUTF;
        model.reg(&NVIC->ISPR[0]) |= rtc_irq;
    }
    model.reg(&RCC->CR) |= RCC_CR_MSIKON | RCC_CR_MSIKRDY;
    model.reg(&RTC->TR) = escape_only ? 2U : 1U;
    if (bad_wake_profile)
        model.reg(&RCC->CFGR2) = RCC_CFGR2_HPRE;
    if (bad_dma_reset)
        model.reg(&RCC->AHB3RSTR) = RCC_AHB3RSTR_LPDMA1RST;
    if (bad_lptim_reset)
        model.reg(&RCC->APB3RSTR) = RCC_APB3RSTR_LPTIM1RST;
}
