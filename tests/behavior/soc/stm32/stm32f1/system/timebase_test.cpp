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
#include "nexus/io/watchdog.h"
#include "nexus_config.h"
#include "register_model.h"
#include "system/platform.h"

#include <cstring>

#if defined(NEXUS_CLOCK_HSE_BYPASS) && NEXUS_CLOCK_HSE_BYPASS == 1
#define TEST_HSE72 1
#endif

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
#if defined(TEST_HSE72)
extern "C" uint32_t SystemCoreClock;
constexpr uint32_t timer_prescaler = 71U;
bool hse_present;
bool allow_hsi_switch;
bool safe_pll_switch;
#else
constexpr uint32_t timer_prescaler = 7U;
#endif

class F103Timebase : public testing::Test {
  protected:
    void SetUp() override {
        std::memset(&f1_test_nvic, 0, sizeof(f1_test_nvic));
        f1_test_rcc = {};
        f1_test_flash = {};
        f1_test_timer = {};
        f1_test_rcc.CR =
            RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        f1_test_rcc.CSR = RCC_CSR_PORRSTF | RCC_CSR_SFTRSTF | RCC_CSR_WWDGRSTF;
        f1_test_rcc.AHBENR = RCC_AHBENR_FLITFEN | RCC_AHBENR_SRAMEN;
#if defined(FLASH_ACR_PRFTBE)
        f1_test_flash.ACR = FLASH_ACR_PRFTBE | FLASH_ACR_PRFTBS;
#endif
        f1_test_timer.ARR = 65535U;
        flash_kib = 128U;
        init_update = true;
        disable_irq = true;
#if defined(TEST_HSE72)
        hse_present = true;
        allow_hsi_switch = true;
        safe_pll_switch = false;
#endif
    }

    void TearDown() override {
        disable_irq = true;
#if defined(TEST_HSE72)
        allow_hsi_switch = true;
#endif
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

TEST_F(F103Timebase, RejectsForeignFlashTimingBeforeAcquiringTimer) {
    constexpr uint32_t foreign[] = {
        FLASH_ACR_HLFCYA,
#if defined(FLASH_ACR_LATENCY)
        FLASH_ACR_LATENCY_0,
#endif
    };
    const uint32_t reset = FLASH->ACR;
    for (const uint32_t bits : foreign) {
        FLASH->ACR = reset | bits;
        ASSERT_EQ(NX_ERROR_BUSY, nx_stm32f1_platform_system_start(&rb).primary);
        EXPECT_EQ(reset | bits, FLASH->ACR);
        EXPECT_EQ(0U, RCC->APB1ENR);
        EXPECT_FALSE(nx_stm32f1_system_live());
    }
}

TEST_F(F103Timebase, ResetCauseRemainsLatchedAcrossPlatformRestart) {
    constexpr uint32_t raw =
        RCC_CSR_PORRSTF | RCC_CSR_SFTRSTF | RCC_CSR_WWDGRSTF;
    constexpr uint32_t reported =
        NX_RESET_POWER_ON | NX_RESET_SOFTWARE | NX_RESET_WWDG;
    ASSERT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_SUCCESS);
    EXPECT_EQ(RCC->CSR, raw);
    EXPECT_EQ(nx_reset_cause(), reported);
    ASSERT_EQ(nx_stm32f1_platform_system_release(&rb, true), NX_SUCCESS);
    RCC->CSR = RCC_CSR_PINRSTF | RCC_CSR_IWDGRSTF | RCC_CSR_LPWRRSTF;
    ASSERT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_SUCCESS);
    EXPECT_EQ(nx_reset_cause(), reported);
}

TEST_F(F103Timebase, CountsPendingWrapBeforeAndAfterInterruptOnce) {
    ASSERT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_SUCCESS);
    EXPECT_TRUE(nx_stm32f1_system_live());
    EXPECT_EQ(f1_test_timer.PSC, timer_prescaler);
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
#if defined(TEST_HSE72)
TEST_F(F103Timebase, SwitchesOnlyAfterFlashAndBusAreSafe) {
    ASSERT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_SUCCESS);
    EXPECT_TRUE(safe_pll_switch);
    EXPECT_EQ(SystemCoreClock, 72000000U);
    EXPECT_EQ(f1_test_timer.PSC, 71U);
    EXPECT_NE(f1_test_rcc.CR & RCC_CR_HSION, 0U);
    ASSERT_EQ(nx_stm32f1_platform_system_release(&rb, true), NX_SUCCESS);
    EXPECT_EQ(SystemCoreClock, 8000000U);
    EXPECT_EQ(f1_test_rcc.CFGR, 0U);
    EXPECT_EQ(f1_test_flash.ACR & FLASH_ACR_LATENCY, 0U);
}

TEST_F(F103Timebase, AbsentMcoReturnsToHsiBeforePublishing) {
    hse_present = false;
    const auto result = nx_stm32f1_platform_system_start(&rb);
    EXPECT_EQ(result.primary, NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(f1_test_rcc.CR & (RCC_CR_HSEON | RCC_CR_HSEBYP | RCC_CR_PLLON),
              0U);
    EXPECT_EQ(SystemCoreClock, 8000000U);
    EXPECT_FALSE(nx_stm32f1_system_live());
}

TEST_F(F103Timebase, FailedReturnToHsiRetainsPllAndFlashWaitStates) {
    ASSERT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_SUCCESS);
    allow_hsi_switch = false;
    EXPECT_EQ(nx_stm32f1_platform_system_release(&rb, true), NX_ERROR_TIMEOUT);
    EXPECT_EQ(f1_test_rcc.CFGR & RCC_CFGR_SWS, RCC_CFGR_SWS_PLL);
    EXPECT_NE(f1_test_rcc.CR & RCC_CR_PLLON, 0U);
    EXPECT_NE(f1_test_rcc.CR & RCC_CR_HSEON, 0U);
    EXPECT_EQ(f1_test_flash.ACR & FLASH_ACR_LATENCY, 2U);
    EXPECT_FALSE(nx_stm32f1_system_live());
    EXPECT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_ERROR_STATE);
    allow_hsi_switch = true;
    EXPECT_EQ(nx_stm32f1_platform_system_release(&rb, true), NX_SUCCESS);
}
#endif

#if defined(RCC_CR_PLL2ON)
TEST_F(F103Timebase, RejectsRetainedConnectivityPll) {
    constexpr uint32_t sources[] = {RCC_CR_PLL2ON, RCC_CR_PLL2RDY,
                                    RCC_CR_PLL3ON, RCC_CR_PLL3RDY};
    const uint32_t reset = RCC->CR;
    for (const uint32_t source : sources) {
        RCC->CR = reset | source;
        ASSERT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_ERROR_BUSY);
        EXPECT_EQ(RCC->CR, reset | source);
        EXPECT_EQ(RCC->APB1ENR, 0U);
    }
}
#endif

#if defined(RCC_CFGR2_PREDIV1)
TEST_F(F103Timebase, RejectsNonResetExternalClockPrescaler) {
    RCC->CFGR2 = RCC_CFGR2_PREDIV1;
    EXPECT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_ERROR_BUSY);
    EXPECT_EQ(RCC->CFGR2, RCC_CFGR2_PREDIV1);
    EXPECT_EQ(RCC->APB1ENR, 0U);
}
#endif
} /* namespace */

uint16_t f1_test_flash_kib() {
    return flash_kib;
}

void f1_test_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &TIM2->SR) {
        *reg &= value;
        return;
    }
#if defined(TEST_HSE72)
    if (reg == &RCC->CR) {
        uint32_t ready = 0U;
        if ((value & RCC_CR_HSION) != 0U)
            ready |= RCC_CR_HSIRDY;
        if (hse_present && (value & RCC_CR_HSEON) != 0U)
            ready |= RCC_CR_HSERDY;
        if ((ready & RCC_CR_HSERDY) != 0U && (value & RCC_CR_PLLON) != 0U)
            ready |= RCC_CR_PLLRDY;
        *reg = (value & ~static_cast<uint32_t>(RCC_CR_HSIRDY | RCC_CR_HSERDY |
                                               RCC_CR_PLLRDY)) |
               ready;
        return;
    }
    if (reg == &RCC->CFGR) {
        uint32_t source = *reg & RCC_CFGR_SWS;
        if ((value & RCC_CFGR_SW) == RCC_CFGR_SW_PLL &&
            (RCC->CR & RCC_CR_PLLRDY) != 0U) {
            source = RCC_CFGR_SWS_PLL;
            safe_pll_switch =
                (FLASH->ACR & FLASH_ACR_LATENCY) == 2U &&
                (value & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV2 &&
                (value & (RCC_CFGR_HPRE | RCC_CFGR_PPRE2)) == 0U &&
                (value & RCC_CFGR_ADCPRE) == RCC_CFGR_ADCPRE_DIV6 &&
                (RCC->CR & RCC_CR_CSSON) != 0U;
        } else if ((value & RCC_CFGR_SW) == RCC_CFGR_SW_HSI &&
                   allow_hsi_switch) {
            source = RCC_CFGR_SWS_HSI;
        }
        *reg = (value & ~static_cast<uint32_t>(RCC_CFGR_SWS)) | source;
        return;
    }
#endif
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
