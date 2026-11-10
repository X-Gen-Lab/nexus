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
#include "stm32f407_lowpower.h"
#include "stm32f407_system.h"
#include "timebase_lowpower.h"
#include "timebase_soc_model.h"
nx_stm32_system_t g_nx_stm32_system{};
#define TIMER_CTRL    TIM2->CR1
#define TIMER_COUNT   TIM2->CNT
#define TIMER_FLAGS   TIM2->SR
#define TIMER_ENABLE  TIM2->DIER
#define TIMER_IRQ     TIM2_IRQn
#define TIMER_HANDLER TIM2_IRQHandler
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
    EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 1U), -1);
    EXPECT_EQ(TIMER_CTRL & 1U, 1U);
    EXPECT_EQ(TIMER_ENABLE, 1U);
    const auto* state = static_cast<nx_timebase_lowpower_t*>(face->context);
    EXPECT_TRUE(state->paused);
}
TEST_F(TimebaseIntegration, ClockStopRetainsTheFirstShortTickUntilItsRealIrq) {
    face = nx_freertos_lowpower_port();
    ASSERT_NE(face, nullptr);
    nx_freertos_tick_snapshot_t snapshot{};
    ASSERT_EQ(NX_SUCCESS, face->pause_tick(face->context, &snapshot));
    face->resume_tick(face->context, face->now_us(face->context) + 100U);
    const auto* state = static_cast<nx_timebase_lowpower_t*>(face->context);
    ASSERT_TRUE(state->first_short);
    EXPECT_EQ(-1, nx_stm32_system_stop(&g_nx_stm32_system, 1U));
    EXPECT_EQ(TIM_CR1_CEN, TIMER_CTRL);
    SysTick_Handler();
    EXPECT_FALSE(state->first_short);
    EXPECT_FALSE(nx_stm32_lowpower_busy());
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
/** \brief This continuous-timer fixture models all incoming masks as clear. */
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return {0U, 0U, 0U};
}
extern "C" void nx_arch_isb(void) {
}
extern "C" void nx_arch_dsb(void) {
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
