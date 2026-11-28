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

#if defined(NEXUS_ARCH_SLEEP_MODEL)
#include "arch_sleep_model.h"
#include "nexus/arch/arch.h"
#include "nexus/arch/sleep.h"
#endif

#if defined(TEST_F1_STOP)
#include "nexus/arch/arch.h"
#include "nexus/components/clock_domain_owner.h"
#include "nexus_bindings.h"
#include "system/stop_internal.h"
#endif

#include <cstring>

#if defined(NEXUS_CLOCK_HSE_BYPASS)
#define TEST_HSE_PLL 1
#if defined(STM32F105xC) || defined(STM32F107xC)
#define TEST_PLL2 1
#endif
#endif

RCC_TypeDef f1_test_rcc;
FLASH_TypeDef f1_test_flash;
TIM_TypeDef f1_test_timer;
NVIC_Type f1_test_nvic;

#if defined(TEST_F1_STOP) || defined(TEST_F1_STANDBY)
AFIO_TypeDef f1_test_afio;
EXTI_TypeDef f1_test_exti;
PWR_TypeDef f1_test_power;
RTC_TypeDef f1_test_rtc;
SCB_Type f1_test_scb{};
SysTick_Type f1_test_systick{};
#endif

#if defined(TEST_F1_STOP)
const nx_exti_ops_t nx_stm32f1_exti_ops{};
unsigned platform_suspend_calls;
nx_result_t platform_suspend_status;

extern "C" nx_result_t nx_platform_suspend(void) {
    ++platform_suspend_calls;
    return platform_suspend_status;
}
#endif

namespace {
constexpr nx_stm32f1_platform_config_t rb{128U * 1024U};
constexpr nx_stm32f1_platform_config_t c8{64U * 1024U};

uint16_t flash_kib;
bool init_update;
bool disable_irq;
#if defined(NEXUS_ARCH_SLEEP_MODEL)
uint32_t sleep_control;
unsigned wfi_count;
#endif
#if defined(TEST_HSE_PLL)
extern "C" uint32_t SystemCoreClock;
#if defined(STM32F101xB)
constexpr uint32_t sysclk = 36000000U;
constexpr uint32_t flash_latency = 1U;
constexpr uint32_t clock_fields = RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE_HSE_DIV2 |
                                  RCC_CFGR_PLLMULL9 | RCC_CFGR_ADCPRE_DIV4;
constexpr uint32_t extra_clock_fields = 0U;
#elif defined(STM32F102xB)
constexpr uint32_t sysclk = 48000000U;
constexpr uint32_t flash_latency = 1U;
constexpr uint32_t clock_fields = RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL6 |
                                  RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV4 |
                                  RCC_CFGR_USBPRE;
constexpr uint32_t extra_clock_fields = RCC_CFGR_USBPRE;
#else
constexpr uint32_t sysclk = 72000000U;
constexpr uint32_t flash_latency = 2U;
constexpr uint32_t clock_fields = RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9 |
                                  RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV6;
constexpr uint32_t extra_clock_fields = 0U;
#endif
constexpr uint32_t timer_prescaler = sysclk / 1000000U - 1U;
bool hse_present;
bool allow_hsi_switch;
bool safe_pll_switch;
#if defined(TEST_PLL2)
bool pll2_locks;
bool pll_locks;
bool pll2_stops;
constexpr uint32_t pll2_config = RCC_CFGR2_PREDIV1_DIV5 |
                                 RCC_CFGR2_PREDIV2_DIV5 | RCC_CFGR2_PLL2MUL8 |
                                 RCC_CFGR2_PREDIV1SRC_PLL2;
#endif
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
#if defined(NEXUS_ARCH_SLEEP_MODEL)
        sleep_control = 0U;
        wfi_count = 0U;
#endif
#if defined(TEST_HSE_PLL)
        hse_present = true;
        allow_hsi_switch = true;
        safe_pll_switch = false;
#endif
#if defined(TEST_PLL2)
        pll2_locks = pll_locks = pll2_stops = true;
#endif
    }

    void TearDown() override {
        disable_irq = true;
#if defined(TEST_HSE_PLL)
        allow_hsi_switch = true;
#endif
#if defined(TEST_PLL2)
        pll2_stops = true;
#endif
        EXPECT_EQ(nx_stm32f1_platform_system_release(&rb, true), NX_SUCCESS);
    }
};

#if defined(NEXUS_ARCH_SLEEP_MODEL)
TEST_F(F103Timebase, ShallowWfiPreservesTheLiveTim2EpochAndClockOwners) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_platform_system_start(&rb).primary);
    const uint32_t clock = RCC->CR;
    const uint32_t dividers = RCC->CFGR;
    const uint32_t backup = RCC->BDCR;
    const uint32_t ahb = RCC->AHBENR;
    const uint32_t apb = RCC->APB1ENR;

    uint32_t sequence = 0U;
    bool slept = true;
    TIM2->CNT = 65534U;
    ASSERT_EQ(65534U, nx_time_now_us());
    ASSERT_EQ(NX_ARCH_OK, nx_arch_idle_if_unchanged(&sequence, 1U, &slept));
    EXPECT_FALSE(slept);
    EXPECT_EQ(0U, wfi_count);

    ASSERT_EQ(NX_ARCH_OK, nx_arch_idle_if_unchanged(&sequence, 0U, &slept));
    EXPECT_TRUE(slept);
    EXPECT_EQ(1U, wfi_count);
    EXPECT_EQ(0U, nx_arch_irq_masks().primask);
    EXPECT_NE(0U, TIM2->SR & TIM_SR_UIF);
    EXPECT_EQ(65540U, nx_time_now_us());
    TIM2_IRQHandler();
    EXPECT_EQ(65540U, nx_time_now_us());
    EXPECT_TRUE(nx_stm32f1_system_live());
    EXPECT_EQ(clock, RCC->CR);
    EXPECT_EQ(dividers, RCC->CFGR);
    EXPECT_EQ(backup, RCC->BDCR);
    EXPECT_EQ(ahb, RCC->AHBENR);
    EXPECT_EQ(apb, RCC->APB1ENR);
    EXPECT_EQ(0U, RCC->APB1ENR & (RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN));
}

TEST_F(F103Timebase, ShallowWfiRefusesInheritedSleepOnExitOrDeepSleep) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_platform_system_start(&rb).primary);
    const uint32_t sequence = 0U;
    constexpr uint32_t alternate_modes[] = {SCB_SCR_SLEEPONEXIT_Msk,
                                            SCB_SCR_SLEEPDEEP_Msk};
    for (const uint32_t mode : alternate_modes) {
        sleep_control = mode;
        bool slept = true;
        EXPECT_EQ(NX_ARCH_UNSUPPORTED,
                  nx_arch_idle_if_unchanged(&sequence, 0U, &slept));
        EXPECT_FALSE(slept);
        EXPECT_EQ(mode, sleep_control);
        EXPECT_EQ(0U, nx_arch_irq_masks().primask);
    }
    EXPECT_EQ(0U, wfi_count);
}
#endif

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
#if defined(TEST_HSE_PLL)
TEST_F(F103Timebase, SwitchesOnlyAfterFlashAndBusAreSafe) {
    ASSERT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_SUCCESS);
    EXPECT_TRUE(safe_pll_switch);
    EXPECT_EQ(SystemCoreClock, sysclk);
    EXPECT_EQ(f1_test_timer.PSC, timer_prescaler);
    EXPECT_NE(f1_test_rcc.CR & RCC_CR_HSION, 0U);
    const uint32_t bypass = NEXUS_CLOCK_HSE_BYPASS ? RCC_CR_HSEBYP : 0U;
    EXPECT_EQ(f1_test_rcc.CR & RCC_CR_HSEBYP, bypass);
    ASSERT_EQ(nx_stm32f1_platform_system_release(&rb, true), NX_SUCCESS);
    EXPECT_EQ(SystemCoreClock, 8000000U);
    EXPECT_EQ(f1_test_rcc.CFGR, 0U);
    EXPECT_EQ(f1_test_flash.ACR & FLASH_ACR_LATENCY, 0U);
}

TEST_F(F103Timebase, AbsentHseReturnsToHsiBeforePublishing) {
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
    EXPECT_EQ(f1_test_flash.ACR & FLASH_ACR_LATENCY, flash_latency);
    EXPECT_FALSE(nx_stm32f1_system_live());
    EXPECT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_ERROR_STATE);
    allow_hsi_switch = true;
    EXPECT_EQ(nx_stm32f1_platform_system_release(&rb, true), NX_SUCCESS);
}
#endif

#if defined(STM32F102xB) && defined(TEST_HSE_PLL)
TEST_F(F103Timebase, RetainsPll48UntilUsbGateIsReleased) {
    ASSERT_EQ(nx_stm32f1_platform_system_start(&rb).primary, NX_SUCCESS);
    EXPECT_EQ(RCC->APB1ENR & RCC_APB1ENR_USBEN, 0U);
    EXPECT_NE(RCC->CFGR & RCC_CFGR_USBPRE, 0U);
    RCC->APB1ENR = RCC->APB1ENR | RCC_APB1ENR_USBEN;
    EXPECT_EQ(nx_stm32f1_platform_system_release(&rb, true), NX_ERROR_BUSY);
    EXPECT_NE(RCC->CFGR & RCC_CFGR_USBPRE, 0U);
    EXPECT_NE(RCC->CR & RCC_CR_PLLRDY, 0U);
    RCC->APB1ENR = RCC->APB1ENR & ~static_cast<uint32_t>(RCC_APB1ENR_USBEN);
    EXPECT_EQ(nx_stm32f1_platform_system_release(&rb, true), NX_SUCCESS);
    EXPECT_EQ(RCC->CFGR & RCC_CFGR_USBPRE, 0U);
}
#endif

#if defined(TEST_PLL2)
TEST_F(F103Timebase, LinksBothPllsWithoutOpeningOtgClock) {
    const uint32_t ahb_idle = RCC->AHBENR;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_platform_system_start(&rb).primary);
    EXPECT_EQ(pll2_config, RCC->CFGR2);
    EXPECT_EQ(0U, RCC->CR & (RCC_CR_HSEBYP | RCC_CR_PLL3ON | RCC_CR_PLL3RDY));
    EXPECT_NE(0U, RCC->CR & RCC_CR_PLL2RDY);
    EXPECT_EQ(0U, RCC->CFGR & RCC_CFGR_OTGFSPRE);
    EXPECT_EQ(ahb_idle, RCC->AHBENR);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_platform_system_release(&rb, true));
    EXPECT_EQ(0U, RCC->CFGR2);
    EXPECT_EQ(0U, RCC->CR & (RCC_CR_HSEON | RCC_CR_PLLON | RCC_CR_PLL2ON));
}

TEST_F(F103Timebase, FailureOfEitherPllRestoresHsiAndClockFields) {
    for (const bool pll2_fail : {true, false}) {
        pll2_locks = !pll2_fail;
        pll_locks = pll2_fail;
        const auto result = nx_stm32f1_platform_system_start(&rb);
        EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(0U, RCC->CFGR);
        EXPECT_EQ(0U, RCC->CFGR2);
        EXPECT_EQ(0U, RCC->APB1ENR);
        EXPECT_FALSE(nx_stm32f1_system_live());
    }
}

TEST_F(F103Timebase, LatePll2WithdrawalRetainsHseAndCanRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_platform_system_start(&rb).primary);
    const uint32_t ahb_idle = RCC->AHBENR;
    RCC->AHBENR = ahb_idle | RCC_AHBENR_OTGFSEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_platform_system_release(&rb, true));
    EXPECT_EQ(RCC_CFGR_SWS_PLL, RCC->CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(ahb_idle | RCC_AHBENR_OTGFSEN, RCC->AHBENR);
    RCC->AHBENR = ahb_idle;

    pll2_stops = false;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32f1_platform_system_release(&rb, true));
    EXPECT_EQ(RCC_CFGR_SWS_HSI, RCC->CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(0U, RCC->CR & (RCC_CR_PLLON | RCC_CR_PLLRDY));
    EXPECT_NE(0U, RCC->CR & RCC_CR_HSEON);
    EXPECT_EQ(pll2_config, RCC->CFGR2);
    EXPECT_FALSE(nx_stm32f1_system_live());
    pll2_stops = true;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_platform_system_release(&rb, true));
    EXPECT_EQ(0U, RCC->CFGR2);
}

TEST_F(F103Timebase, RefusesForeignOtgGateResetOrReadyIrq) {
    const uint32_t reset = RCC->CR;
    const uint32_t ahb_idle = RCC->AHBENR;
    RCC->AHBENR = ahb_idle | RCC_AHBENR_OTGFSEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_platform_system_start(&rb).primary);
    EXPECT_EQ(ahb_idle | RCC_AHBENR_OTGFSEN, RCC->AHBENR);
    EXPECT_EQ(reset, RCC->CR);
    RCC->AHBENR = ahb_idle;
    RCC->AHBRSTR = RCC_AHBRSTR_OTGFSRST;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_platform_system_start(&rb).primary);
    EXPECT_EQ(RCC_AHBRSTR_OTGFSRST, RCC->AHBRSTR);
    EXPECT_EQ(reset, RCC->CR);
    RCC->AHBRSTR = 0U;
    RCC->CIR = RCC_CIR_PLL3RDYIE;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_platform_system_start(&rb).primary);
    EXPECT_EQ(RCC_CIR_PLL3RDYIE, RCC->CIR);
    EXPECT_EQ(reset, RCC->CR);
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
#if defined(TEST_F1_STOP)
constexpr uint32_t wake_line = 1U << 13U;
constexpr uint32_t wake_vector = 1U << (EXTI15_10_IRQn - 32U);
unsigned stop_waits;
bool signal_at_wait;
bool signal_at_arm;
bool clock_lost;
bool deep_stuck;

class F103Stop : public F103Timebase {
  protected:
    nx_clock_domain_t domain{};
    nx_clock_barrier_t barrier{};
    nx_stm32f1_exti_config_t wake_config{};
    nx_stm32f1_exti_t wake{};

    void SetUp() override {
        F103Timebase::SetUp();
        SCB->SCR = SCB->ICSR = SCB->SHCSR = 0U;
        SysTick->LOAD = SysTick->VAL = 0U;
        f1_test_afio = {};
        f1_test_exti = {};
        f1_test_power = {};
        f1_test_rtc = {};
        stop_waits = 0U;
        platform_suspend_calls = 0U;
        platform_suspend_status = NX_SUCCESS;
        signal_at_wait = true;
        signal_at_arm = false;
        clock_lost = false;
        deep_stuck = false;
        FLASH->OBR = FLASH_OBR_IWDG_SW | FLASH_OBR_nRST_STOP;
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_platform_system_start(&rb).primary);
        ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
        ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
        wake_config.gpio = GPIOC;
        wake_config.line = 13U;
        wake_config.gpio_index = 2U;
        wake_config.edge = NX_EXTI_FALLING;
        wake.config = &wake_config;
        wake.live = wake.line_owned = wake.vector_owned = true;
        RCC->APB2ENR = RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPCEN;
        AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI13_PC;
        EXTI->FTSR = EXTI->IMR = wake_line;
        NVIC->ISER[1] = wake_vector;
        TIM2->CNT = 37U;
    }
};

TEST_F(F103Stop, PublicOwnerSuspendsBeforeArmingAndResumesOnlyAfterWithdrawal) {
    const nx_exti_port_t port{&nx_stm32f1_exti_ops, &wake};
    const auto observed = nx_stm32f103_stop_enter(barrier, &port);
    EXPECT_EQ(1U, platform_suspend_calls);
    EXPECT_TRUE(observed.platform_suspended);
    EXPECT_TRUE(observed.resume_ready);
    EXPECT_TRUE(observed.wake_pending);
    EXPECT_EQ(NX_SUCCESS, observed.status);
    EXPECT_EQ(NX_SUCCESS, observed.cleanup);
    EXPECT_EQ(observed.previous_epoch + 1U, observed.current_epoch);
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_PWREN);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_stop_wake_resume(&wake));
}

TEST_F(F103Stop, PublicOwnerPreservesThePlatformPartialSuspendContract) {
    const nx_exti_port_t port{&nx_stm32f1_exti_ops, &wake};
    platform_suspend_status = NX_ERROR_BUSY;
    const auto observed = nx_stm32f103_stop_enter(barrier, &port);
    EXPECT_EQ(1U, platform_suspend_calls);
    EXPECT_EQ(NX_ERROR_BUSY, observed.status);
    EXPECT_FALSE(observed.platform_suspended);
    EXPECT_FALSE(observed.resume_ready);
    EXPECT_EQ(37U, nx_time_now_us());
    EXPECT_EQ(0U, stop_waits);
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_PWREN);
}

TEST_F(F103Stop, PublicResumeCannotContinueAfterFailedTimeRestore) {
    const nx_exti_port_t port{&nx_stm32f1_exti_ops, &wake};
    clock_lost = true;
    const auto observed = nx_stm32f103_stop_enter(barrier, &port);
    EXPECT_TRUE(observed.platform_suspended);
    EXPECT_FALSE(observed.resume_ready);
    EXPECT_EQ(NX_ERROR_IO, observed.cleanup);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f1_stop_wake_resume(&wake));
    EXPECT_EQ(NX_SUCCESS, nx_clock_domain_quiescent(barrier));
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_PWREN);
    EXPECT_EQ(wake_line, EXTI->PR);
}

TEST_F(F103Stop, RefusesResumeWhenSleepdeepCannotBeWithdrawn) {
    const nx_exti_port_t port{&nx_stm32f1_exti_ops, &wake};
    deep_stuck = true;
    const auto observed = nx_stm32f103_stop_enter(barrier, &port);
    EXPECT_TRUE(observed.platform_suspended);
    EXPECT_FALSE(observed.resume_ready);
    EXPECT_EQ(NX_ERROR_IO, observed.cleanup);
    EXPECT_GT(observed.current_epoch, observed.previous_epoch);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f1_stop_wake_resume(&wake));
    EXPECT_EQ(NX_SUCCESS, nx_clock_domain_quiescent(barrier));
}

TEST_F(F103Stop, AdvancesEpochBeforeTheWakeIrqCanRun) {
    const auto observed = nx_stm32f1_stop_hsi8(barrier, &wake);
    ASSERT_EQ(NX_SUCCESS, observed.status);
    EXPECT_EQ(NX_SUCCESS, observed.cleanup);
    EXPECT_EQ(37U, observed.before_us);
    EXPECT_EQ(observed.previous_epoch + 1U, observed.current_epoch);
    EXPECT_TRUE(observed.wait_issued);
    EXPECT_TRUE(observed.wake_pending);
    EXPECT_EQ(1U, stop_waits);
    EXPECT_EQ(0U, SCB->SCR);
    EXPECT_EQ(0U, nx_arch_irq_masks().primask);
    EXPECT_EQ(wake_line, EXTI->PR);
    EXPECT_EQ(wake_vector, NVIC->ISPR[1]);
    EXPECT_TRUE(nx_stm32f1_system_live());
    EXPECT_EQ(NX_SUCCESS, nx_clock_domain_quiescent(barrier));
}

TEST_F(F103Stop, RefusesRetainedConsumerAndUnconsumedWake) {
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_reopen(barrier));
    nx_clock_lease_t loan{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_acquire(&domain, &loan));
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_stop_hsi8(barrier, &wake).status);
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_release(&domain, &loan));
    EXTI->PR = wake_line;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_stop_hsi8(barrier, &wake).status);
    EXPECT_EQ(wake_line, EXTI->PR);
    EXPECT_EQ(37U, nx_time_now_us());
    EXPECT_EQ(0U, stop_waits);
}

TEST_F(F103Stop, RefusesActiveDmaAndPreservesRtcAndSystemPendingFlags) {
    RCC->AHBENR = RCC->AHBENR | RCC_AHBENR_DMA1EN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_stop_hsi8(barrier, &wake).status);
    RCC->AHBENR = RCC_AHBENR_SRAMEN | RCC_AHBENR_FLITFEN;
    RTC->CRL = RTC_CRL_ALRF;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_stop_hsi8(barrier, &wake).status);
    EXPECT_EQ(RTC_CRL_ALRF, RTC->CRL);
    RTC->CRL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTSET_Msk;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_stop_hsi8(barrier, &wake).status);
    EXPECT_EQ(SCB_ICSR_PENDSTSET_Msk, SCB->ICSR);
    EXPECT_EQ(0U, stop_waits);
}

TEST_F(F103Stop, LateReadyEdgeSkipsWfiAndPreservesItsPendingWitness) {
    signal_at_arm = true;
    const auto observed = nx_stm32f1_stop_hsi8(barrier, &wake);
    EXPECT_EQ(NX_ERROR_BUSY, observed.status);
    EXPECT_EQ(NX_SUCCESS, observed.cleanup);
    EXPECT_FALSE(observed.wait_issued);
    EXPECT_TRUE(observed.wake_pending);
    EXPECT_EQ(0U, stop_waits);
    EXPECT_EQ(observed.previous_epoch + 1U, observed.current_epoch);
    EXPECT_EQ(wake_line, EXTI->PR);
    EXPECT_EQ(wake_vector, NVIC->ISPR[1]);
    EXPECT_TRUE(nx_stm32f1_system_live());
}

TEST_F(F103Stop, WaitWithoutExpectedEdgeDoesNotReportAWakeup) {
    signal_at_wait = false;
    const auto observed = nx_stm32f1_stop_hsi8(barrier, &wake);
    EXPECT_EQ(NX_SUCCESS, observed.status);
    EXPECT_TRUE(observed.wait_issued);
    EXPECT_FALSE(observed.wake_pending);
    EXPECT_EQ(observed.previous_epoch + 1U, observed.current_epoch);
    EXPECT_TRUE(nx_stm32f1_system_live());
}

TEST_F(F103Stop, FailedHsiRestoreRetainsWakeEvidenceAndClosedDomain) {
    clock_lost = true;
    const auto observed = nx_stm32f1_stop_hsi8(barrier, &wake);
    EXPECT_EQ(NX_ERROR_IO, observed.cleanup);
    EXPECT_EQ(0U, observed.current_epoch);
    EXPECT_FALSE(nx_stm32f1_system_live());
    EXPECT_EQ(NX_SUCCESS, nx_clock_domain_quiescent(barrier));
    EXPECT_EQ(wake_line, EXTI->PR);
    EXPECT_EQ(wake_vector, NVIC->ISPR[1]);
    EXPECT_EQ(0U, NVIC->ISER[1] & wake_vector);
    EXPECT_EQ(0U, nx_arch_irq_masks().primask);
}
#endif
} /* namespace */

#if defined(NEXUS_ARCH_SLEEP_MODEL)
uint32_t nx_arch_sleep_model_control(void) {
    return sleep_control;
}
void nx_arch_sleep_model_wfi(void) {
    EXPECT_EQ(1U, nx_arch_irq_masks().primask);
    const uint32_t memory_clocks = RCC_AHBENR_FLITFEN | RCC_AHBENR_SRAMEN;
    EXPECT_EQ(memory_clocks, RCC->AHBENR & memory_clocks);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_TIM2EN);
    EXPECT_NE(0U, TIM2->CR1 & TIM_CR1_CEN);
    EXPECT_NE(0U, TIM2->DIER & TIM_DIER_UIE);
    ++wfi_count;
    TIM2->CNT = 4U;
    TIM2->SR |= TIM_SR_UIF;
}
#endif

#if defined(TEST_F1_STOP)
void f1_test_stop_wait(void) {
    EXPECT_EQ(1U, nx_arch_irq_masks().primask);
    EXPECT_NE(0U, SCB->SCR & SCB_SCR_SLEEPDEEP_Msk);
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_TIM2EN);
    EXPECT_EQ(0U, NVIC_GetEnableIRQ(TIM2_IRQn));
    EXPECT_NE(0U, RCC->AHBENR & RCC_AHBENR_FLITFEN);
    ++stop_waits;
    if (signal_at_wait) {
        EXTI->PR = wake_line;
        NVIC->ISPR[1] = wake_vector;
    }
    if (clock_lost) {
        RCC->CR = RCC->CR & ~static_cast<uint32_t>(RCC_CR_HSIRDY);
    }
}
#endif

uint16_t f1_test_flash_kib() {
    return flash_kib;
}

void f1_test_write(volatile uint32_t* reg, uint32_t value) {
#if defined(TEST_F1_STANDBY)
    if (f1_test_standby_write(reg, value)) {
        return;
    }
#endif
#if defined(TEST_F1_STOP)
    if (reg == &SCB->SCR && deep_stuck && (value & SCB_SCR_SLEEPDEEP_Msk) == 0U)
        return;
    if (reg == &SCB->SCR && signal_at_arm &&
        (value & SCB_SCR_SLEEPDEEP_Msk) != 0U) {
        EXTI->PR = wake_line;
        NVIC->ISPR[1] = wake_vector;
    }
    if (reg == &NVIC->ICER[1]) {
        NVIC->ISER[1] = NVIC->ISER[1] & ~value;
        return;
    }
#endif
    if (reg == &TIM2->SR) {
        *reg &= value;
        return;
    }
#if defined(TEST_HSE_PLL)
    if (reg == &RCC->CR) {
        uint32_t ready = 0U;
        if ((value & RCC_CR_HSION) != 0U)
            ready |= RCC_CR_HSIRDY;
        if (hse_present && (value & RCC_CR_HSEON) != 0U)
            ready |= RCC_CR_HSERDY;
        uint32_t ready_mask = RCC_CR_HSIRDY | RCC_CR_HSERDY | RCC_CR_PLLRDY;
        bool main_source_ready = (ready & RCC_CR_HSERDY) != 0U;
#if defined(TEST_PLL2)
        ready_mask |= RCC_CR_PLL2RDY;
        if ((value & RCC_CR_PLL2ON) != 0U) {
            EXPECT_EQ(pll2_config, RCC->CFGR2);
            if (main_source_ready && pll2_locks)
                ready |= RCC_CR_PLL2RDY;
        } else if ((*reg & RCC_CR_PLL2ON) != 0U) {
            EXPECT_EQ(0U, value & RCC_CR_PLLON);
            EXPECT_EQ(0U, RCC->CFGR & RCC_CFGR_PLLSRC);
            EXPECT_EQ(RCC_CFGR_SWS_HSI, RCC->CFGR & RCC_CFGR_SWS);
        }
        if (!pll2_stops)
            ready |= *reg & static_cast<uint32_t>(RCC_CR_PLL2RDY);
        main_source_ready = (ready & RCC_CR_PLL2RDY) != 0U && pll_locks;
#endif
        if (main_source_ready && (value & RCC_CR_PLLON) != 0U)
            ready |= RCC_CR_PLLRDY;
        *reg = (value & ~ready_mask) | ready;
        return;
    }
    if (reg == &RCC->CFGR) {
        uint32_t source = *reg & RCC_CFGR_SWS;
        if ((value & RCC_CFGR_SW) == RCC_CFGR_SW_PLL &&
            (RCC->CR & RCC_CR_PLLRDY) != 0U) {
            source = RCC_CFGR_SWS_PLL;
            safe_pll_switch =
                (FLASH->ACR & FLASH_ACR_LATENCY) == flash_latency &&
                (value &
                 (RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 |
                  RCC_CFGR_ADCPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE |
                  RCC_CFGR_PLLMULL | extra_clock_fields)) == clock_fields &&
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
