/**
 * \file            timer_clock_test.cpp
 * \brief           Real timer owners consume the generated static clock plan
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_clock.h"
#include "stm32f407_timer_advanced.h"
#include "stm32f4_encoder.h"
nx_stm32_system_t g_nx_stm32_system;
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

/** \brief Model only ordered hardware stores and update/W0C side effects. */
extern "C" void nx_timer_clock_write(volatile uint32_t* address,
                                     uint32_t value) {
    for (TIM_TypeDef* timer : {TIM1, TIM3, TIM4, TIM5, TIM8, TIM9, TIM10, TIM11,
                               TIM12, TIM13, TIM14}) {
        if (address == &timer->SR) {
            *address &= value;
            return;
        }
        if (address == &timer->EGR && (value & TIM_EGR_UG) != 0U &&
            (timer->CR1 & TIM_CR1_UDIS) == 0U)
            timer->SR |= TIM_SR_UIF;
    }
    *address = value;
}
/** \brief No vector is adopted or simulated as a host memory address. */
extern "C" uint32_t nx_timer_clock_vector(IRQn_Type) {
    return 0U;
}
/** \brief Tests own the privileged serialized Thread hardware boundary. */
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
/** \brief No asynchronous test callback runs in an exception. */
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
/** \brief Incoming mask is explicitly clear in this owner context. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return false;
}
/** \brief Preserve the model's always-clear incoming mask. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    return nx_arch_irq_state_t{0U};
}
/** \brief No second host thread shares these device registers. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t) {
}
/** \brief Host stores are observed immediately; no physical timing claim. */
extern "C" void nx_arch_dsb(void) {
}
/** \brief Timer counter values remain distinct from this Core observation. */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return 1U;
}

namespace {
class TimerClock : public testing::Test {
  protected:
    void* mapped = MAP_FAILED;
    void SetUp() override {
        mapped = mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapped, reinterpret_cast<void*>(0x40000000U));
        std::memset(mapped, 0, 0x80000U);
        RCC->AHB1ENR =
            RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
        RCC->CFGR = NEXUS_CLOCK_CFGR;
        GPIOA->MODER = (2U << 0U) | (2U << 12U) | (2U << 14U) | (2U << 16U);
        GPIOA->AFR[0] = (2U << 0U) | (3U << 28U) | (2U << 24U);
        GPIOA->AFR[1] = 1U;
        GPIOB->MODER = (2U << 12U) | (2U << 24U) | (2U << 26U);
        GPIOB->AFR[0] = 2U << 24U;
        GPIOB->AFR[1] = 17U << 16U;
        GPIOC->MODER = 2U << 12U;
        GPIOC->AFR[0] = 3U << 24U;
    }
    void TearDown() override {
        if (mapped != MAP_FAILED) {
            EXPECT_EQ(0, munmap(mapped, 0x80000U));
        }
    }
};

const struct Capture {
    TIM_TypeDef* timer;
    nx_result_t (*initialize)(nx_timer_capture_controller_t*,
                              const nx_timer_capture_config_t*);
    nx_result_t (*release)(nx_timer_capture_controller_t*);
    uint32_t gate;
    uint32_t frequency = NEXUS_APB1_TIMER_HZ;
    bool apb2 = false;
    GPIO_TypeDef* bank = nullptr;
    uint8_t pin = 0U;
    uint8_t af = 0U;
} captures[]{{TIM3, nx_stm32_tim3_timer_capture_initialize,
              nx_stm32_tim3_timer_capture_release, RCC_APB1ENR_TIM3EN,
              NEXUS_APB1_TIMER_HZ, false, GPIOA, 6U, 2U},
             {TIM4, nx_stm32_timer_capture_initialize,
              nx_stm32_timer_capture_release, RCC_APB1ENR_TIM4EN,
              NEXUS_APB1_TIMER_HZ, false, GPIOB, 6U, 2U},
             {TIM5, nx_stm32_tim5_timer_capture_initialize,
              nx_stm32_tim5_timer_capture_release, RCC_APB1ENR_TIM5EN,
              NEXUS_APB1_TIMER_HZ, false, GPIOA, 0U, 2U},
             {TIM1, nx_stm32_tim1_timer_capture_initialize,
              nx_stm32_tim1_timer_capture_release, RCC_APB2ENR_TIM1EN,
              NEXUS_APB2_TIMER_HZ, true, GPIOA, 8U, 1U},
             {TIM8, nx_stm32_tim8_timer_capture_initialize,
              nx_stm32_tim8_timer_capture_release, RCC_APB2ENR_TIM8EN,
              NEXUS_APB2_TIMER_HZ, true, GPIOC, 6U, 3U},
             {TIM9, nx_stm32_tim9_timer_capture_initialize,
              nx_stm32_tim9_timer_capture_release, RCC_APB2ENR_TIM9EN,
              NEXUS_APB2_TIMER_HZ, true, GPIOA, 2U, 3U},
             {TIM10, nx_stm32_tim10_timer_capture_initialize,
              nx_stm32_tim10_timer_capture_release, RCC_APB2ENR_TIM10EN,
              NEXUS_APB2_TIMER_HZ, true, GPIOB, 8U, 3U},
             {TIM11, nx_stm32_tim11_timer_capture_initialize,
              nx_stm32_tim11_timer_capture_release, RCC_APB2ENR_TIM11EN,
              NEXUS_APB2_TIMER_HZ, true, GPIOB, 9U, 3U},
             {TIM12, nx_stm32_tim12_timer_capture_initialize,
              nx_stm32_tim12_timer_capture_release, RCC_APB1ENR_TIM12EN,
              NEXUS_APB1_TIMER_HZ, false, GPIOB, 14U, 9U},
             {TIM13, nx_stm32_tim13_timer_capture_initialize,
              nx_stm32_tim13_timer_capture_release, RCC_APB1ENR_TIM13EN,
              NEXUS_APB1_TIMER_HZ, false, GPIOA, 6U, 9U},
             {TIM14, nx_stm32_tim14_timer_capture_initialize,
              nx_stm32_tim14_timer_capture_release, RCC_APB1ENR_TIM14EN,
              NEXUS_APB1_TIMER_HZ, false, GPIOA, 7U, 9U}};

const struct Advanced {
    TIM_TypeDef* timer;
    nx_result_t (*initialize)(nx_advanced_pwm_controller_t*,
                              const nx_advanced_pwm_config_t*);
    nx_result_t (*release)(nx_advanced_pwm_controller_t*);
    const nx_advanced_pwm_ops_t* ops;
    volatile uint32_t* input;
    uint32_t gate;
    uint32_t mask;
} advanced[]{
    {TIM1, nx_stm32_advanced_pwm_initialize, nx_stm32_advanced_pwm_release,
     &nx_stm32_advanced_pwm_ops, &GPIOB->IDR, RCC_APB2ENR_TIM1EN, 4096U},
    {TIM8, nx_stm32_tim8_advanced_pwm_initialize,
     nx_stm32_tim8_advanced_pwm_release, &nx_stm32_tim8_advanced_pwm_ops,
     &GPIOA->IDR, RCC_APB2ENR_TIM8EN, 64U}};
} /* namespace */

TEST_F(TimerClock,
       CaptureUsesActualSelectedTimerClockAndRejectsChangedDivider) {
    for (const auto& item : captures) {
        SCOPED_TRACE(item.timer);
        if (item.bank != nullptr) {
            const uint32_t shift = (item.pin % 8U) * 4U;
            item.bank->MODER = (item.bank->MODER & ~(3U << (item.pin * 2U))) |
                               (2U << (item.pin * 2U));
            item.bank->AFR[item.pin / 8U] =
                (item.bank->AFR[item.pin / 8U] & ~(15U << shift)) |
                ((uint32_t)item.af << shift);
        }
        if (item.apb2)
            RCC->APB2ENR = item.gate;
        else
            RCC->APB1ENR = item.gate;
        nx_timer_capture_controller_t state{};
        const nx_timer_capture_config_t config{
            item.frequency, 1000000U, NX_TIMER_EDGE_RISING, 0U, true, 1U};
        const nx_result_t initialized = item.initialize(&state, &config);
        EXPECT_EQ(NX_SUCCESS, initialized);
        if (initialized != NX_SUCCESS)
            continue;
        EXPECT_EQ(item.frequency / 1000000U - 1U, item.timer->PSC);
        EXPECT_EQ(NX_SUCCESS, item.release(&state));
        const uint32_t saved = RCC->CFGR;
        RCC->CFGR ^= item.apb2 ? RCC_CFGR_PPRE2_2 : RCC_CFGR_PPRE1_2;
        EXPECT_EQ(NX_ERROR_STATE, item.initialize(&state, &config));
        EXPECT_EQ(nullptr, state.config);
        RCC->CFGR = saved;
    }
}

TEST_F(TimerClock, ComplementaryPwmUsesSelectedPrescalerAndDeadtimeFrequency) {
    for (const auto& item : advanced) {
        SCOPED_TRACE(item.timer);
        if (item.timer == TIM8)
            GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(15U << 24U)) | (3U << 24U);
        RCC->APB2ENR = item.gate;
        nx_advanced_pwm_controller_t state{};
        const nx_advanced_pwm_config_t config{
            item.input, item.mask, NEXUS_APB2_TIMER_HZ,
            1000000U,   1000U,     128U,
            true,       false,     false,
            true,       1U};
        const nx_result_t initialized = item.initialize(&state, &config);
        EXPECT_EQ(NX_SUCCESS, initialized);
        if (initialized != NX_SUCCESS)
            continue;
        EXPECT_EQ(NEXUS_APB2_TIMER_HZ / 1000000U - 1U, item.timer->PSC);
        nx_advanced_pwm_state_t info{};
        ASSERT_EQ(NX_SUCCESS, item.ops->state(&state, &info));
        EXPECT_EQ(NEXUS_APB2_TIMER_HZ, info.deadtime_hz);
        EXPECT_EQ(128U, info.deadtime_ticks);
        EXPECT_EQ(NX_SUCCESS, item.release(&state));
        const uint32_t saved = RCC->CFGR;
        RCC->CFGR ^= RCC_CFGR_PPRE2_2;
        EXPECT_EQ(NX_ERROR_STATE, item.initialize(&state, &config));
        EXPECT_EQ(nullptr, state.config);
        RCC->CFGR = saved;
    }
}

TEST_F(TimerClock, EncoderDescribesActualFilterClockOnlyWhileGateRemainsLive) {
    RCC->APB1ENR = 0U;
    nx_stm32_encoder_state_t state{};
    const nx_stm32_encoder_config_t config{TIM3,
                                           NX_ENCODER_MODE_X4,
                                           3U,
                                           7U,
                                           false,
                                           false,
                                           {{GPIOA, 0U, 6U}, {GPIOA, 0U, 7U}}};
    ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_prepare(&state, &config));
    nx_encoder_state_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_ops.state(&state, &info));
    EXPECT_EQ(NEXUS_APB1_TIMER_HZ, info.filter_clock_hz);
    RCC->APB1ENR &= ~(uint32_t)RCC_APB1ENR_TIM3EN;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_encoder_ops.state(&state, &info));
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
}
