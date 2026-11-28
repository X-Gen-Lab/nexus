/**
 * \file            capture_test.cpp
 * \brief           TIM4 capture, collision witness and ring detachment.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/timer_capture/instance.h"
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t peripheral = 0x40000000U;
constexpr size_t peripheral_size = 0x24000U;
constexpr uint32_t line = 1U << 6U;
constexpr uint32_t peer = 1U << 7U;
constexpr uint32_t route = 0x0234U;
constexpr uint32_t channel_if = TIM_SR_CC1IF;
constexpr uint32_t channel_of = TIM_SR_CC1OF;
struct Vector {
    uint32_t enabled;
    uint32_t pending;
    uint32_t priority;
};
Vector timer;
Vector witness;
bool collide;
bool edge_during_ack;
bool block_disable;
bool block_enable;
bool block_dier;
bool block_witness_ack;
unsigned ccr_reads;
nx_time_us_t now;

Vector& vector(IRQn_Type irq) {
    EXPECT_TRUE(irq == TIM4_IRQn || irq == EXTI9_5_IRQn);
    return irq == TIM4_IRQn ? timer : witness;
}

class F1Tim4Capture : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    nx_stm32f1_tim4_capture_t state{};
    nx_timer_capture_event_t ring[2]{};
    const nx_timer_capture_t port{&nx_stm32f1_tim4_capture_ops, &state};

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(peripheral), peripheral_size,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(peripheral));
        timer = {0U, 0U, 9U};
        witness = {};
        collide = edge_during_ack = block_enable = block_disable = false;
        block_dier = block_witness_ack = false;
        ccr_reads = 0U;
        now = 10U;
        GPIOB->CRL = 0x88888888U;
        AFIO->EXTICR[1] = route;
    }

    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, peripheral_size), 0);
        }
    }

    void prepare() {
        ASSERT_EQ(nx_stm32f1_tim4_capture_prepare(&state, 1000000U, 5U),
                  NX_SUCCESS);
        ASSERT_EQ(timer.priority, 5U);
        ASSERT_EQ(TIM4->PSC, 7U);
        ASSERT_EQ(TIM4->ARR, UINT16_MAX);
        ASSERT_EQ(TIM4->DIER, 0U);
        ASSERT_EQ(EXTI->IMR, 0U);
        ASSERT_EQ(witness.enabled, 0U);
    }

    void start(size_t capacity) {
        prepare();
        ASSERT_EQ(nx_timer_capture_start(&port, ring, capacity), NX_SUCCESS);
        ASSERT_EQ(TIM4->DIER, TIM_DIER_CC1IE);
        ASSERT_EQ(EXTI->IMR, line);
        ASSERT_EQ(witness.enabled, 0U);
        ASSERT_EQ(timer.enabled, 1U);
    }

    void edge(uint32_t ticks, bool dispatch = true) {
        TIM4->CCR1 = ticks;
        TIM4->SR |= channel_if;
        EXTI->PR |= line;
        witness.pending = 1U;
        timer.pending = 1U;
        if (dispatch) {
            timer.pending = 0U;
            nx_stm32f1_tim4_capture_irq(&state);
        }
    }
};

TEST_F(F1Tim4Capture, KeepsRawWrappedCounterAndIndependentObservation) {
    EXTI->PR = peer;
    start(2U);
    edge(UINT16_MAX);
    now = 91U;
    TIM4->SR |= TIM_SR_UIF;
    edge(1U);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&port, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, UINT16_MAX);
    EXPECT_EQ(event.observed_at, 10U);
    EXPECT_EQ(event.flags, 0U);
    ASSERT_EQ(nx_timer_capture_read(&port, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 1U);
    EXPECT_EQ(event.observed_at, 91U);
    EXPECT_EQ(event.flags, 0U);
    EXPECT_EQ(TIM4->SR, TIM_SR_UIF);
    ASSERT_EQ(nx_timer_capture_stop(&port), NX_SUCCESS);
    const auto reads = ccr_reads;
    nx_stm32f1_tim4_capture_irq(&state);
    EXPECT_EQ(ccr_reads, reads);
    ASSERT_EQ(nx_stm32f1_tim4_capture_close(&state), NX_SUCCESS);
    EXPECT_EQ(RCC->APB1ENR, 0U);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    EXPECT_EQ(timer.priority, 9U);
    EXPECT_EQ(GPIOB->CRL, 0x88888888U);
    EXPECT_EQ(AFIO->EXTICR[1], route);
    EXPECT_EQ(EXTI->PR, peer);
}

TEST_F(F1Tim4Capture, UsesSamePinWitnessWhenSiliconLosesCaptureFlag) {
    start(2U);
    collide = true;
    edge(17U);
    EXPECT_EQ(TIM4->SR & (channel_if | channel_of), 0U);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&port, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 17U);
    EXPECT_EQ(event.flags, NX_TIMER_CAPTURE_OVERCAPTURE);
    nx_timer_capture_state_t info{};
    ASSERT_EQ(nx_timer_capture_state(&port, &info), NX_SUCCESS);
    EXPECT_EQ(info.loss_incidents, 1U);
    EXPECT_EQ(info.counter_bits, 16U);
    EXPECT_EQ(info.tick_hz, 1000000U);
}

TEST_F(F1Tim4Capture, NeverAcknowledgesUnobservedLaterCaptureOrOverflow) {
    start(2U);
    edge_during_ack = true;
    TIM4->SR |= static_cast<uint32_t>(TIM_SR_UIF) | channel_of;
    edge(7U);
    EXPECT_EQ(TIM4->SR, TIM_SR_UIF | channel_if);
    nx_stm32f1_tim4_capture_irq(&state);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&port, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 7U);
    EXPECT_EQ(event.flags, NX_TIMER_CAPTURE_OVERCAPTURE);
    ASSERT_EQ(nx_timer_capture_read(&port, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 8U);
    EXPECT_EQ(event.flags, 0U);
}

TEST_F(F1Tim4Capture, RetainsOrderAndReportsRingLossWithNextRetainedEdge) {
    start(1U);
    edge(10U);
    edge(11U);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&port, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 10U);
    EXPECT_EQ(event.flags, 0U);
    edge(12U);
    ASSERT_EQ(nx_timer_capture_read(&port, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 12U);
    EXPECT_EQ(event.flags, NX_TIMER_CAPTURE_RING_LOSS);
    EXPECT_EQ(nx_timer_capture_read(&port, &event), NX_ERROR_EMPTY);
    EXPECT_EQ(event.captured_ticks, 12U);
}

TEST_F(F1Tim4Capture, HoldsLoanUntilBothSourcesAndFinalEdgeAreDrained) {
    start(2U);
    edge(18U, false);
    block_dier = true;
    block_disable = true;
    EXPECT_EQ(nx_timer_capture_stop(&port), NX_ERROR_BUSY);
    EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_TIM4EN, 0U);
    EXPECT_EQ(ccr_reads, 0U);
    block_dier = false;
    block_disable = true;
    EXPECT_NE(timer.enabled, 0U);
    EXPECT_EQ(nx_timer_capture_stop(&port), NX_ERROR_BUSY);
    nx_stm32f1_tim4_capture_irq(&state);
    EXPECT_EQ(ccr_reads, 0U);
    block_disable = false;
    block_witness_ack = true;
    EXPECT_EQ(nx_timer_capture_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(TIM4->DIER | TIM4->CCER | TIM4->CR1 | EXTI->IMR, 0U);
    nx_timer_capture_state_t info{};
    ASSERT_EQ(nx_timer_capture_state(&port, &info), NX_SUCCESS);
    EXPECT_TRUE(info.draining);
    EXPECT_FALSE(info.running);
    EXPECT_EQ(info.pending, 1U);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&port, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 18U);
    EXPECT_EQ(nx_timer_capture_stop(&port), NX_ERROR_BUSY);
    block_witness_ack = false;
    ASSERT_EQ(nx_timer_capture_stop(&port), NX_SUCCESS);
    EXPECT_EQ(timer.enabled | timer.pending | witness.enabled | witness.pending,
              0U);
    ASSERT_EQ(nx_stm32f1_tim4_capture_close(&state), NX_SUCCESS);
    EXPECT_EQ(RCC->APB1ENR | RCC->APB2ENR, 0U);
}

TEST_F(F1Tim4Capture, RejectsForeignVectorAndNeverBorrowsOnFailedStart) {
    EXTI->IMR = peer;
    EXPECT_EQ(nx_stm32f1_tim4_capture_prepare(&state, 1000000U, 5U),
              NX_ERROR_BUSY);
    EXPECT_EQ(RCC->APB1ENR | RCC->APB2ENR, 0U);
    EXTI->IMR = 0U;
    prepare();
    block_enable = true;
    ASSERT_EQ(nx_timer_capture_start(&port, ring, 2U), NX_ERROR_IO);
    EXPECT_EQ(state.events, nullptr);
    EXPECT_EQ(TIM4->DIER | TIM4->CCER | TIM4->CR1 | EXTI->IMR, 0U);
    ASSERT_EQ(nx_stm32f1_tim4_capture_close(&state), NX_SUCCESS);
}
} /* namespace */

extern "C" {
bool nx_stm32f1_system_live(void) {
    return true;
}

nx_time_us_t nx_time_now_us(void) {
    return now;
}

void nx_f1_capture_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &RCC->APB1RSTR && (value & RCC_APB1RSTR_TIM4RST) != 0U) {
        *TIM4 = TIM_TypeDef{};
        TIM4->ARR = UINT16_MAX;
    }
    if (reg == &TIM4->EGR && value == TIM_EGR_UG) {
        EXPECT_EQ(TIM4->CR2 & TIM_CR2_MMS, TIM_CR2_MMS_0);
        EXPECT_EQ(TIM4->CR1 & TIM_CR1_CEN, 0U);
        TIM4->SR |= TIM_SR_UIF;
    }
    if (reg == &EXTI->PR) {
        ASSERT_EQ(value, line);
        if (!block_witness_ack) {
            *reg &= ~value;
        }
    } else if (reg == &TIM4->SR) {
        if (edge_during_ack && (value & channel_of) == 0U) {
            edge_during_ack = false;
            TIM4->CCR1 += 1U;
            TIM4->SR |= channel_if;
            EXTI->PR |= line;
        }
        *reg &= value;
    } else if (reg != &TIM4->DIER || !block_dier || value != 0U) {
        *reg = value;
    }
}

void nx_f1_capture_bsrr(GPIO_TypeDef* gpio, uint32_t value) {
    gpio->ODR = (gpio->ODR | (value & UINT16_MAX)) & ~(value >> 16U);
}

uint32_t nx_f1_capture_ccr(void) {
    const uint32_t value = TIM4->CCR1;
    ++ccr_reads;
    if (collide) {
        EXPECT_NE(EXTI->IMR & line, 0U);
        EXPECT_NE(EXTI->RTSR & line, 0U);
        EXPECT_EQ(EXTI->PR & line, 0U);
        TIM4->CCR1 += 1U;
        EXTI->PR |= line;
        collide = false;
    }
    TIM4->SR &= ~channel_if;
    return value;
}

uint32_t nx_f1_capture_get_enable(IRQn_Type irq) {
    return vector(irq).enabled;
}
uint32_t nx_f1_capture_get_active(IRQn_Type irq) {
    (void)vector(irq);
    return 0U;
}
uint32_t nx_f1_capture_get_pending(IRQn_Type irq) {
    return vector(irq).pending;
}
uint32_t nx_f1_capture_get_priority(IRQn_Type irq) {
    return vector(irq).priority;
}
void nx_f1_capture_enable(IRQn_Type irq) {
    EXPECT_EQ(irq, TIM4_IRQn);
    if (!block_enable) {
        vector(irq).enabled = 1U;
    }
}
void nx_f1_capture_disable(IRQn_Type irq) {
    EXPECT_EQ(irq, TIM4_IRQn);
    if (!block_disable) {
        vector(irq).enabled = 0U;
    }
}
void nx_f1_capture_clear(IRQn_Type irq) {
    vector(irq).pending = 0U;
}
void nx_f1_capture_set_priority(IRQn_Type irq, uint32_t priority) {
    EXPECT_EQ(irq, TIM4_IRQn);
    vector(irq).priority = priority;
}
}
