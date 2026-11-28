/**
 * \file            timebase_test.cpp
 * \brief           F103 platform timebase, density and withdrawal contract.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include <gtest/gtest.h>

#include "nexus/core/time.h"
#include "register_model.h"
#include "system/platform.h"

#include <cstring>

RCC_TypeDef f1_test_rcc;
FLASH_TypeDef f1_test_flash;
TIM_TypeDef f1_test_timer;
NVIC_Type f1_test_nvic;

namespace {
constexpr nx_stm32f1_platform_config_t rb{128U * 1024U};
constexpr nx_stm32f1_platform_config_t c8{64U * 1024U};

uint16_t flash_kib;
bool init_update;
bool disable_irq;

class F103Timebase : public testing::Test {
  protected:
    void SetUp() override {
        std::memset(&f1_test_nvic, 0, sizeof(f1_test_nvic));
        f1_test_rcc = {};
        f1_test_flash = {};
        f1_test_timer = {};
        f1_test_rcc.CR =
            RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        f1_test_timer.ARR = 65535U;
        flash_kib = 128U;
        init_update = true;
        disable_irq = true;
    }

    void TearDown() override {
        disable_irq = true;
        EXPECT_EQ(nx_stm32f1_platform_system_release(&rb, true), NX_SUCCESS);
    }
};

TEST_F(F103Timebase, WrongDensityDoesNotAcquireTimer) {
    EXPECT_EQ(nx_stm32f1_platform_system_start(&c8).primary, NX_ERROR_IO);
    EXPECT_EQ(f1_test_rcc.APB1ENR, 0U);
    EXPECT_FALSE(nx_stm32f1_system_live());
    flash_kib = 64U;
    EXPECT_EQ(nx_stm32f1_platform_system_start(&c8).primary, NX_SUCCESS);
}

TEST_F(F103Timebase, CountsPendingWrapBeforeAndAfterInterruptOnce) {
    ASSERT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_SUCCESS);
    EXPECT_TRUE(nx_stm32f1_system_live());
    EXPECT_EQ(f1_test_timer.PSC, 7U);
    f1_test_timer.CNT = 65535U;
    EXPECT_EQ(nx_time_now_us(), 65535U);
    f1_test_timer.CNT = 7U;
    f1_test_timer.SR = TIM_SR_UIF;
    EXPECT_EQ(nx_time_now_us(), 65543U);
    TIM2_IRQHandler();
    EXPECT_EQ(nx_time_now_us(), 65543U);
    EXPECT_EQ(f1_test_timer.SR, 0U);
}

TEST_F(F103Timebase, MissingUpdateWithdrawsTimerGate) {
    init_update = false;
    const auto result = nx_stm32f1_platform_system_start(&rb);
    EXPECT_EQ(result.primary, NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(f1_test_rcc.APB1ENR, 0U);
}

TEST_F(F103Timebase, FailedVectorWithdrawalRetainsClockUntilRetry) {
    NVIC_SetPriority(TIM2_IRQn, 6U);
    ASSERT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_SUCCESS);
    disable_irq = false;
    EXPECT_EQ(nx_stm32f1_platform_system_release(&rb, true), NX_ERROR_IO);
    EXPECT_NE(f1_test_rcc.APB1ENR & RCC_APB1ENR_TIM2EN, 0U);
    EXPECT_FALSE(nx_stm32f1_system_live());
    EXPECT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_ERROR_STATE);
    disable_irq = true;
    EXPECT_EQ(nx_stm32f1_platform_system_release(&rb, true), NX_SUCCESS);
    EXPECT_EQ(f1_test_rcc.APB1ENR, 0U);
    EXPECT_EQ(NVIC_GetPriority(TIM2_IRQn), 6U);
    EXPECT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_SUCCESS);
}
} /* namespace */

uint16_t f1_test_flash_kib() {
    return flash_kib;
}

void f1_test_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &TIM2->SR) {
        *reg &= value;
        return;
    }
    *reg = value;
    if (reg == &TIM2->EGR && init_update) {
        TIM2->SR |= TIM_SR_UIF;
    }
    if (reg == &RCC->APB1RSTR && (value & RCC_APB1RSTR_TIM2RST) != 0U) {
        f1_test_timer = {};
        f1_test_timer.ARR = 65535U;
    }
}

void f1_test_enable_irq(IRQn_Type irq) {
    NVIC->ISER[0] |= 1U << static_cast<uint32_t>(irq);
}

void f1_test_disable_irq(IRQn_Type irq) {
    if (disable_irq)
        NVIC->ISER[0] &= ~(1U << static_cast<uint32_t>(irq));
}

void f1_test_clear_pending_irq(IRQn_Type irq) {
    NVIC->ISPR[0] &= ~(1U << static_cast<uint32_t>(irq));
}
