/**
 * \file            pulse_test.cpp
 * \brief           Check hardware one-pulse admission, completion and stop.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/timer_pulse/instance.h"
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t peripheral = 0x40000000U;
constexpr size_t peripheral_size = 0x24000U;
constexpr uint32_t pad_mask = 0xFU << 24U;
constexpr uint32_t pad_idle = 2U << 24U;
constexpr uint32_t pad_alt = 0xAU << 24U;
constexpr uint32_t pin = 1U << 6U;
constexpr uint32_t pulse_cr1 = TIM_CR1_OPM | TIM_CR1_URS;
struct Vector {
    uint32_t enabled;
    uint32_t pending;
    uint32_t priority;
};
Vector vector;
bool block_pad;
bool fail_irq_enable;
bool fail_irq_clear;
bool system_live;
nx_time_us_t now;

bool pin_high() {
    if ((GPIOA->CRL & pad_mask) == pad_idle)
        return (GPIOA->ODR & pin) != 0U;
    return (GPIOA->CRL & pad_mask) == pad_alt &&
           (TIM3->CCER & TIM_CCER_CC1E) != 0U &&
           TIM3->CCMR1 == TIM_CCMR1_OC1M && TIM3->CNT >= TIM3->CCR1;
}

class F1Tim3Pulse : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    nx_stm32f1_tim3_pulse_t state{};
    const nx_timer_pulse_t port{&nx_stm32f1_tim3_pulse_ops, &state};
    nx_timer_pulse_request_t request{};
    uint32_t width = 250U;

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(peripheral), peripheral_size,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(peripheral));
        vector = {0U, 0U, 9U};
        nx_request_initialize(&request.base);
        RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
        GPIOA->CRL = 0x44444444U;
        GPIOA->ODR = pin;
        system_live = true;
        now = 10U;
        block_pad = fail_irq_enable = fail_irq_clear = false;
    }

    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, peripheral_size), 0);
        }
    }

    void prepare(uint32_t period = 1000U) {
        ASSERT_EQ(nx_stm32f1_tim3_pulse_prepare(&state, 1000000U, period, 5U),
                  NX_SUCCESS);
        EXPECT_EQ(TIM3->PSC, 7U);
        EXPECT_EQ(TIM3->ARR, period - 1U);
        EXPECT_EQ(TIM3->CR1, pulse_cr1);
        EXPECT_EQ(vector.enabled, 0U);
        EXPECT_EQ(vector.priority, 5U);
        EXPECT_FALSE(pin_high());
    }

    void prime(uint32_t ticks = 250U, nx_time_us_t deadline = 100000U) {
        width = ticks;
        request.width_ticks = &width;
        request.channel_count = 1U;
        ASSERT_EQ(nx_request_prepare(&request.base, deadline), NX_SUCCESS);
    }

    void advance(uint32_t ticks) {
        while (ticks-- != 0U && (TIM3->CR1 & TIM_CR1_CEN) != 0U) {
            ++now;
            if (TIM3->CNT == TIM3->ARR) {
                TIM3->CNT = 0U;
                if ((TIM3->CR1 & TIM_CR1_OPM) != 0U)
                    TIM3->CR1 &= ~static_cast<uint32_t>(TIM_CR1_CEN);
                TIM3->SR |= TIM_SR_UIF;
                if ((TIM3->DIER & TIM_DIER_UIE) != 0U)
                    vector.pending = 1U;
            } else {
                ++TIM3->CNT;
            }
        }
    }

    void result(nx_result_t expected, size_t periods) {
        nx_result_t observed = NX_ERROR_STATE;
        size_t completed = 99U;
        ASSERT_EQ(nx_request_result(&request.base, &observed, &completed),
                  NX_SUCCESS);
        EXPECT_EQ(observed, expected);
        EXPECT_EQ(completed, periods);
    }
};

TEST_F(F1Tim3Pulse,
       RetainsOneHardwareUpdateAndWithdrawsVectorBeforeSettlement) {
    prepare();
    prime();
    ASSERT_EQ(nx_timer_pulse_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(TIM3->CCR1, 750U);
    ASSERT_EQ(vector.enabled, 1U);
    EXPECT_EQ(nx_timer_pulse_port_service(&port), NX_ERROR_BUSY);
    advance(749U);
    EXPECT_FALSE(pin_high());
    advance(1U);
    EXPECT_TRUE(pin_high());
    advance(249U);
    EXPECT_TRUE(pin_high());
    advance(1U);
    EXPECT_FALSE(pin_high());
    EXPECT_EQ(TIM3->CR1, pulse_cr1);
    EXPECT_EQ(TIM3->CNT, 0U);
    ASSERT_EQ(vector.pending, 1U);
    vector.pending = 0U;
    nx_stm32f1_tim3_pulse_irq();
    EXPECT_EQ(vector.enabled, 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_NE(TIM3->SR & TIM_SR_UIF, 0U);
    EXPECT_EQ(nx_timer_pulse_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(TIM3->SR | TIM3->DIER | TIM3->CCER, 0U);
    EXPECT_EQ(GPIOA->CRL & pad_mask, pad_idle);
    result(NX_SUCCESS, 1U);
    EXPECT_EQ(nx_stm32f1_tim3_pulse_close(&state), NX_SUCCESS);
    EXPECT_EQ(RCC->APB1ENR, RCC_APB1ENR_TIM2EN);
    EXPECT_EQ(GPIOA->CRL, 0x44444444U);
    EXPECT_EQ(GPIOA->ODR, pin);
    EXPECT_EQ(vector.priority, 9U);
}

TEST_F(F1Tim3Pulse, RangeRejectionKeepsReadyLoanAndZeroWidthFitsMaxPeriod) {
    prepare(65536U);
    prime(65536U);
    EXPECT_EQ(nx_timer_pulse_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(TIM3->CR1, pulse_cr1);
    width = 0U;
    ASSERT_EQ(nx_timer_pulse_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(TIM3->CCMR1, TIM_CCMR1_OC1M_2);
    advance(65535U);
    EXPECT_FALSE(pin_high());
    advance(1U);
    EXPECT_FALSE(pin_high());
    EXPECT_EQ(nx_timer_pulse_port_service(&port), NX_SUCCESS);
    result(NX_SUCCESS, 1U);
}

TEST_F(F1Tim3Pulse, FailedMidPulseStopRetainsBorrowUntilPadAndIrqWithdraw) {
    prepare();
    prime();
    ASSERT_EQ(nx_timer_pulse_port_submit(&port, &request), NX_SUCCESS);
    advance(750U);
    EXPECT_TRUE(pin_high());
    block_pad = true;
    EXPECT_EQ(nx_timer_pulse_port_stop(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_NE(TIM3->CR1 & TIM_CR1_CEN, 0U);
    nx_timer_pulse_state_t observed{};
    ASSERT_EQ(nx_timer_pulse_port_state(&port, &observed), NX_SUCCESS);
    EXPECT_TRUE(observed.quarantined);
    EXPECT_EQ(nx_stm32f1_tim3_pulse_close(&state), NX_ERROR_IO);
    EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_TIM3EN, 0U);
    block_pad = false;
    EXPECT_EQ(nx_timer_pulse_port_stop(&port), NX_SUCCESS);
    EXPECT_FALSE(pin_high());
    EXPECT_EQ(TIM3->DIER | TIM3->CCER | vector.enabled, 0U);
    result(NX_ERROR_CANCELLED, 0U);
}

TEST_F(F1Tim3Pulse, PendingVectorBlocksSettlementEvenAfterHardwareFinishes) {
    prepare();
    prime();
    ASSERT_EQ(nx_timer_pulse_port_submit(&port, &request), NX_SUCCESS);
    advance(1000U);
    ASSERT_EQ(vector.pending, 1U);
    fail_irq_clear = true;
    EXPECT_EQ(nx_timer_pulse_port_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(TIM3->SR, 0U);
    EXPECT_EQ(vector.enabled, 0U);
    EXPECT_EQ(vector.pending, 1U);
    fail_irq_clear = false;
    EXPECT_EQ(nx_timer_pulse_port_service(&port), NX_ERROR_IO);
    EXPECT_EQ(vector.pending, 0U);
    result(NX_ERROR_IO, 0U);
}

TEST_F(F1Tim3Pulse, CompletionFirstObservedAtDeadlineIsTimeout) {
    prepare();
    prime(250U, now + 1000U);
    ASSERT_EQ(nx_timer_pulse_port_submit(&port, &request), NX_SUCCESS);
    advance(1000U);
    EXPECT_EQ(nx_timer_pulse_port_service(&port), NX_ERROR_TIMEOUT);
    result(NX_ERROR_TIMEOUT, 0U);
    EXPECT_FALSE(pin_high());
    EXPECT_EQ(vector.enabled | vector.pending, 0U);
}

TEST_F(F1Tim3Pulse, ClockLossParksPadAndQuarantinesUntilSafeRetry) {
    prepare();
    prime();
    ASSERT_EQ(nx_timer_pulse_port_submit(&port, &request), NX_SUCCESS);
    advance(750U);
    ASSERT_TRUE(pin_high());
    system_live = false;
    EXPECT_EQ(nx_timer_pulse_port_service(&port), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_FALSE(pin_high());
    system_live = true;
    EXPECT_EQ(nx_timer_pulse_port_stop(&port), NX_SUCCESS);
    result(NX_ERROR_STATE, 0U);
}

TEST_F(F1Tim3Pulse, StartFailureAfterAdmissionRetiresWithoutCreditingPulse) {
    prepare();
    prime();
    fail_irq_enable = true;
    EXPECT_EQ(nx_timer_pulse_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(nx_timer_pulse_port_service(&port), NX_ERROR_IO);
    result(NX_ERROR_IO, 0U);
    EXPECT_FALSE(pin_high());
    EXPECT_EQ(TIM3->CR1, pulse_cr1);
}

TEST_F(F1Tim3Pulse, ForeignRemapAndPendingVectorAreNeverAcquired) {
    AFIO->MAPR = AFIO_MAPR_TIM3_REMAP;
    EXPECT_EQ(nx_stm32f1_tim3_pulse_prepare(&state, 1000000U, 1000U, 5U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    EXPECT_EQ(GPIOA->CRL, 0x44444444U);
    AFIO->MAPR = 0U;
    vector.pending = 1U;
    EXPECT_EQ(nx_stm32f1_tim3_pulse_prepare(&state, 1000000U, 1000U, 5U),
              NX_ERROR_BUSY);
    EXPECT_EQ(vector.pending, 1U);
    EXPECT_EQ(vector.priority, 9U);
    EXPECT_EQ(RCC->APB1ENR, RCC_APB1ENR_TIM2EN);
}
} /* namespace */

extern "C" {
bool nx_stm32f1_system_live(void) {
    return system_live;
}

nx_time_us_t nx_time_now_us(void) {
    return now;
}

void nx_f1_pulse_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &RCC->APB1RSTR && (value & RCC_APB1RSTR_TIM3RST) != 0U) {
        *TIM3 = TIM_TypeDef{};
        TIM3->ARR = UINT16_MAX;
    }
    if (reg == &TIM3->EGR && value == TIM_EGR_UG) {
        EXPECT_EQ(TIM3->CR1, pulse_cr1);
        EXPECT_EQ(TIM3->CCER | TIM3->DIER, 0U);
        TIM3->CNT = 0U;
    }
    if (reg == &GPIOA->CRL && (value & pad_mask) == pad_idle && block_pad)
        return;
    if (reg == &TIM3->CR1 && (value & TIM_CR1_CEN) != 0U) {
        EXPECT_EQ(TIM3->CNT, 0U);
        EXPECT_NE(TIM3->CCR1, 0U);
        EXPECT_FALSE(pin_high());
    }
    *reg = value;
}

void nx_f1_pulse_bsrr(GPIO_TypeDef* gpio, uint32_t value) {
    gpio->ODR = (gpio->ODR | (value & UINT16_MAX)) & ~(value >> 16U);
}

uint32_t nx_f1_pulse_get_enable(IRQn_Type irq) {
    EXPECT_EQ(irq, TIM3_IRQn);
    return vector.enabled;
}
uint32_t nx_f1_pulse_get_active(IRQn_Type irq) {
    EXPECT_EQ(irq, TIM3_IRQn);
    return 0U;
}
uint32_t nx_f1_pulse_get_pending(IRQn_Type irq) {
    EXPECT_EQ(irq, TIM3_IRQn);
    return vector.pending;
}
uint32_t nx_f1_pulse_get_priority(IRQn_Type irq) {
    EXPECT_EQ(irq, TIM3_IRQn);
    return vector.priority;
}
void nx_f1_pulse_enable(IRQn_Type irq) {
    EXPECT_EQ(irq, TIM3_IRQn);
    if (!fail_irq_enable)
        vector.enabled = 1U;
}
void nx_f1_pulse_disable(IRQn_Type irq) {
    EXPECT_EQ(irq, TIM3_IRQn);
    vector.enabled = 0U;
}
void nx_f1_pulse_clear(IRQn_Type irq) {
    EXPECT_EQ(irq, TIM3_IRQn);
    if (!fail_irq_clear)
        vector.pending = 0U;
}
void nx_f1_pulse_set_priority(IRQn_Type irq, uint32_t priority) {
    EXPECT_EQ(irq, TIM3_IRQn);
    vector.priority = priority;
}
}
