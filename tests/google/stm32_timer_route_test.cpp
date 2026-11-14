/**
 * \file            stm32_timer_route_test.cpp
 * \brief           Alternate capture and complementary routes run real C paths
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-14
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_timer_cold.h"
nx_stm32_system_t g_nx_stm32_system{};
NVIC_Type g_nx_timer_route_nvic{};
SCB_Type g_nx_timer_route_scb{};
}
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
bool in_irq;
nx_arch_irq_state_t mask{};
} /* namespace */

extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return in_irq;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return mask.value != 0U;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous = mask;
    mask.value = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    mask = previous;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return 1000U;
}
extern "C" void nx_timer_route_write(volatile uint32_t* reg, uint32_t value) {
    for (unsigned bank = 0U; bank < 3U; ++bank) {
        if (reg == &NVIC->ISER[bank]) {
            NVIC->ISER[bank] |= value;
            return;
        }
        if (reg == &NVIC->ICER[bank]) {
            NVIC->ISER[bank] &= ~value;
            return;
        }
        if (reg == &NVIC->ICPR[bank]) {
            NVIC->ISPR[bank] &= ~value;
            return;
        }
    }
    if (reg == &TIM4->SR || reg == &TIM1->SR)
        *reg &= value;
    else
        *reg = value;
    for (TIM_TypeDef* timer : {TIM4, TIM1})
        if (reg == &timer->EGR && (value & TIM_EGR_UG) != 0U &&
            (timer->CR1 & TIM_CR1_UDIS) == 0U)
            timer->SR |= TIM_SR_UIF;
}
extern "C" uint32_t nx_timer_route_read(volatile uint32_t* reg) {
    uint32_t value = *reg;
    if (reg == &TIM4->CCR1)
        TIM4->SR &= ~UINT32_C(2);
    return value;
}

/** \brief Each fixture retains all cold descriptors through successful close.
 */
class TimerRoute : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    nx_timer_capture_controller_t capture{};
    nx_advanced_pwm_controller_t advanced{};
    nx_stm32_timer_cold_t capture_cold{};
    nx_stm32_timer_cold_t advanced_cold{};
    uint16_t saved_capture{};
    uint16_t saved_advanced[3]{};
    const nx_stm32_capture_cold_config_t capture_config{
        {84000000U, 1000000U, NX_TIMER_EDGE_RISING, 0U, true, 1U},
        &saved_capture,
        5U,
        false,
        TIM4};
    const nx_stm32_advanced_cold_config_t advanced_config{
        {&GPIOE->IDR, 32768U, 168000000U, 1000000U, 1000U, 128U, true, false,
         false, true, 1U},
        saved_advanced,
        5U,
        false};
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(MAP_FAILED, mapping);
        g_nx_timer_route_nvic = {};
        g_nx_timer_route_scb.AIRCR = 0U;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        in_irq = false;
        mask = {};
        RCC->CFGR = RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
        RCC->AHB1ENR = RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN;
        RCC->APB1ENR = RCC_APB1ENR_TIM7EN;
    }
    void TearDown() override {
        in_irq = false;
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(NX_SUCCESS,
                      nx_stm32_capture_cold_close(&capture, &capture_cold,
                                                  &capture_config));
            EXPECT_EQ(NX_SUCCESS,
                      nx_stm32_advanced_cold_close(&advanced, &advanced_cold,
                                                   &advanced_config));
            EXPECT_EQ(0, munmap(mapping, 0x80000U));
        }
    }
};

TEST_F(TimerRoute, AlternateCaptureSharesItsColdAndHotPadConnection) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_capture_cold_prepare(&capture, &capture_cold,
                                                        &capture_config));
    EXPECT_EQ(2U, (GPIOD->MODER >> 24U) & 3U);
    EXPECT_EQ(0U, GPIOB->MODER);
    const nx_timer_capture_t port{&nx_stm32_timer_capture_ops, &capture};
    nx_timer_capture_event_t ring[2]{};
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_start(&port, ring, 2U));
    TIM4->CCR1 = 71U;
    TIM4->SR |= 2U;
    in_irq = true;
    nx_stm32_timer_capture_irq(&capture);
    in_irq = false;
    nx_timer_capture_event_t event{};
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_read(&port, &event));
    EXPECT_EQ(71U, event.captured_ticks);
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_stop(&port));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_capture_cold_close(&capture, &capture_cold,
                                                      &capture_config));
    EXPECT_EQ(0U, GPIOD->MODER);
    EXPECT_EQ(RCC_APB1ENR_TIM7EN, RCC->APB1ENR);
}

TEST_F(TimerRoute, AlternateBreakAndOutputsUseOneSelectedComplementaryRoute) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_advanced_cold_prepare(
                              &advanced, &advanced_cold, &advanced_config));
    for (unsigned pin : {8U, 9U, 15U})
        EXPECT_EQ(2U, (GPIOE->MODER >> (pin * 2U)) & 3U);
    const nx_advanced_pwm_t port{&nx_stm32_advanced_pwm_ops, &advanced};
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_set_duty(&port, 100U));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_start(&port));
    EXPECT_NE(0U, TIM1->BDTR & TIM_BDTR_MOE);
    GPIOE->IDR = 32768U;
    EXPECT_EQ(NX_ERROR_STATE, nx_advanced_pwm_start(&port));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_stop(&port));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_advanced_cold_close(
                              &advanced, &advanced_cold, &advanced_config));
    EXPECT_EQ(0U, GPIOE->MODER);
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOB->MODER);
}
