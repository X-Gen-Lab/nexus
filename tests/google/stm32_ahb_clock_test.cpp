/**
 * \file            stm32_ahb_clock_test.cpp
 * \brief           Observe actual HPRE across failed startup and withdrawal
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
uint32_t nx_boot_cpacr;
uint32_t nx_boot_mpu;
uint32_t nx_boot_vtor;
uint32_t nx_system_rng_control;
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdexcept>

namespace {
testing::MockFunction<void(volatile uint32_t*, uint32_t)>* hardware;
uint32_t mask;
bool hold_hse_start;
bool hold_hse_stop;
bool refuse_prescalers;
bool irq_enabled;

class AhbClock : public testing::Test {
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
        mask = 0U;
        nx_boot_cpacr = 0x00f00000U;
        nx_boot_mpu = 0U;
        nx_boot_vtor = 0x08000000U;
        nx_system_rng_control = 0U;
        hold_hse_start = hold_hse_stop = refuse_prescalers = false;
        irq_enabled = false;
        hardware = &writes;
        ON_CALL(writes, Call(testing::_, testing::_))
            .WillByDefault([this](volatile uint32_t* reg, uint32_t value) {
                if (refuse_prescalers && reg == &rcc.CFGR &&
                    value == NEXUS_CLOCK_CFGR)
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
        hold_hse_start = hold_hse_stop = refuse_prescalers = false;
        mask = 0U;
        EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
        hardware = nullptr;
    }
    nx_stm32_start_result_t Start() {
        return nx_stm32_system_start(&g_nx_stm32_system, 8U);
    }
};
} /* namespace */

TEST_F(AhbClock, SelectedDividerStartsAndWithdrawsTheActualClock) {
    ASSERT_EQ(0, Start().primary);
    EXPECT_EQ(NEXUS_CORE_HZ, g_nx_stm32_system.core_hz);
    EXPECT_EQ(NEXUS_CLOCK_CFGR & RCC_CFGR_HPRE, rcc.CFGR & RCC_CFGR_HPRE);
    EXPECT_EQ(NEXUS_APB1_TIMER_HZ / 1000000U - 1U, timer.PSC);
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(16000000U, g_nx_stm32_system.core_hz);
    EXPECT_EQ(16000000U, SystemCoreClock);
}

TEST_F(AhbClock, EarlySourceFailureDoesNotClaimTheUnestablishedDivider) {
    hold_hse_start = true;
    const auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_EQ(0U, rcc.CFGR & RCC_CFGR_HPRE);
    EXPECT_EQ(16000000U, g_nx_stm32_system.core_hz);
    EXPECT_EQ(16000000U, SystemCoreClock);
}

TEST_F(AhbClock, FailedPrescalerAndSourceWithdrawalPublishObservedHpre) {
    refuse_prescalers = hold_hse_stop = true;
    const auto result = Start();
    ASSERT_NE(0, result.primary);
    ASSERT_NE(0, result.cleanup);
    ASSERT_EQ(0U, rcc.CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE));
    EXPECT_EQ(16000000U, g_nx_stm32_system.core_hz);
    EXPECT_EQ(16000000U, SystemCoreClock);
    EXPECT_NE(0U, result.remaining_effects);
    hold_hse_stop = refuse_prescalers = false;
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(16000000U, SystemCoreClock);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
}

TEST_F(AhbClock, FailedSourceWithdrawalRetainsItsEstablishedDividerUntilRetry) {
    ASSERT_EQ(0, Start().primary);
    hold_hse_stop = true;
    ASSERT_NE(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(16000000U / NEXUS_STM32_AHB_DIV, SystemCoreClock);
    EXPECT_EQ(SystemCoreClock, g_nx_stm32_system.core_hz);
    EXPECT_EQ(NEXUS_CLOCK_CFGR & RCC_CFGR_HPRE, rcc.CFGR & RCC_CFGR_HPRE);
    hold_hse_stop = false;
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(0U, rcc.CFGR & RCC_CFGR_HPRE);
    EXPECT_EQ(16000000U, SystemCoreClock);
}

/** \brief Model RCC ready/status bits independently of programmed requests. */
extern "C" void nx_stm32_system_model_poll(void* opaque) {
    auto* state = static_cast<nx_stm32_system_t*>(opaque);
    auto* rcc = state->rcc;
    if ((rcc->CR & RCC_CR_HSEON) != 0U) {
        if (!hold_hse_start)
            rcc->CR |= RCC_CR_HSERDY;
    } else if (!hold_hse_stop) {
        rcc->CR &= ~(uint32_t)RCC_CR_HSERDY;
    }
    if ((rcc->CR & RCC_CR_PLLON) != 0U)
        rcc->CR |= RCC_CR_PLLRDY;
    else
        rcc->CR &= ~(uint32_t)RCC_CR_PLLRDY;
    rcc->CFGR = (rcc->CFGR & ~(uint32_t)RCC_CFGR_SWS) |
                ((rcc->CFGR & RCC_CFGR_SW) << 2U);
}

/** \brief Forward real production stores to the external hardware boundary. */
extern "C" void nx_stm32_system_model_write(volatile uint32_t* reg,
                                            uint32_t value) {
    hardware->Call(reg, value);
}

/** \brief Model the reserved vector without any foreign interrupt owner. */
extern "C" bool nx_stm32_system_model_irq_available(int) {
    return !irq_enabled;
}

/** \brief Observe the requested reserved-vector enable state. */
extern "C" int nx_stm32_system_model_irq_control(int, bool enabled) {
    irq_enabled = enabled;
    return 0;
}

/** \brief Preserve the modeled incoming interrupt mask. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t saved = {mask};
    mask = 1U;
    return saved;
}

/** \brief Restore exactly the modeled saved mask. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    mask = saved.value;
}

/** \brief Expose the complete modeled mask tuple. */
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return {mask, 0U, 0U};
}

/** \brief This cold-start fixture runs with privilege. */
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}

/** \brief This fixture has no asynchronous exception handler. */
extern "C" bool nx_arch_in_isr(void) {
    return false;
}

/** \brief Observe the incoming modeled interrupt mask. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return mask != 0U;
}

/** \brief Host stores have no physical Cortex-M data pipeline. */
extern "C" void nx_arch_dsb(void) {
}

/** \brief Host execution has no physical Cortex-M instruction pipeline. */
extern "C" void nx_arch_isb(void) {
}

/** \brief Fail the fixture if production unexpectedly enters fail-stop. */
extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    throw std::runtime_error("unexpected divided-clock terminal fault");
}
