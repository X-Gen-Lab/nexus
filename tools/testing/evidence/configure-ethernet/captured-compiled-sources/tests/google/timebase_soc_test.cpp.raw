/**
 * \file            timebase_soc_test.cpp
 * \brief           Actual reviewed SoC sleep binding and timer channel
 * ownership \author          Nexus Team \version         1.0.0 \date 2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "FreeRTOS.h"
#include "nexus/arch/arch.h"
#include "nexus/core/time.h"
#include "nexus/os/lowpower.h"
#include "timebase_lowpower.h"
#include "timebase_soc_model.h"
#ifdef NX_TIMEBASE_GD_MODEL
#include "gd32f470_lowpower.h"
uint32_t SystemCoreClock = 200000000U;
#define TIMER_CTRL        TIMER_CTL0(TIMER1)
#define TIMER_PRESCALER   TIMER_PSC(TIMER1)
#define TIMER_TOP         TIMER_CAR(TIMER1)
#define TIMER_FLAGS       TIMER_INTF(TIMER1)
#define TIMER_ENABLE      TIMER_DMAINTEN(TIMER1)
#define TIMER_COMPARE     TIMER_CH0CV(TIMER1)
#define TIMER_CLOCK       RCU_APB1EN
#define TIMER_CLOCK_BIT   RCU_APB1EN_TIMER1EN
#define TIMER_SLEEP_CLOCK RCU_APB1SPEN
#define TIMER_SLEEP_BIT   RCU_APB1SPEN_TIMER1SPEN
#define TIMER_IRQ         TIMER1_IRQn
#define LOWPOWER_BUSY     nx_gd32_lowpower_busy
#define LOWPOWER_ALARM    nx_gd32_lowpower_alarm_irq
#else
#include "stm32f407_lowpower.h"
#include "stm32f407_system.h"
nx_stm32_system_t g_nx_stm32_system{};
#define TIMER_CTRL        TIM2->CR1
#define TIMER_PRESCALER   TIM2->PSC
#define TIMER_TOP         TIM2->ARR
#define TIMER_FLAGS       TIM2->SR
#define TIMER_ENABLE      TIM2->DIER
#define TIMER_COMPARE     TIM2->CCR1
#define TIMER_CLOCK       RCC->APB1ENR
#define TIMER_CLOCK_BIT   RCC_APB1ENR_TIM2EN
#define TIMER_SLEEP_CLOCK RCC->APB1LPENR
#define TIMER_SLEEP_BIT   RCC_APB1LPENR_TIM2LPEN
#define TIMER_IRQ         TIM2_IRQn
#define LOWPOWER_BUSY     nx_stm32_lowpower_busy
#define LOWPOWER_ALARM    nx_stm32_lowpower_alarm_irq
#endif
nx_timebase_model_systick_t g_timebase_model_systick{};
NVIC_Type g_timebase_model_nvic{};
SCB_Type g_timebase_model_scb{};
}
#include <cstring>
#include <gtest/gtest.h>
#include <stdexcept>
#include <sys/mman.h>

namespace {
uint64_t now;
unsigned ticks;
const nx_freertos_lowpower_port_t* face;

/** \brief MMIO is real SDK layout; clocks and CPU execution remain a model. */
class TimebaseSoC : public testing::Test {
    void* mapping{};

  protected:
    void SetUp() override {
        mapping = mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_NE(mapping, MAP_FAILED);
        std::memset(mapping, 0, 0x30000U);
        std::memset(&g_timebase_model_systick, 0,
                    sizeof g_timebase_model_systick);
        for (auto& bank : g_timebase_model_nvic.ISER)
            bank = 0U;
        g_timebase_model_scb.ICSR = 0U;
        now = 1000U;
        ticks = 0U;
        face = nullptr;
#ifndef NX_TIMEBASE_GD_MODEL
        g_nx_stm32_system = {};
        g_nx_stm32_system.started = true;
        g_nx_stm32_system.core_hz = 168000000U;
        g_nx_stm32_system.timer = TIM2;
        RCC->CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV4 |
                    RCC_CFGR_PPRE2_DIV2;
        RCC->PLLCFGR = 8U | (336U << 6U) | RCC_PLLCFGR_PLLSRC_HSE | (7U << 24U);
        RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        FLASH->ACR = FLASH_ACR_LATENCY_5WS;
#endif
#ifdef NX_TIMEBASE_GD_MODEL
        RCU_CFG0 = RCU_CKSYSSRC_PLLP | RCU_SCSS_PLLP | RCU_APB1_CKAHB_DIV4 |
                   RCU_APB2_CKAHB_DIV2;
        RCU_PLL = 25U | (400U << 6U) | RCU_PLLSRC_HXTAL | (9U << 24U);
        RCU_CTL =
            RCU_CTL_HXTALEN | RCU_CTL_HXTALSTB | RCU_CTL_PLLEN | RCU_CTL_PLLSTB;
        FMC_WS = WS_WSCNT_7;
        SystemCoreClock = 200000000U;
#endif
        TIMER_CTRL = 1U;
        TIMER_PRESCALER =
#ifdef NX_TIMEBASE_GD_MODEL
            99U;
#else
            83U;
#endif
        TIMER_TOP = UINT32_MAX;
        TIMER_CLOCK |= TIMER_CLOCK_BIT;
        TIMER_SLEEP_CLOCK |= TIMER_SLEEP_BIT;
        TIMER_ENABLE = 1U;
        TIMER_FLAGS = 1U;
        NVIC->ISER[static_cast<unsigned>(TIMER_IRQ) / 32U] =
            1U << (static_cast<unsigned>(TIMER_IRQ) % 32U);
        SysTick->CTRL = 7U;
        SysTick->LOAD = configCPU_CLOCK_HZ / configTICK_RATE_HZ - 1U;
        SysTick->VAL = SysTick->LOAD - 100U;
    }
    void TearDown() override {
        if (face != nullptr) {
            auto* state = static_cast<nx_timebase_lowpower_t*>(face->context);
            if (state->armed)
                face->disarm(face->context);
            if (state->paused)
                face->resume_tick(face->context, now + 1000U);
            SysTick_Handler();
        }
        if (mapping != MAP_FAILED) {
            ASSERT_EQ(munmap(mapping, 0x30000U), 0);
        }
    }
};

TEST_F(TimebaseSoC, ExactReviewedClockAndIRQProduceAStablePort) {
    face = nx_freertos_lowpower_port();
    ASSERT_NE(face, nullptr);
    EXPECT_EQ(nx_freertos_lowpower_port(), face);
    EXPECT_EQ(face->now_us(face->context), now);
    const auto* state = static_cast<nx_timebase_lowpower_t*>(face->context);
    EXPECT_EQ(state->registers->timer_compare, &TIMER_COMPARE);
    EXPECT_EQ(state->registers->compare_flag, 2U);
    EXPECT_EQ(state->registers->cycles_per_us, configCPU_CLOCK_HZ / 1000000U);
}
TEST_F(TimebaseSoC, MissingSleepClockAndDisabledWakeIRQRejectThePort) {
    TIMER_SLEEP_CLOCK &= ~static_cast<uint32_t>(TIMER_SLEEP_BIT);
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
    TIMER_SLEEP_CLOCK |= TIMER_SLEEP_BIT;
    NVIC->ISER[static_cast<unsigned>(TIMER_IRQ) / 32U] = 0U;
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
}
TEST_F(TimebaseSoC, WrongPrescalerAndStoppedTimebaseRejectWithoutRepair) {
    TIMER_PRESCALER = 1U;
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
    EXPECT_EQ(TIMER_PRESCALER, 1U);
    TIMER_CTRL = 0U;
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
}
TEST_F(TimebaseSoC, PLLTupleSourceReadinessAndSpreadSpectrumAreNotCachedFacts) {
#ifdef NX_TIMEBASE_GD_MODEL
    const uint32_t pll = RCU_PLL;
    RCU_PLL ^= 1U;
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
    EXPECT_EQ(RCU_PLL, pll ^ 1U);
    RCU_PLL = pll;
    RCU_CTL &= ~static_cast<uint32_t>(RCU_CTL_PLLSTB);
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
    RCU_CTL |= RCU_CTL_PLLSTB;
    RCU_PLLSSCTL |= RCU_PLLSSCTL_SSCGON;
#else
    const uint32_t pll = RCC->PLLCFGR;
    RCC->PLLCFGR ^= 1U;
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
    EXPECT_EQ(RCC->PLLCFGR, pll ^ 1U);
    RCC->PLLCFGR = pll;
    RCC->CR &= ~static_cast<uint32_t>(RCC_CR_PLLRDY);
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
    RCC->CR |= RCC_CR_PLLRDY;
    RCC->SSCGR |= RCC_SSCGR_SSCGEN;
#endif
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
}
TEST_F(TimebaseSoC,
       CounterDirectionOnePulseAndComparePreloadRejectWithoutRepair) {
#ifdef NX_TIMEBASE_GD_MODEL
    TIMER_CTRL |= TIMER_CTL0_DIR;
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
    TIMER_CTRL = 1U | TIMER_CTL0_SPM;
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
    TIMER_CTRL = 1U;
    TIMER_CHCTL0(TIMER1) = TIMER_CHCTL0_CH0COMSEN;
#else
    TIMER_CTRL |= TIM_CR1_DIR;
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
    TIMER_CTRL = 1U | TIM_CR1_OPM;
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
    TIMER_CTRL = 1U;
    TIM2->CCMR1 = TIM_CCMR1_OC1PE;
#endif
    EXPECT_EQ(nx_freertos_lowpower_port(), nullptr);
}
TEST_F(TimebaseSoC, ReservedCompareKeepsOverflowAndHasExplicitBorrowState) {
    face = nx_freertos_lowpower_port();
    ASSERT_NE(face, nullptr);
    nx_freertos_tick_snapshot_t snapshot{};
    ASSERT_EQ(face->pause_tick(face->context, &snapshot), NX_SUCCESS);
    EXPECT_TRUE(LOWPOWER_BUSY());
    ASSERT_EQ(face->arm_deadline(face->context, now + 100U), NX_SUCCESS);
    EXPECT_EQ(TIMER_COMPARE, static_cast<uint32_t>(now + 100U));
    TIMER_FLAGS = 3U;
    LOWPOWER_ALARM();
    EXPECT_EQ(TIMER_FLAGS, 1U);
    EXPECT_EQ(TIMER_ENABLE, 1U);
    EXPECT_TRUE(LOWPOWER_BUSY());
    face->resume_tick(face->context, now + 1000U);
    EXPECT_FALSE(LOWPOWER_BUSY());
    SysTick_Handler();
    EXPECT_EQ(ticks, 1U);
}
} /* namespace */

extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
extern "C" uint32_t nx_timebase_model_read(volatile uint32_t* reg) {
    uint32_t v = *reg;
    if (reg == &SysTick->CTRL)
        SysTick->CTRL &= ~(1U << 16U);
    return v;
}
extern "C" void nx_timebase_model_write(volatile uint32_t* reg, uint32_t v) {
    if (reg == &TIMER_FLAGS)
        TIMER_FLAGS &= v;
    else
        *reg = v;
}
extern "C" void nx_arch_dsb(void) {
    if ((SysTick->CTRL & 1U) != 0U && SysTick->VAL == 0U)
        SysTick->VAL = SysTick->LOAD;
}
extern "C" void nx_freertos_kernel_tick_handler(void) {
    ++ticks;
}
extern "C" void nx_freertos_assert_failed(const char*, unsigned);
extern "C" void nx_freertos_assert_failed(const char*, unsigned) {
    throw std::runtime_error("retained SoC timer fault");
}
