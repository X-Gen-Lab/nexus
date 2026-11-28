/**
 * \file            window_watchdog_test.cpp
 * \brief           F103 WWDG cold ownership, reload window and retained fault
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/watchdog/window_watchdog.h"
#include "nexus/arch/arch.h"
#include "system/platform.h"
#include <gtest/gtest.h>

RCC_TypeDef f103_wwdg_rcc{};
WWDG_TypeDef f103_wwdg_regs{};
static struct {
    uint32_t writes;
    uint32_t config_writes;
    uint32_t activations;
    uint32_t reloads;
    uint32_t vector;
    uint32_t exception;
    bool live;
    bool privileged;
    bool irq;
    bool masked;
    bool lose_activation;
    bool lose_narrowing;
    bool lose_wide;
    bool lose_gate_release;
} model;

extern "C" void f103_wwdg_write(volatile uint32_t* reg, uint32_t value) {
    ++model.writes;
    if (reg == &WWDG->CR) {
        EXPECT_TRUE(model.masked);
        EXPECT_NE(0U, value & WWDG_CR_WDGA);
        EXPECT_NE(0U, value & WWDG_CR_T_6);
        EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_WWDGEN);
        const uint32_t previous = WWDG->CR & WWDG_CR_T;
        const uint32_t window = WWDG->CFR & WWDG_CFR_W;
        if ((WWDG->CR & WWDG_CR_WDGA) == 0U) {
            ++model.activations;
            /* Conservative hardware model compares the old counter even at
             * activation; only a full window tolerates any valid old count. */
            EXPECT_LE(previous, window);
            EXPECT_EQ(WWDG_CFR_W, window);
            if (model.lose_activation) {
                return;
            }
        } else {
            ++model.reloads;
            EXPECT_GT(previous, WWDG_CR_T_6);
            EXPECT_LT(previous, window);
        }
    } else if (reg == &WWDG->CFR) {
        ++model.config_writes;
        EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_WWDGEN);
        EXPECT_EQ(0U, value & WWDG_CFR_EWI);
        const bool active = (WWDG->CR & WWDG_CR_WDGA) != 0U;
        if ((!active && model.lose_wide) || (active && model.lose_narrowing)) {
            return;
        }
    } else {
        EXPECT_EQ(&RCC->APB1ENR, reg);
        if (model.lose_gate_release && (value & RCC_APB1ENR_WWDGEN) == 0U) {
            return;
        }
    }
    *reg = value;
}

extern "C" uint32_t f103_wwdg_vector(IRQn_Type irq) {
    EXPECT_EQ(WWDG_IRQn, irq);
    return model.vector;
}

extern "C" bool nx_stm32f1_system_live(void) {
    return model.live;
}

extern "C" bool nx_arch_is_privileged(void) {
    return model.privileged;
}

extern "C" bool nx_arch_in_isr(void) {
    return model.irq;
}

extern "C" uint32_t nx_arch_exception_number(void) {
    return model.exception;
}

extern "C" bool nx_arch_irq_is_masked(void) {
    return model.masked;
}

extern "C" void nx_arch_dsb(void) {
}

extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous{model.masked ? 1U : 0U};
    model.masked = true;
    return previous;
}

extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    model.masked = previous.value != 0U;
}

class F103WindowWatchdog : public testing::Test {
  protected:
    nx_stm32f1_wwdg_t owner{};
    nx_watchdog_state_t state{};
    const nx_watchdog_port_t face{&nx_stm32f1_wwdg_ops, &owner};

    void SetUp() override {
        model = {};
        model.live = true;
        model.privileged = true;
        RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
        RCC->APB1RSTR = 0U;
        RCC->CR = RCC_CR_HSIRDY;
        RCC->CFGR = RCC_CFGR_SWS_HSI;
        WWDG->CFR = WWDG_CFR_W;
        WWDG->CR = WWDG_CR_T;
        WWDG->SR = WWDG_SR_EWIF;
    }

    void Counter(uint32_t counter) {
        WWDG->CR = WWDG_CR_WDGA | counter;
    }
};

TEST_F(F103WindowWatchdog, RejectsForeignGateVectorAndWrongClockBeforeWrites) {
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32f1_wwdg_admit(&owner, 0U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32f1_wwdg_admit(&owner, 63U));
    RCC->APB1ENR |= RCC_APB1ENR_WWDGEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_wwdg_admit(&owner, 3U));
    RCC->APB1ENR &= ~static_cast<uint32_t>(RCC_APB1ENR_WWDGEN);
    model.vector = 2U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_wwdg_prepare(&owner, 3U));
    model.vector = 0U;
    RCC->CFGR = RCC_CFGR_PPRE1_DIV2;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32f1_wwdg_prepare(&owner, 3U));
    RCC->CFGR = RCC_CFGR_SWS_HSI;
    model.live = false;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f1_wwdg_prepare(&owner, 3U));
    ASSERT_EQ(0U, model.writes);
    model.live = true;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_wwdg_prepare(&owner, 3U));
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_state(&face, &state));
    EXPECT_FALSE(state.enabled);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_wwdg_close(&owner));
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
}

TEST_F(F103WindowWatchdog, EarlyAndLateNeverWriteAndValidIrqFeedWritesOnce) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_wwdg_prepare(&owner, 3U));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_watchdog_port_enable(&face, 5000U, true, &state));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_watchdog_port_enable(&face, 2048U, false, &state));
    EXPECT_EQ(1U, model.writes);
    ASSERT_EQ(NX_SUCCESS, nx_watchdog_port_enable(&face, 5000U, false, &state));
    EXPECT_EQ(73U, WWDG->CR & WWDG_CR_T);
    EXPECT_EQ(71U, WWDG->CFR);
    EXPECT_EQ(1U, model.activations);
    EXPECT_EQ(0U, model.reloads);
    EXPECT_TRUE(state.enabled);
    EXPECT_TRUE(state.irreversible);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_watchdog_port_state(&face, &state));
    EXPECT_EQ(0U, state.minimum_timeout_us);
    EXPECT_EQ(0U, state.maximum_timeout_us);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_watchdog_port_enable(&face, 5000U, false, &state));
    Counter(71U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_watchdog_port_feed(&face));
    Counter(64U);
    EXPECT_EQ(NX_ERROR_STATE, nx_watchdog_port_feed(&face));
    Counter(63U);
    EXPECT_EQ(NX_ERROR_STATE, nx_watchdog_port_feed(&face));
    EXPECT_EQ(0U, model.reloads);

    Counter(70U);
    model.irq = true;
    model.exception = 2U;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_watchdog_port_feed(&face));
    model.exception = 16U;
    model.masked = true;
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_feed(&face));
    EXPECT_TRUE(model.masked);
    EXPECT_EQ(1U, model.reloads);
    EXPECT_EQ(73U, WWDG->CR & WWDG_CR_T);
    EXPECT_EQ(WWDG_SR_EWIF, WWDG->SR);
    model.irq = false;
    model.masked = false;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32f1_wwdg_close(&owner));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_WWDGEN);
}

TEST_F(F103WindowWatchdog, WindowWidthAndPrescalerBoundaries) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_wwdg_prepare(&owner, 62U));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_watchdog_port_enable(&face, 32256U, false, &state));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_watchdog_port_enable(&face, 262145U, false, &state));
    ASSERT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 262144U, false, &state));
    EXPECT_EQ(127U, WWDG->CR & WWDG_CR_T);
    EXPECT_EQ(66U | WWDG_CFR_WDGTB, WWDG->CFR);
    EXPECT_EQ(2U, model.config_writes);
    Counter(66U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_watchdog_port_feed(&face));
    Counter(65U);
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_feed(&face));
    Counter(64U);
    EXPECT_EQ(NX_ERROR_STATE, nx_watchdog_port_feed(&face));
}

TEST_F(F103WindowWatchdog, UnverifiedActivationNeverRetries) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_wwdg_prepare(&owner, 3U));
    model.lose_activation = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 5000U, false, &state));
    EXPECT_TRUE(state.irreversible);
    EXPECT_EQ(1U, model.activations);
    EXPECT_EQ(0U, model.config_writes);
    EXPECT_EQ(NX_ERROR_STATE, nx_watchdog_port_feed(&face));
    EXPECT_EQ(NX_ERROR_STATE,
              nx_watchdog_port_enable(&face, 5000U, false, &state));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32f1_wwdg_close(&owner));
    EXPECT_EQ(1U, model.activations);
}

TEST_F(F103WindowWatchdog,
       UnverifiedNarrowingRetainsActivationAndDisallowsFeed) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_wwdg_prepare(&owner, 3U));
    model.lose_narrowing = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 5000U, false, &state));
    EXPECT_TRUE(state.enabled);
    EXPECT_EQ(1U, model.activations);
    EXPECT_EQ(1U, model.config_writes);
    Counter(70U);
    EXPECT_EQ(NX_ERROR_STATE, nx_watchdog_port_feed(&face));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32f1_wwdg_close(&owner));
    EXPECT_EQ(0U, model.reloads);
}

TEST_F(F103WindowWatchdog, AlteredHardwareDisallowsFeedingWithoutSideEffects) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_wwdg_prepare(&owner, 3U));
    ASSERT_EQ(NX_SUCCESS, nx_watchdog_port_enable(&face, 5000U, false, &state));
    Counter(70U);
    RCC->CFGR = RCC_CFGR_PPRE1_DIV2;
    EXPECT_EQ(NX_ERROR_STATE, nx_watchdog_port_feed(&face));
    RCC->CFGR = RCC_CFGR_SWS_HSI;
    WWDG->CFR |= WWDG_CFR_EWI;
    EXPECT_EQ(NX_ERROR_STATE, nx_watchdog_port_feed(&face));
    WWDG->CFR &= ~static_cast<uint32_t>(WWDG_CFR_EWI);
    RCC->APB1ENR &= ~static_cast<uint32_t>(RCC_APB1ENR_WWDGEN);
    EXPECT_EQ(NX_ERROR_STATE, nx_watchdog_port_feed(&face));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_watchdog_port_state(&face, &state));
    EXPECT_TRUE(state.irreversible);
    EXPECT_EQ(0U, model.reloads);
}

TEST_F(F103WindowWatchdog, FailureToCloseRetainsGateButRevokesActivation) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_wwdg_prepare(&owner, 3U));
    model.lose_gate_release = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_wwdg_close(&owner));
    EXPECT_EQ(NX_ERROR_STATE,
              nx_watchdog_port_enable(&face, 5000U, false, &state));
    EXPECT_EQ(0U, model.activations);
    model.lose_gate_release = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_wwdg_close(&owner));
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
}

TEST_F(F103WindowWatchdog,
       WatchdogObservedDuringGateAcquisitionIsIrreversible) {
    Counter(65U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_wwdg_prepare(&owner, 3U));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_watchdog_port_state(&face, &state));
    EXPECT_TRUE(state.enabled);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32f1_wwdg_close(&owner));
    EXPECT_EQ(1U, model.writes);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_WWDGEN);
}

TEST_F(F103WindowWatchdog, PrescalerFailureRemainsReversibleAndOwnsNoReload) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_wwdg_prepare(&owner, 3U));
    model.lose_wide = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 65536U, false, &state));
    EXPECT_FALSE(state.irreversible);
    EXPECT_EQ(0U, model.activations);
    model.lose_wide = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_wwdg_close(&owner));
    EXPECT_EQ(WWDG_CFR_W, WWDG->CFR);
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
}
