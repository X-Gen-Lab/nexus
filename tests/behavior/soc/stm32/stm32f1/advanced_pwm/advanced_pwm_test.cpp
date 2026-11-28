/**
 * \file            advanced_pwm_test.cpp
 * \brief           F103 TIM1 complementary output and break contracts.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/advanced_pwm/instance.h"
#include "nexus/arch/arch.h"
#include "system/platform.h"
#include <gtest/gtest.h>

TIM_TypeDef f1_apwm_tim{};
GPIO_TypeDef f1_apwm_gpioa{};
GPIO_TypeDef f1_apwm_gpiob{};
RCC_TypeDef f1_apwm_rcc{};
AFIO_TypeDef f1_apwm_afio{};

constexpr uint32_t pair = TIM_CCER_CC1E | TIM_CCER_CC1NE;
constexpr uint32_t break_pin = 1U << 12U;
constexpr uint32_t protection =
    TIM_BDTR_BKE | TIM_BDTR_OSSI | TIM_BDTR_OSSR | 8U;
static bool enabled, pending, active, mask, inject_break, refuse_detach;
static uint32_t irq_priority;
static unsigned writes;

static void hardware_break() {
    GPIOB->IDR &= ~break_pin;
    if ((TIM1->BDTR & TIM_BDTR_BKE) != 0U) {
        TIM1->BDTR &= ~static_cast<uint32_t>(TIM_BDTR_MOE);
        TIM1->SR |= TIM_SR_BIF;
        pending = enabled && (TIM1->DIER & TIM_DIER_BIE) != 0U;
    }
}

extern "C" void f1_apwm_write(volatile uint32_t* reg, uint32_t value) {
    ++writes;
    if (reg == &RCC->APB2RSTR && (value & RCC_APB2RSTR_TIM1RST) != 0U) {
        f1_apwm_tim = {};
    }
    if (reg == &GPIOA->BSRR || reg == &GPIOB->BSRR) {
        GPIO_TypeDef* target = reg == &GPIOA->BSRR ? GPIOA : GPIOB;
        target->ODR = (target->ODR | (value & 0xffffU)) & ~(value >> 16U);
        return;
    }
    if (reg == &TIM1->EGR) {
        EXPECT_EQ(static_cast<uint32_t>(TIM_EGR_UG), value);
        TIM1->CNT = 0U;
        TIM1->SR |= TIM_SR_UIF;
        return;
    }
    if (reg == &TIM1->SR) {
        TIM1->SR &= value;
        if ((GPIOB->IDR & break_pin) == 0U &&
            (TIM1->BDTR & TIM_BDTR_BKE) != 0U) {
            TIM1->SR |= TIM_SR_BIF;
        }
        return;
    }
    if (reg == &TIM1->CCER && value == 0U && refuse_detach) {
        return;
    }
    *reg = value;
    if (reg == &TIM1->BDTR) {
        EXPECT_EQ(0U, value & (TIM_BDTR_AOE | TIM_BDTR_LOCK));
        if ((value & TIM_BDTR_MOE) != 0U) {
            EXPECT_EQ(pair, TIM1->CCER & pair);
            EXPECT_EQ(0xaU, GPIOA->CRH & 0xfU);
            EXPECT_EQ(0xaU, (GPIOB->CRH >> 20U) & 0xfU);
            if (inject_break) {
                hardware_break();
                GPIOB->IDR |= break_pin;
            }
        } else if ((GPIOB->IDR & break_pin) == 0U) {
            hardware_break();
        }
    }
}

extern "C" bool f1_apwm_irq_enabled(void) {
    return enabled;
}
extern "C" bool f1_apwm_irq_pending(void) {
    return pending;
}
extern "C" bool f1_apwm_irq_active(void) {
    return active;
}
extern "C" uint32_t f1_apwm_irq_priority(void) {
    return irq_priority;
}
extern "C" void f1_apwm_set_priority(uint32_t value) {
    irq_priority = value;
}
extern "C" void f1_apwm_enable_irq(void) {
    enabled = true;
}
extern "C" void f1_apwm_disable_irq(void) {
    enabled = false;
}
extern "C" void f1_apwm_clear_irq(void) {
    pending = false;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return mask;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_isb(void) {
}

extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous{mask ? 1U : 0U};
    mask = true;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t state) {
    mask = state.value != 0U;
}

class F1AdvancedPwm : public testing::Test {
  protected:
    nx_stm32f1_advanced_pwm_t owner{};
    nx_stm32f1_advanced_pwm_config_t config{800U,  8000000U, 8U,   5U,
                                            false, false,    false};
    const nx_advanced_pwm_t port{&nx_stm32f1_advanced_pwm_ops, &owner};

    void SetUp() override {
        f1_apwm_tim = {};
        f1_apwm_gpioa = {};
        f1_apwm_gpiob = {};
        f1_apwm_rcc = {};
        f1_apwm_afio = {};
        GPIOA->CRH = GPIOB->CRH = 0x44444444U;
        GPIOB->IDR = break_pin;
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;
        irq_priority = 2U;
        writes = 0;
        enabled = pending = active = mask = inject_break = refuse_detach =
            false;
    }

    void expect_parked() {
        EXPECT_EQ(0U, TIM1->BDTR & TIM_BDTR_MOE);
        EXPECT_EQ(0U, TIM1->CCER & pair);
        EXPECT_EQ(0U, TIM1->CR1 & TIM_CR1_CEN);
        EXPECT_EQ(2U, GPIOA->CRH & 0xfU);
        EXPECT_EQ(2U, (GPIOB->CRH >> 20U) & 0xfU);
    }
};

TEST_F(F1AdvancedPwm, RejectsUnsafeOptionsAndAClaimedVector) {
    config.active_high = true;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    config.active_high = false;
    config.deadtime_ticks = 0U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    config.deadtime_ticks = 8U;
    pending = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    EXPECT_EQ(0U, writes);
}

TEST_F(F1AdvancedPwm, OpenBreakNeverEnablesAnOutput) {
    GPIOB->IDR = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    EXPECT_EQ(protection, TIM1->BDTR);
    EXPECT_EQ(8U, (GPIOB->CRH >> 16U) & 0xfU);
    EXPECT_EQ(0U, GPIOB->ODR & break_pin);
    EXPECT_EQ(NX_ERROR_STATE, nx_advanced_pwm_start(&port));
    EXPECT_EQ(NX_ERROR_BUSY, nx_advanced_pwm_clear_fault(&port));
    expect_parked();
}

TEST_F(F1AdvancedPwm, UsesPreloadedDutyAndRestoresClockPadsVector) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_set_duty(&port, 400U));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_start(&port));
    EXPECT_EQ(400U, TIM1->CCR1);
    EXPECT_EQ(799U, TIM1->ARR);
    EXPECT_EQ(0U, TIM1->PSC);
    EXPECT_TRUE(enabled);
    EXPECT_EQ(5U, irq_priority);
    nx_advanced_pwm_state_t observed{};
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_state(&port, &observed));
    EXPECT_TRUE(observed.running);
    EXPECT_EQ(8U, observed.deadtime_ticks);
    EXPECT_EQ(8000000U, observed.deadtime_hz);
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_stop(&port));
    expect_parked();
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_close(&owner));
    EXPECT_EQ(0x44444444U, GPIOA->CRH);
    EXPECT_EQ(0x44444444U, GPIOB->CRH);
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_FALSE(enabled);
    EXPECT_EQ(2U, irq_priority);
}

TEST_F(F1AdvancedPwm, ATransientBreakAtEnableRequiresExplicitRecovery) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    inject_break = true;
    EXPECT_EQ(NX_ERROR_STATE, nx_advanced_pwm_start(&port));
    expect_parked();
    inject_break = false;
    EXPECT_EQ(NX_ERROR_STATE, nx_advanced_pwm_start(&port));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_clear_fault(&port));
    expect_parked();
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_start(&port));
    hardware_break();
    nx_stm32f1_advanced_pwm_irq(&owner);
    EXPECT_EQ(0U, TIM1->DIER & TIM_DIER_BIE);
    EXPECT_EQ(0U, TIM1->BDTR & TIM_BDTR_MOE);
    EXPECT_NE(0U, TIM1->SR & TIM_SR_BIF);
    EXPECT_EQ(NX_ERROR_STATE, nx_advanced_pwm_start(&port));
}

TEST_F(F1AdvancedPwm, ShutdownFailureParksPinsAndRetainsTheExactOwner) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_start(&port));
    refuse_detach = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_advanced_pwm_close(&owner));
    EXPECT_EQ(0U, TIM1->BDTR & TIM_BDTR_MOE);
    EXPECT_EQ(2U, GPIOA->CRH & 0xfU);
    EXPECT_EQ(2U, (GPIOB->CRH >> 20U) & 0xfU);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_TIM1EN);
    refuse_detach = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_close(&owner));
    EXPECT_EQ(0U, RCC->APB2ENR);
}
