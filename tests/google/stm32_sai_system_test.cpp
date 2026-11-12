/**
 * \file            stm32_sai_system_test.cpp
 * \brief           Actual System auxiliary clock acquisition and withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/core/fault.h"
#include "stm32f407_clock.h"
#include "stm32f407_system.h"
nx_stm32_system_t g_nx_stm32_system{};
uint32_t nx_family_boot_cpacr;
uint32_t nx_family_boot_mpu;
uint32_t nx_family_boot_vtor;
uint32_t nx_family_rng_control;
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdexcept>

namespace {
uint32_t masks;
bool irq_enabled;
bool refuse_enable, refuse_ready, refuse_disable, hold_ready;
bool refuse_pll, refuse_mux, refuse_safe_p;
unsigned stores, unsafe_input_writes;
testing::MockFunction<void(volatile uint32_t*, uint32_t)>* hardware;
uint32_t pll_mask() {
    uint32_t result = RCC_PLLSAICFGR_PLLSAIN | RCC_PLLSAICFGR_PLLSAIP;
#if NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
    result |= RCC_PLLSAICFGR_PLLSAIQ;
#endif
#if NEXUS_CLOCK_PLLSAI_LTDC_ENABLED
    result |= RCC_PLLSAICFGR_PLLSAIR;
#endif
#if defined(STM32F446xx)
    result |= RCC_PLLSAICFGR_PLLSAIM;
#endif
    return result;
}
uint32_t expected_pll() {
    uint32_t result =
        (192U << RCC_PLLSAICFGR_PLLSAIN_Pos) | RCC_PLLSAICFGR_PLLSAIP_0;
#if NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
    result |= 5U << RCC_PLLSAICFGR_PLLSAIQ_Pos;
#endif
#if NEXUS_CLOCK_PLLSAI_LTDC_ENABLED
    result |= 4U << RCC_PLLSAICFGR_PLLSAIR_Pos;
#endif
#if defined(STM32F446xx)
    result |= 10U;
#endif
    return result;
}
uint32_t mux_mask() {
    uint32_t result = 0U;
#if NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
    result |= RCC_DCKCFGR_PLLSAIDIVQ;
#if defined(STM32F446xx)
    result |= RCC_DCKCFGR_SAI1SRC;
#else
    result |= RCC_DCKCFGR_SAI1ASRC | RCC_DCKCFGR_SAI1BSRC;
#endif
#endif
#if NEXUS_CLOCK_PLLSAI_LTDC_ENABLED
    result |= RCC_DCKCFGR_PLLSAIDIVR;
#endif
    return result;
}
uint32_t expected_mux() {
    uint32_t result = 0U;
#if NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
    result |= 4U << 8U;
#endif
#if NEXUS_CLOCK_PLLSAI_LTDC_ENABLED
    result |= 3U << 16U;
#endif
    return result;
}
class SaiSystem : public testing::Test {
  protected:
    RCC_TypeDef rcc{};
    FLASH_TypeDef flash{};
    PWR_TypeDef power{};
    TIM_TypeDef timer{};
    testing::NiceMock<testing::MockFunction<void(volatile uint32_t*, uint32_t)>>
        writes;
    volatile uint32_t* Clock48() {
#if defined(STM32F446xx)
        return &rcc.DCKCFGR2;
#else
        return &rcc.DCKCFGR;
#endif
    }
    void SetUp() override {
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &rcc;
        g_nx_stm32_system.flash = &flash;
        g_nx_stm32_system.power = &power;
        g_nx_stm32_system.timer = &timer;
        rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        rcc.PLLCFGR = 0x24003010U;
        rcc.PLLSAICFGR = 0x44005211U;
        rcc.DCKCFGR = 0x00f3000bU;
        power.CR = PWR_CR_DBP;
        nx_family_boot_cpacr = 0x00f00000U;
        nx_family_boot_mpu = 0U;
        nx_family_boot_vtor = 0x08000000U;
        nx_family_rng_control = 0U;
        masks = stores = unsafe_input_writes = 0U;
        irq_enabled = refuse_enable = refuse_ready = refuse_disable = false;
        hold_ready = refuse_pll = refuse_mux = refuse_safe_p = false;
        hardware = &writes;
        ON_CALL(writes, Call(testing::_, testing::_))
            .WillByDefault([this](volatile uint32_t* reg, uint32_t value) {
                if (reg == &rcc.CR) {
                    if (refuse_enable && (value & RCC_CR_PLLSAION) != 0U)
                        value &= ~static_cast<uint32_t>(RCC_CR_PLLSAION);
                    if (refuse_disable && (rcc.CR & RCC_CR_PLLSAION) != 0U &&
                        (value & RCC_CR_PLLSAION) == 0U)
                        value |= RCC_CR_PLLSAION;
                    /* RDY is read-only; the poll leaf alone changes it. */
                    value = (value & ~static_cast<uint32_t>(RCC_CR_PLLSAIRDY)) |
                            (rcc.CR & RCC_CR_PLLSAIRDY);
                }
                if (reg == &rcc.PLLCFGR &&
                    ((rcc.PLLCFGR ^ value) &
                     (RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLSRC)) != 0U &&
                    (rcc.CR & (RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY)) != 0U)
                    ++unsafe_input_writes;
                if (reg == &rcc.PLLSAICFGR && refuse_safe_p)
                    value = (value &
                             ~static_cast<uint32_t>(RCC_PLLSAICFGR_PLLSAIP)) |
                            (rcc.PLLSAICFGR & RCC_PLLSAICFGR_PLLSAIP);
                if ((reg == &rcc.PLLSAICFGR && refuse_pll) ||
                    (reg == &rcc.DCKCFGR && refuse_mux))
                    return;
                if (reg == &timer.SR)
                    *reg &= value;
                else
                    *reg = value;
                if (reg == &timer.EGR && (value & TIM_EGR_UG) != 0U) {
                    timer.SR |= TIM_SR_UIF;
                    timer.CNT = 0U;
                }
            });
    }
    void TearDown() override {
        refuse_enable = refuse_ready = refuse_disable = hold_ready = false;
        refuse_pll = refuse_mux = refuse_safe_p = false;
        *Clock48() &= ~UINT32_C(0x08000000);
        rcc.APB2ENR = 0U;
        rcc.CR &= ~static_cast<uint32_t>(RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY);
        EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
        hardware = nullptr;
    }
};
} /* namespace */

TEST_F(SaiSystem, SystemOwnsOneExactAuxiliaryPllWithTheMainPllOff) {
    const uint32_t incoming_main = rcc.PLLCFGR;
    const uint32_t incoming_aux = rcc.PLLSAICFGR;
    const uint32_t incoming_mux = rcc.DCKCFGR;
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    EXPECT_EQ(rcc.CR & (RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY),
              static_cast<uint32_t>(RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY));
    EXPECT_EQ(rcc.CR & (RCC_CR_PLLON | RCC_CR_PLLRDY), 0U);
    EXPECT_EQ(rcc.PLLSAICFGR & pll_mask(), expected_pll());
    EXPECT_EQ(rcc.PLLSAICFGR & ~pll_mask(), incoming_aux & ~pll_mask());
    EXPECT_EQ(rcc.DCKCFGR & mux_mask(), expected_mux());
    EXPECT_EQ(rcc.DCKCFGR & ~mux_mask(), incoming_mux & ~mux_mask());
    EXPECT_EQ(rcc.PLLCFGR & RCC_PLLCFGR_PLLM, NEXUS_CLOCK_PLLM);
    EXPECT_TRUE(g_nx_stm32_system.started);
    EXPECT_TRUE(nx_stm32_clock_profile_matches(&rcc, &flash));
    ASSERT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_EQ(unsafe_input_writes, 0U);
    EXPECT_EQ(rcc.PLLCFGR, incoming_main);
    EXPECT_EQ(rcc.PLLSAICFGR, incoming_aux);
    EXPECT_EQ(rcc.DCKCFGR, incoming_mux);
    EXPECT_EQ(rcc.CR & (RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY), 0U);
    EXPECT_EQ(g_nx_stm32_system.remaining_effects, 0U);
}
TEST_F(SaiSystem, ClockReadbackIncludesTheAuxiliaryTupleMuxAndReadyState) {
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    ASSERT_TRUE(nx_stm32_clock_profile_matches(&rcc, &flash));
    rcc.PLLSAICFGR ^= RCC_PLLSAICFGR_PLLSAIN_0;
    EXPECT_FALSE(nx_stm32_clock_profile_matches(&rcc, &flash));
    rcc.PLLSAICFGR ^= RCC_PLLSAICFGR_PLLSAIN_0;
    rcc.PLLSAICFGR ^= RCC_PLLSAICFGR_PLLSAIP_0;
    EXPECT_FALSE(nx_stm32_clock_profile_matches(&rcc, &flash));
    rcc.PLLSAICFGR ^= RCC_PLLSAICFGR_PLLSAIP_0;
    const uint32_t mux_changes[] = {
#if NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
        RCC_DCKCFGR_PLLSAIDIVQ_0,
#endif
#if NEXUS_CLOCK_PLLSAI_LTDC_ENABLED
        RCC_DCKCFGR_PLLSAIDIVR_0,
#endif
    };
    for (uint32_t change : mux_changes) {
        SCOPED_TRACE(change);
        rcc.DCKCFGR ^= change;
        EXPECT_FALSE(nx_stm32_clock_profile_matches(&rcc, &flash));
        rcc.DCKCFGR ^= change;
        EXPECT_TRUE(nx_stm32_clock_profile_matches(&rcc, &flash));
    }
    rcc.CR &= ~static_cast<uint32_t>(RCC_CR_PLLSAIRDY);
    EXPECT_FALSE(nx_stm32_clock_profile_matches(&rcc, &flash));
}
TEST_F(SaiSystem, RefusedSafePDividerCannotEnableAnOverclockedUnusedOutput) {
    refuse_safe_p = true;
    const auto result = nx_stm32_system_start(&g_nx_stm32_system, 8U);
    EXPECT_NE(result.primary, 0);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(0U, rcc.CR & (RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY));
    EXPECT_EQ(0, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
}
TEST_F(SaiSystem, ExistingPClockSelectionRejectsBeforeAnySystemEffect) {
    *Clock48() |= UINT32_C(0x08000000);
    EXPECT_NE(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    EXPECT_EQ(0U, stores);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_NE(0U, *Clock48() & UINT32_C(0x08000000));
}
TEST_F(SaiSystem, LaterPClockBorrowerKeepsTheClockAndLiveEpochWithoutWrites) {
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    *Clock48() |= UINT32_C(0x08000000);
    const unsigned before = stores;
    EXPECT_NE(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_TRUE(g_nx_stm32_system.started);
    EXPECT_EQ(before, stores);
    EXPECT_NE(0U, *Clock48() & UINT32_C(0x08000000));
}
#if defined(STM32F479xx) && NX_TEST_AUX_KIND != 3U
TEST_F(SaiSystem,
       InvalidUnselectedOutputEncodingIsNotAdoptedOrSilentlyRepaired) {
#if NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
    rcc.PLLSAICFGR &= ~static_cast<uint32_t>(RCC_PLLSAICFGR_PLLSAIR);
#else
    rcc.PLLSAICFGR &= ~static_cast<uint32_t>(RCC_PLLSAICFGR_PLLSAIQ);
#endif
    const auto actual = rcc.PLLSAICFGR;
    EXPECT_NE(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    EXPECT_EQ(0U, stores);
    EXPECT_EQ(actual, rcc.PLLSAICFGR);
}
#endif
#ifndef NX_TEST_R_OUTPUT
TEST_F(SaiSystem, AuxiliaryReadyRefusalCannotStartTheTimebase) {
    refuse_ready = true;
    const auto result = nx_stm32_system_start(&g_nx_stm32_system, 8U);
    EXPECT_NE(result.primary, 0);
    EXPECT_EQ(result.cleanup, 0);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(result.remaining_effects, 0U);
}
TEST_F(SaiSystem, AuxiliaryEnableRefusalCannotPublishTheClockPlan) {
    refuse_enable = true;
    const auto result = nx_stm32_system_start(&g_nx_stm32_system, 8U);
    EXPECT_NE(result.primary, 0);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(result.remaining_effects, 0U);
}
TEST_F(SaiSystem,
       AuxiliaryConfigurationRefusalRollsBackBeforeTimerPublication) {
    refuse_pll = true;
    const auto result = nx_stm32_system_start(&g_nx_stm32_system, 8U);
    EXPECT_NE(result.primary, 0);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(result.cleanup, 0);
}
TEST_F(SaiSystem, AuxiliaryMuxRefusalRollsBackBeforeTimerPublication) {
    refuse_mux = true;
    const auto result = nx_stm32_system_start(&g_nx_stm32_system, 8U);
    EXPECT_NE(result.primary, 0);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(result.cleanup, 0);
}
#endif
TEST_F(SaiSystem, RefusedAuxiliaryDisableRetainsTheSharedInputAndReceipt) {
    const auto incoming_main = rcc.PLLCFGR;
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    /* Independent external observation keeps the old implementation's lack
     * of acquisition from hiding its unsafe cleanup path. */
    rcc.CR |= RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY;
    refuse_disable = true;
    EXPECT_NE(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_NE(g_nx_stm32_system.remaining_effects, 0U);
    EXPECT_EQ(rcc.PLLCFGR & RCC_PLLCFGR_PLLM, NEXUS_CLOCK_PLLM);
    EXPECT_EQ(unsafe_input_writes, 0U);
    refuse_disable = false;
    EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_EQ(rcc.PLLCFGR, incoming_main);
}
#ifndef NX_TEST_R_OUTPUT
TEST_F(SaiSystem, RefusedReadyClearRetainsTheAuxiliaryFieldsUntilRecovery) {
    const auto original = rcc.PLLSAICFGR;
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    rcc.CR |= RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY;
    const auto prepared = rcc.PLLSAICFGR;
    hold_ready = true;
    EXPECT_NE(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_EQ(rcc.PLLSAICFGR, prepared);
    EXPECT_NE(g_nx_stm32_system.remaining_effects, 0U);
    EXPECT_EQ(unsafe_input_writes, 0U);
    hold_ready = false;
    EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_EQ(rcc.PLLSAICFGR, original);
}
#endif
TEST_F(SaiSystem, PreparedOutputOwnerRetainsClockAndTimeEpochWithoutAnyWrites) {
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    const uint32_t gates[] = {
#if NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
        RCC_APB2ENR_SAI1EN,
#endif
#if NEXUS_CLOCK_PLLSAI_LTDC_ENABLED
        RCC_APB2ENR_LTDCEN,
#endif
    };
    const uint64_t epoch = g_nx_stm32_system.time_epoch;
    for (uint32_t gate : gates) {
        SCOPED_TRACE(gate);
        rcc.APB2ENR |= gate;
        const unsigned before = stores;
        EXPECT_NE(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
        EXPECT_TRUE(g_nx_stm32_system.started);
        EXPECT_TRUE(irq_enabled);
        EXPECT_EQ(epoch, g_nx_stm32_system.time_epoch);
        EXPECT_EQ(stores, before);
        rcc.APB2ENR &= ~gate;
    }
}
#ifndef NX_TEST_R_OUTPUT
TEST_F(SaiSystem, AnotherAuxiliaryPllRetainsTheCommonInputBeforeAnyWithdrawal) {
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    rcc.CR |= RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY;
    const unsigned before = stores;
    EXPECT_NE(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_TRUE(g_nx_stm32_system.started);
    EXPECT_EQ(stores, before);
}
TEST_F(SaiSystem,
       RestoreMuxRefusalRetainsCleanupWithoutRestoringTheInputEarly) {
    const auto incoming = rcc.DCKCFGR;
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    refuse_mux = true;
    EXPECT_NE(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_NE(g_nx_stm32_system.remaining_effects, 0U);
    EXPECT_EQ(rcc.PLLCFGR & RCC_PLLCFGR_PLLM, NEXUS_CLOCK_PLLM);
    EXPECT_EQ(unsafe_input_writes, 0U);
    refuse_mux = false;
    EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_EQ(rcc.DCKCFGR, incoming);
}
TEST_F(SaiSystem, ForeignSaiGateRejectsAdmissionBeforeAnyAuxiliaryEffect) {
    rcc.APB2ENR |= RCC_APB2ENR_SAI1EN;
    EXPECT_NE(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    EXPECT_EQ(stores, 0U);
    EXPECT_EQ(g_nx_stm32_system.remaining_effects, 0U);
    rcc.APB2ENR = 0U;
}
#endif
extern "C" void nx_family_poll(void* opaque) {
    auto* s = static_cast<nx_stm32_system_t*>(opaque);
    auto* r = s->rcc;
    if ((r->CR & RCC_CR_PLLSAION) != 0U) {
        if (!refuse_ready)
            r->CR |= RCC_CR_PLLSAIRDY;
    } else if (!hold_ready) {
        r->CR &= ~static_cast<uint32_t>(RCC_CR_PLLSAIRDY);
    }
    if ((r->CR & RCC_CR_PLLON) != 0U)
        r->CR |= RCC_CR_PLLRDY;
    else
        r->CR &= ~static_cast<uint32_t>(RCC_CR_PLLRDY);
    if ((r->CR & RCC_CR_HSEON) == 0U)
        r->CR &= ~static_cast<uint32_t>(RCC_CR_HSERDY);
    r->CFGR = (r->CFGR & ~static_cast<uint32_t>(RCC_CFGR_SWS)) |
              ((r->CFGR & static_cast<uint32_t>(RCC_CFGR_SW)) << 2U);
}
extern "C" void nx_family_write(volatile uint32_t* reg, uint32_t value) {
    ++stores;
    hardware->Call(reg, value);
}
extern "C" bool nx_family_irq_available(int) {
    return !irq_enabled;
}
extern "C" int nx_family_irq_control(int, bool enabled) {
    irq_enabled = enabled;
    return 0;
}
extern "C" uint32_t nx_family_idcode(void) {
#if defined(STM32F446xx)
    return 0x10000421U;
#else
    return 0x10000434U;
#endif
}
extern "C" uint16_t nx_family_flash_kib(void) {
#if defined(STM32F446xx)
    return 512U;
#else
    return 2048U;
#endif
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t saved = {masks};
    masks = 1U;
    return saved;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    masks = saved.value;
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return {masks, 0U, 0U};
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return masks != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    throw std::runtime_error("unexpected System terminal fault");
}
