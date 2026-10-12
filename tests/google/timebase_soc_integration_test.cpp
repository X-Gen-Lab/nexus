/**
 * \file            timebase_soc_integration_test.cpp
 * \brief           Real timebase ISR and stop share optional alarm ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
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
#include "private/system.h"
uint32_t SystemCoreClock = 200000000U;
#define TIMER_CTRL    TIMER_CTL0(TIMER1)
#define TIMER_COUNT   TIMER_CNT(TIMER1)
#define TIMER_FLAGS   TIMER_INTF(TIMER1)
#define TIMER_ENABLE  TIMER_DMAINTEN(TIMER1)
#define TIMER_IRQ     TIMER1_IRQn
#define TIMER_HANDLER TIMER1_IRQHandler
#else
#include "stm32f407_lowpower.h"
#include "stm32f407_system.h"
nx_stm32_system_t g_nx_stm32_system{};
#define TIMER_CTRL    TIM2->CR1
#define TIMER_COUNT   TIM2->CNT
#define TIMER_FLAGS   TIM2->SR
#define TIMER_ENABLE  TIM2->DIER
#define TIMER_IRQ     TIM2_IRQn
#define TIMER_HANDLER TIM2_IRQHandler
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
bool inject_compare;
unsigned ticks;
const nx_freertos_lowpower_port_t* face;

/** \brief Clock SDK/CPU boundaries are modeled, not the timer or binding. */
class TimebaseIntegration : public testing::Test {
    void* mapping = MAP_FAILED;

  protected:
    void SetUp() override {
        mapping = mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_NE(mapping, MAP_FAILED);
        std::memset(mapping, 0, 0x80000U);
        std::memset(&g_timebase_model_systick, 0,
                    sizeof g_timebase_model_systick);
        for (auto& bank : NVIC->ISER)
            bank = 0U;
        SCB->ICSR = 0U;
        inject_compare = false;
        ticks = 0U;
        face = nullptr;
#ifdef NX_TIMEBASE_GD_MODEL
        RCU_CFG0 = RCU_CKSYSSRC_PLLP | RCU_SCSS_PLLP | RCU_APB1_CKAHB_DIV4 |
                   RCU_APB2_CKAHB_DIV2;
        RCU_PLL = 25U | (400U << 6U) | RCU_PLLSRC_HXTAL | (9U << 24U);
        RCU_CTL =
            RCU_CTL_HXTALEN | RCU_CTL_HXTALSTB | RCU_CTL_PLLEN | RCU_CTL_PLLSTB;
        FMC_WS = WS_WSCNT_7;
        SystemCoreClock = 200000000U;
        ASSERT_EQ(nx_gd32_soc_start(), 0);
#else
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        g_nx_stm32_system.flash = FLASH;
        g_nx_stm32_system.power = PWR;
        g_nx_stm32_system.timer = TIM2;
        g_nx_stm32_system.started = true;
        g_nx_stm32_system.core_hz = 168000000U;
        RCC->CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV4 |
                    RCC_CFGR_PPRE2_DIV2;
        RCC->PLLCFGR = 8U | (336U << 6U) | RCC_PLLCFGR_PLLSRC_HSE | (7U << 24U);
        RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        FLASH->ACR = FLASH_ACR_LATENCY_5WS;
        RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
        RCC->APB1LPENR = RCC_APB1LPENR_TIM2LPEN;
        TIM2->PSC = 83U;
        TIM2->ARR = UINT32_MAX;
        TIMER_ENABLE = 1U;
        TIMER_CTRL = 1U;
        nx_timebase_model_enable_irq(TIMER_IRQ);
#endif
        TIMER_COUNT = 1000U;
        TIMER_FLAGS = 0U;
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
                face->resume_tick(face->context,
                                  face->now_us(face->context) + 1000U);
            SysTick_Handler();
        }
        SysTick->CTRL = 0U;
        SCB->ICSR = 0U;
#ifdef NX_TIMEBASE_GD_MODEL
        EXPECT_EQ(nx_gd32_soc_stop(), 0);
#endif
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
    }
    void PauseAndArm() {
        face = nx_freertos_lowpower_port();
        ASSERT_NE(face, nullptr);
        nx_freertos_tick_snapshot_t snapshot{};
        ASSERT_EQ(face->pause_tick(face->context, &snapshot), NX_SUCCESS);
        ASSERT_EQ(face->arm_deadline(face->context,
                                     face->now_us(face->context) + 100U),
                  NX_SUCCESS);
    }
};

TEST_F(TimebaseIntegration, VectorDispatchesCompareAfterMonotonicOverflow) {
    PauseAndArm();
    TIMER_FLAGS = 3U;
    TIMER_HANDLER();
    EXPECT_EQ(TIMER_FLAGS & 3U, 0U);
    EXPECT_EQ(TIMER_ENABLE, 1U);
    EXPECT_EQ(face->now_us(face->context), (UINT64_C(1) << 32U) + 1000U);
    const auto* state = static_cast<nx_timebase_lowpower_t*>(face->context);
    EXPECT_FALSE(state->armed);
    EXPECT_TRUE(state->paused);
}
TEST_F(TimebaseIntegration, FreshCompareBetweenFlagReadAndAckStillWakes) {
    PauseAndArm();
    TIMER_FLAGS = 1U;
    inject_compare = true;
    TIMER_HANDLER();
    const auto* state = static_cast<nx_timebase_lowpower_t*>(face->context);
    EXPECT_FALSE(state->armed);
    EXPECT_EQ(TIMER_ENABLE, 1U);
    EXPECT_EQ(TIMER_FLAGS & 3U, 0U);
}
TEST_F(TimebaseIntegration, ClockStopRejectsOutstandingPauseBeforeAnyRelease) {
    face = nx_freertos_lowpower_port();
    ASSERT_NE(face, nullptr);
    nx_freertos_tick_snapshot_t snapshot{};
    ASSERT_EQ(face->pause_tick(face->context, &snapshot), NX_SUCCESS);
#ifdef NX_TIMEBASE_GD_MODEL
    EXPECT_EQ(nx_gd32_soc_stop(), -1);
#else
    EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 1U), -1);
#endif
    EXPECT_EQ(TIMER_CTRL & 1U, 1U);
    EXPECT_EQ(TIMER_ENABLE, 1U);
    const auto* state = static_cast<nx_timebase_lowpower_t*>(face->context);
    EXPECT_TRUE(state->paused);
}
} /* namespace */

extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    return {1U};
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t) {
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
/** \brief Supply a privileged owner for the private hardware binding. */
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return false;
}
extern "C" void nx_arch_isb(void) {
}
extern "C" void nx_arch_dsb(void) {
#ifdef NX_TIMEBASE_GD_MODEL
    if ((TIMER_SWEVG(TIMER1) & TIMER_SWEVG_UPG) != 0U) {
        TIMER_INTF(TIMER1) |= TIMER_INTF_UPIF;
        TIMER_CNT(TIMER1) = 0U;
        TIMER_SWEVG(TIMER1) = 0U;
    }
#endif
    if ((SysTick->CTRL & 1U) != 0U && SysTick->VAL == 0U)
        SysTick->VAL = SysTick->LOAD;
}
extern "C" uint32_t nx_timebase_model_read(volatile uint32_t* reg) {
    uint32_t value = *reg;
    if (reg == &SysTick->CTRL)
        SysTick->CTRL &= ~(1U << 16U);
    return value;
}
extern "C" void nx_timebase_model_write(volatile uint32_t* reg,
                                        uint32_t value) {
    if (reg == &TIMER_FLAGS)
        TIMER_FLAGS &= value;
    else
        *reg = value;
}
extern "C" void nx_timebase_model_ack(uint32_t value) {
    if (inject_compare) {
        TIMER_FLAGS |= 2U;
        inject_compare = false;
    }
    TIMER_FLAGS &= value;
}
extern "C" void nx_timebase_model_enable_irq(IRQn_Type irq) {
    NVIC->ISER[static_cast<unsigned>(irq) / 32U] |=
        1U << (static_cast<unsigned>(irq) % 32U);
}
extern "C" void nx_timebase_model_disable_irq(IRQn_Type irq) {
    NVIC->ISER[static_cast<unsigned>(irq) / 32U] &=
        ~(1U << (static_cast<unsigned>(irq) % 32U));
}
extern "C" void nx_timebase_model_clear_irq(IRQn_Type irq) {
    NVIC->ISPR[static_cast<unsigned>(irq) / 32U] &=
        ~(1U << (static_cast<unsigned>(irq) % 32U));
}
extern "C" void nx_timebase_model_priority(IRQn_Type, uint32_t) {
}
extern "C" void nx_freertos_kernel_tick_handler(void) {
    ++ticks;
}
extern "C" void nx_freertos_assert_failed(const char*, unsigned);
extern "C" void nx_freertos_assert_failed(const char*, unsigned) {
    throw std::runtime_error("retained integrated timer fault");
}
#ifdef NX_TIMEBASE_GD_MODEL
/** \brief Model the independent clock component's read-only cold admission. */
extern "C" nx_result_t nx_gd32_clock_preflight(void) {
    return NX_SUCCESS;
}
extern "C" int nx_gd32_clock_start(void) {
    return 0;
}
extern "C" int nx_gd32_clock_stop(void) {
    return 0;
}
extern "C" void rcu_periph_clock_enable(rcu_periph_enum peripheral) {
    REG32(RCU + (static_cast<uint32_t>(peripheral) >> 6U)) |=
        1U << (static_cast<uint32_t>(peripheral) & 31U);
}
extern "C" void rcu_periph_clock_disable(rcu_periph_enum peripheral) {
    REG32(RCU + (static_cast<uint32_t>(peripheral) >> 6U)) &=
        ~(1U << (static_cast<uint32_t>(peripheral) & 31U));
}
extern "C" void
rcu_periph_clock_sleep_enable(rcu_periph_sleep_enum peripheral) {
    REG32(RCU + (static_cast<uint32_t>(peripheral) >> 6U)) |=
        1U << (static_cast<uint32_t>(peripheral) & 31U);
}
extern "C" void
rcu_periph_clock_sleep_disable(rcu_periph_sleep_enum peripheral) {
    REG32(RCU + (static_cast<uint32_t>(peripheral) >> 6U)) &=
        ~(1U << (static_cast<uint32_t>(peripheral) & 31U));
}
extern "C" void timer_deinit(uint32_t timer) {
    std::memset(reinterpret_cast<void*>(timer), 0, 0x50U);
}
#endif
