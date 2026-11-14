/**
 * \file            stm32_sdio_system_test.cpp
 * \brief           Closed SDIO clock admission preserves foreign USB and RNG
 * owners \author          Nexus Team \version         1.0.0 \date 2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/components/clock_domain_owner.h"
#include "nexus/core/fault.h"
#include "nexus/soc/stm32f4_power.h"
#include "stm32f407_clock.h"
#include "stm32f407_system.h"
nx_stm32_system_t g_nx_stm32_system{};
uint32_t nx_boot_cpacr;
uint32_t nx_boot_mpu;
uint32_t nx_boot_vtor;
uint32_t nx_system_rng_control;
uint32_t nx_power_scr, nx_power_icsr, nx_power_tick, nx_power_emr;
uint32_t nx_power_rtsr, nx_power_imr, nx_power_ftsr;
uint32_t nx_power_dma1[64], nx_power_dma2[64];
uint32_t nx_power_enabled[3], nx_power_active[3];
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdexcept>

namespace {
uint32_t mask;
bool irq_enabled;
unsigned deep_waits;
testing::MockFunction<void(volatile uint32_t*, uint32_t)>* hardware;

class SdioSystem : public testing::Test {
  protected:
    RCC_TypeDef rcc{};
    FLASH_TypeDef flash{};
    PWR_TypeDef power{};
    TIM_TypeDef timer{};
    testing::NiceMock<testing::MockFunction<void(volatile uint32_t*, uint32_t)>>
        writes;
    void SetUp() override {
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &rcc;
        g_nx_stm32_system.flash = &flash;
        g_nx_stm32_system.power = &power;
        g_nx_stm32_system.timer = &timer;
        rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        rcc.PLLCFGR = 0x24003010U;
        nx_boot_cpacr = 0x00f00000U;
        nx_boot_mpu = 0U;
        nx_boot_vtor = 0x08000000U;
        nx_system_rng_control = 0U;
        mask = 0U;
        irq_enabled = false;
        deep_waits = 0U;
        nx_power_scr = nx_power_icsr = nx_power_tick = nx_power_emr = 0U;
        nx_power_rtsr = nx_power_imr = nx_power_ftsr = 0U;
        std::memset(nx_power_dma1, 0, sizeof(nx_power_dma1));
        std::memset(nx_power_dma2, 0, sizeof(nx_power_dma2));
        std::memset(nx_power_enabled, 0, sizeof(nx_power_enabled));
        std::memset(nx_power_active, 0, sizeof(nx_power_active));
        hardware = &writes;
        ON_CALL(writes, Call(testing::_, testing::_))
            .WillByDefault([this](volatile uint32_t* reg, uint32_t value) {
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
        testing::Mock::VerifyAndClearExpectations(&writes);
        rcc.AHB1ENR = rcc.AHB2ENR = 0U;
        EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
        hardware = nullptr;
    }
    void Foreign(uint32_t ahb1, uint32_t ahb2) {
        rcc.AHB1ENR = ahb1;
        rcc.AHB2ENR = ahb2;
#if NX_TEST_SDIO_HS375
        const auto original = rcc.CR;
        EXPECT_CALL(writes, Call(testing::_, testing::_)).Times(0);
        EXPECT_EQ(nx_stm32_system_preflight(&g_nx_stm32_system), NX_ERROR_BUSY);
        const auto result = nx_stm32_system_start(&g_nx_stm32_system, 8U);
        EXPECT_NE(result.primary, 0);
        EXPECT_EQ(result.cleanup, 0);
        EXPECT_EQ(result.remaining_effects, 0U);
        EXPECT_FALSE(g_nx_stm32_system.started);
        EXPECT_FALSE(irq_enabled);
        EXPECT_EQ(rcc.CR, original);
        EXPECT_EQ(power.CR, 0U);
        EXPECT_EQ(flash.ACR, 0U);
#else
        EXPECT_EQ(nx_stm32_system_preflight(&g_nx_stm32_system), NX_SUCCESS);
        EXPECT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
#endif
        EXPECT_EQ(rcc.AHB1ENR, ahb1);
        EXPECT_EQ(rcc.AHB2ENR, ahb2);
    }
};
} /* namespace */

TEST_F(SdioSystem, UncontendedPlanPublishesItsActualClockAndWithdrawsCleanly) {
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    EXPECT_EQ(g_nx_stm32_system.core_hz, NEXUS_CORE_HZ);
    EXPECT_EQ(timer.PSC, NEXUS_APB1_TIMER_HZ / 1000000U - 1U);
    EXPECT_EQ(flash.ACR & FLASH_ACR_LATENCY, NEXUS_FLASH_WAIT_STATES);
    EXPECT_TRUE(nx_stm32_clock_profile_matches(&rcc, &flash));
    EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_EQ(SystemCoreClock, 16000000U);
    EXPECT_EQ(g_nx_stm32_system.remaining_effects, 0U);
}
TEST_F(SdioSystem, ForeignFullSpeedUsbGateIsNeverRetargeted) {
    Foreign(0U, RCC_AHB2ENR_OTGFSEN);
}
TEST_F(SdioSystem, ForeignHighSpeedUsbGateIsNeverRetargeted) {
    Foreign(RCC_AHB1ENR_OTGHSEN, 0U);
}
TEST_F(SdioSystem, ForeignUlpiGateAloneIsNeverRetargeted) {
    Foreign(RCC_AHB1ENR_OTGHSULPIEN, 0U);
}
TEST_F(SdioSystem, ForeignRngGateWithoutRnGenStillKeepsItsDomain) {
    Foreign(0U, RCC_AHB2ENR_RNGEN);
}

#if NX_TEST_SDIO_HS375
TEST_F(SdioSystem, HandLinkedPowerEntryCannotBroadenTheClosedRunProfile) {
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    nx_clock_domain_t domain{};
    ASSERT_EQ(nx_clock_domain_construct(&domain, 1U), NX_SUCCESS);
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(nx_clock_domain_close(&domain, &barrier), NX_SUCCESS);
    const auto epoch = g_nx_stm32_system.time_epoch;
    const auto power_control = power.CR;
    nx_stm32_stop_receipt_t receipt{};
    nx_stm32_power_options_t options = {
        {1U << 6U, 0U, 0U, 0U}, 8U, 0U, 0U, false, false, false};
    nx_power_enabled[0] = 1U << 6U;
    nx_power_imr = nx_power_rtsr = 1U;
    EXPECT_CALL(writes, Call(testing::_, testing::_)).Times(0);
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt),
              NX_ERROR_UNSUPPORTED);
    options.standby = true;
    options.wake_irqs[0] = 0U;
    options.wake_pins = NX_STM32_WAKE_PIN_PA0;
    nx_power_enabled[0] = 0U;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(g_nx_stm32_system.time_epoch, epoch);
    EXPECT_TRUE(g_nx_stm32_system.started);
    EXPECT_EQ(power.CR, power_control);
    EXPECT_EQ(nx_power_scr, 0U);
    EXPECT_EQ(mask, 0U);
    EXPECT_EQ(deep_waits, 0U);
    EXPECT_EQ(receipt.old_epoch, 0U);
    EXPECT_EQ(receipt.new_epoch, 0U);
    EXPECT_FALSE(receipt.waited);
}
#endif

/** \brief Model ready/SWS observations without implementing clock behavior. */
extern "C" void nx_stm32_system_model_poll(void* opaque) {
    auto* state = static_cast<nx_stm32_system_t*>(opaque);
    auto* rcc = state->rcc;
    if ((rcc->CR & RCC_CR_HSEON) != 0U)
        rcc->CR |= RCC_CR_HSERDY;
    else
        rcc->CR &= ~static_cast<uint32_t>(RCC_CR_HSERDY);
    if ((rcc->CR & RCC_CR_PLLON) != 0U)
        rcc->CR |= RCC_CR_PLLRDY;
    else
        rcc->CR &= ~static_cast<uint32_t>(RCC_CR_PLLRDY);
    rcc->CFGR = (rcc->CFGR & ~static_cast<uint32_t>(RCC_CFGR_SWS)) |
                ((rcc->CFGR & static_cast<uint32_t>(RCC_CFGR_SW)) << 2U);
}
extern "C" void nx_stm32_system_model_write(volatile uint32_t* reg,
                                            uint32_t value) {
    hardware->Call(reg, value);
}
extern "C" bool nx_stm32_system_model_irq_available(int) {
    return !irq_enabled;
}
extern "C" int nx_stm32_system_model_irq_control(int, bool enabled) {
    irq_enabled = enabled;
    return 0;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t saved = {mask};
    mask = 1U;
    return saved;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    mask = saved.value;
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return {mask, 0U, 0U};
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    throw std::runtime_error("unexpected SDIO clock terminal fault");
}

extern "C" uint32_t nx_arch_exception_number(void) {
    return 0U;
}
extern "C" void nx_stm32_power_model_wait(void) {
    ++deep_waits;
}
extern "C" void nx_stm32_power_model_event_wait(void) {
    ++deep_waits;
}
