/**
 * \file            valueline_test.cpp
 * \brief           F100 TIM15, TIM16 and TIM17 fixed PWM ownership.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "instance.h"
#include "system/platform.h"
#include <array>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t peripheral_base = 0x40000000U;
constexpr size_t peripheral_bytes = 0x24000U;
constexpr uint32_t reset_mask =
    RCC_APB2RSTR_TIM15RST | RCC_APB2RSTR_TIM16RST | RCC_APB2RSTR_TIM17RST;
constexpr uint32_t pwm_gate_mask =
    RCC_APB2ENR_TIM15EN | RCC_APB2ENR_TIM16EN | RCC_APB2ENR_TIM17EN;
constexpr uint32_t input_fields = 0x44444444U;
TIM_TypeDef* fail_moe{};

class F100Pwm : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    std::array<nx_stm32f1_pwm_t, 3U> owner{};
    const std::array<nx_stm32f1_pwm_config_t, 3U> configs{{
        {TIM15, GPIOA, 8000000U, 1000U, 250U, 1000000U, 0U, 2U, 1U},
        {TIM16, GPIOB, 8000000U, 800U, 300U, 1000000U, 0U, 8U, 1U},
        {TIM17, GPIOB, 8000000U, 600U, 350U, 1000000U, 0U, 9U, 1U},
    }};
    const std::array<nx_pwm_port_t, 3U> ports{{
        {&nx_stm32f1_pwm_ops, &owner[0]},
        {&nx_stm32f1_pwm_ops, &owner[1]},
        {&nx_stm32f1_pwm_ops, &owner[2]},
    }};

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(peripheral_base), peripheral_bytes,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(peripheral_base));
        GPIOA->CRL = input_fields;
        GPIOB->CRH = input_fields;
        RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;
        fail_moe = nullptr;
    }

    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, peripheral_bytes), 0);
        }
    }
};

TEST_F(F100Pwm, StartsIndependentChannelsAndStopsInReverse) {
    for (size_t i = 0U; i < owner.size(); ++i) {
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_pwm_prepare(&owner[i], &configs[i]));
        EXPECT_EQ(0U, configs[i].registers->BDTR);
        ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(&ports[i]));
        EXPECT_EQ(static_cast<uint32_t>(TIM_BDTR_MOE),
                  configs[i].registers->BDTR);
        EXPECT_EQ(static_cast<uint32_t>(TIM_CCER_CC1E),
                  configs[i].registers->CCER);
        EXPECT_EQ(0U, configs[i].registers->DIER);
        EXPECT_EQ(configs[i].period_ticks - 1U, configs[i].registers->ARR);
        EXPECT_EQ(7U, configs[i].registers->PSC);
    }
    EXPECT_EQ(0xaU, (GPIOA->CRL >> 8U) & 0xfU);
    EXPECT_EQ(0xaaU, GPIOB->CRH & 0xffU);
    EXPECT_EQ(pwm_gate_mask, RCC->APB2ENR & pwm_gate_mask);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_set(&ports[1], 800U, 0U));
    EXPECT_EQ(250U, TIM15->CCR1);
    EXPECT_EQ(0U, TIM16->CCR1);
    EXPECT_EQ(350U, TIM17->CCR1);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_stop(&ports[2]));
    EXPECT_EQ(0x2aU, GPIOB->CRH & 0xffU);
    EXPECT_EQ(0U, TIM17->BDTR);
    EXPECT_NE(0U, TIM16->BDTR & TIM_BDTR_MOE);
    for (size_t i = owner.size(); i != 0U; --i) {
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_pwm_close(&owner[i - 1U]));
    }
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(input_fields, GPIOA->CRL);
    EXPECT_EQ(input_fields, GPIOB->CRH);
}

TEST_F(F100Pwm, DefaultRemapAndApb2ClockAreRequired) {
    AFIO->MAPR2 = AFIO_MAPR2_TIM16_REMAP;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32f1_pwm_prepare(&owner[1], &configs[1]));
    EXPECT_EQ(static_cast<uint32_t>(AFIO_MAPR2_TIM16_REMAP), AFIO->MAPR2);
    EXPECT_EQ(0U, RCC->APB2ENR);
    RCC->CFGR = RCC_CFGR_PPRE2_DIV2;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32f1_pwm_prepare(&owner[0], &configs[0]));
    EXPECT_EQ(0U, RCC->APB2ENR);
    RCC->CFGR = 0U;
    auto wrong = configs[2];
    wrong.pin = 8U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32f1_pwm_prepare(&owner[2], &wrong));
}

TEST_F(F100Pwm, FailedMasterEnableLeavesOutputParkedAndRetainsOwner) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_pwm_prepare(&owner[0], &configs[0]));
    fail_moe = TIM15;
    EXPECT_EQ(NX_ERROR_IO, nx_pwm_port_start(&ports[0]));
    EXPECT_EQ(0U, TIM15->BDTR);
    EXPECT_EQ(0U, TIM15->CCER);
    EXPECT_EQ(0U, TIM15->CR1 & TIM_CR1_CEN);
    EXPECT_EQ(2U, (GPIOA->CRL >> 8U) & 0xfU);
    EXPECT_EQ(0U, GPIOA->ODR & (1U << 2U));
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_TIM15EN);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_pwm_close(&owner[0]));
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_TIM15EN);
}
} /* namespace */

extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}

extern "C" void nx_pwm_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &RCC->APB2RSTR) {
        const uint32_t timers = value & reset_mask;
        if ((timers & RCC_APB2RSTR_TIM15RST) != 0U) {
            *TIM15 = {};
        }
        if ((timers & RCC_APB2RSTR_TIM16RST) != 0U) {
            *TIM16 = {};
        }
        if ((timers & RCC_APB2RSTR_TIM17RST) != 0U) {
            *TIM17 = {};
        }
    }
    for (auto* timer : {TIM15, TIM16, TIM17}) {
        if (reg == &timer->EGR) {
            EXPECT_EQ(static_cast<uint32_t>(TIM_EGR_UG), value);
            EXPECT_EQ(0U, timer->BDTR & TIM_BDTR_MOE);
            timer->CNT = 0U;
            timer->SR |= TIM_SR_UIF;
            return;
        }
        if (reg == &timer->SR) {
            *reg &= value;
            return;
        }
        if (reg == &timer->BDTR) {
            EXPECT_EQ(0U, value & ~static_cast<uint32_t>(TIM_BDTR_MOE));
            if (timer == fail_moe && (value & TIM_BDTR_MOE) != 0U) {
                return;
            }
        }
    }
    *reg = value;
}

extern "C" uint32_t nx_pwm_model_read(volatile const uint32_t* reg) {
    return *reg;
}

extern "C" void nx_pwm_model_bsrr(GPIO_TypeDef* reg, uint32_t value) {
    reg->ODR = (reg->ODR | (value & 0xffffU)) & ~(value >> 16U);
}
