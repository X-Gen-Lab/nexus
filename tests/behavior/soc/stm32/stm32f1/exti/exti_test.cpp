/**
 * \file            exti_test.cpp
 * \brief           F103 EXTI13 event, shared vector and retained cleanup.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/exti/instance.h"
#include "system/platform.h"
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t peripheral = 0x40000000U;
constexpr size_t peripheral_size = 0x24000U;
constexpr uint32_t line = 1U << 13U;
constexpr uint32_t peer = 1U << 12U;
constexpr uint32_t prior_route = 0x1041U;
bool irq_enabled;
bool irq_pending;
bool irq_active;
bool block_ack;
uint32_t priority;
unsigned clear_calls;
nx_time_us_t now;

class F1Exti : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    nx_stm32f1_exti_t state{};
    nx_exti_event_t storage[2]{};
    nx_stm32f1_exti_config_t config{};
    const nx_exti_port_t port{&nx_stm32f1_exti_ops, &state};

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(peripheral), peripheral_size,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(peripheral));
        irq_enabled = irq_pending = irq_active = block_ack = false;
        priority = 11U;
        clear_calls = 0U;
        now = 10U;
        AFIO->EXTICR[3] = prior_route;
        GPIOC->CRH = UINT32_C(0x44444444);
        config.gpio = GPIOC;
        config.storage = storage;
        config.capacity = 2U;
        config.edge = NX_EXTI_BOTH;
        config.gpio_index = 2U;
        config.line = 13U;
        config.priority = 5U;
        config.pull = 0U;
    }

    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, peripheral_size), 0);
        }
    }

    void dispatch() {
        nx_stm32f1_exti_irq(&state);
    }

    void edge(bool high) {
        GPIOC->IDR = high ? line : 0U;
        EXTI->PR |= line;
        dispatch();
        ++now;
    }
};

TEST_F(F1Exti, OwnsOnlyConfiguredCauseAndRestoresItsPhysicalRoute) {
    EXTI->PR = peer;
    EXTI->EMR = peer;
    EXTI->RTSR = peer;
    ASSERT_EQ(nx_stm32f1_exti_prepare(&state, &config), NX_SUCCESS);
    EXPECT_EQ(AFIO->EXTICR[3],
              (prior_route & ~static_cast<uint32_t>(AFIO_EXTICR4_EXTI13)) |
                  AFIO_EXTICR4_EXTI13_PC);
    EXPECT_TRUE(irq_enabled);
    EXPECT_EQ(priority, 5U);
    EXPECT_EQ(EXTI->PR, peer);
    const auto before = clear_calls;
    dispatch();
    EXPECT_EQ(clear_calls, before);
    GPIOC->IDR = line;
    EXTI->PR = line | peer;
    EXTI->IMR &= ~line;
    dispatch();
    EXPECT_EQ(clear_calls, before);
    EXTI->IMR |= line;
    dispatch();
    EXPECT_EQ(EXTI->PR, peer);
    nx_exti_event_t events[1]{};
    size_t count = 0U;
    ASSERT_EQ(nx_exti_port_read(&port, events, 1U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 1U);
    EXPECT_EQ(events[0].edge, NX_EXTI_RISING);
    EXPECT_EQ(events[0].flags, NX_EXTI_EVENT_COALESCED);
    ASSERT_EQ(nx_exti_port_stop(&port), NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_exti_close(&state), NX_SUCCESS);
    EXPECT_EQ(EXTI->EMR, peer);
    EXPECT_EQ(EXTI->RTSR, peer);
    EXPECT_EQ(EXTI->PR, peer);
    EXPECT_EQ(AFIO->EXTICR[3], prior_route);
    EXPECT_EQ(GPIOC->CRH, UINT32_C(0x44444444));
    EXPECT_EQ(RCC->APB2ENR, 0U);
    EXPECT_EQ(priority, 11U);
}

TEST_F(F1Exti, ReportsLossAfterOlderFactsAndBeforeLaterEdges) {
    ASSERT_EQ(nx_stm32f1_exti_prepare(&state, &config), NX_SUCCESS);
    edge(true);
    edge(false);
    edge(true);
    nx_exti_event_t events[3]{};
    size_t count = 0U;
    ASSERT_EQ(nx_exti_port_read(&port, events, 1U, &count), NX_SUCCESS);
    EXPECT_EQ(events[0].edge, NX_EXTI_RISING);
    edge(false);
    ASSERT_EQ(nx_exti_port_read(&port, events, 3U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 2U);
    EXPECT_EQ(events[0].edge, NX_EXTI_FALLING);
    EXPECT_EQ(events[1].flags, NX_EXTI_EVENT_LOSS);
    edge(true);
    ASSERT_EQ(nx_exti_port_read(&port, events, 3U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 1U);
    EXPECT_EQ(events[0].edge, NX_EXTI_RISING);
}

TEST_F(F1Exti, FailedPendingDrainRetainsPadsButStopsQueueWrites) {
    ASSERT_EQ(nx_stm32f1_exti_prepare(&state, &config), NX_SUCCESS);
    EXTI->PR |= line;
    block_ack = true;
    EXPECT_EQ(nx_exti_port_stop(&port), NX_ERROR_IO);
    EXPECT_FALSE(irq_enabled);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_IOPCEN, 0U);
    const nx_time_us_t prior = storage[0].timestamp_us;
    dispatch();
    EXPECT_EQ(storage[0].timestamp_us, prior);
    block_ack = false;
    ASSERT_EQ(nx_exti_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(EXTI->PR & line, 0U);
    ASSERT_EQ(nx_stm32f1_exti_close(&state), NX_SUCCESS);
    EXPECT_EQ(RCC->APB2ENR, 0U);
}

TEST_F(F1Exti, ForeignEnabledLineObstructsSharedVectorRelease) {
    EXTI->IMR = peer;
    EXPECT_EQ(nx_stm32f1_exti_prepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    EXTI->IMR = 0U;
    ASSERT_EQ(nx_stm32f1_exti_prepare(&state, &config), NX_SUCCESS);
    EXTI->IMR |= peer;
    EXTI->PR |= peer;
    irq_pending = true;
    EXPECT_EQ(nx_exti_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_TRUE(irq_enabled);
    EXPECT_TRUE(irq_pending);
    EXPECT_EQ(EXTI->IMR, peer);
    EXPECT_EQ(EXTI->PR, peer);
    EXTI->IMR = 0U;
    ASSERT_EQ(nx_stm32f1_exti_close(&state), NX_SUCCESS);
    EXPECT_EQ(EXTI->PR, peer);
}

TEST_F(F1Exti, SingleEdgeRemainsKnownEvenAfterRawLevelChanges) {
    config.edge = NX_EXTI_RISING;
    ASSERT_EQ(nx_stm32f1_exti_prepare(&state, &config), NX_SUCCESS);
    edge(false);
    bool high = true;
    ASSERT_EQ(nx_exti_port_level(&port, &high), NX_SUCCESS);
    EXPECT_FALSE(high);
    nx_exti_event_t events[1]{};
    size_t count = 0U;
    ASSERT_EQ(nx_exti_port_read(&port, events, 1U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 1U);
    EXPECT_EQ(events[0].edge, NX_EXTI_RISING);
    EXPECT_EQ(events[0].flags, NX_EXTI_EVENT_COALESCED);
}
} /* namespace */

extern "C" {
bool nx_stm32f1_system_live(void) {
    return true;
}
nx_time_us_t nx_time_now_us(void) {
    return now;
}
void nx_exti_model_clear(uint32_t mask) {
    EXPECT_EQ(mask, line);
    ++clear_calls;
    if (!block_ack) {
        EXTI->PR &= ~mask;
    }
}
void nx_exti_model_bsrr(GPIO_TypeDef* gpio, uint32_t value) {
    gpio->ODR = (gpio->ODR & ~(value >> 16U)) | (value & UINT16_MAX);
}
bool nx_exti_model_irq_enabled(void) {
    return irq_enabled;
}
bool nx_exti_model_irq_pending(void) {
    return irq_pending;
}
bool nx_exti_model_irq_active(void) {
    return irq_active;
}
uint32_t nx_exti_model_irq_priority(void) {
    return priority;
}
void nx_exti_model_set_priority(uint32_t value) {
    priority = value;
}
void nx_exti_model_irq_enable(void) {
    irq_enabled = true;
}
void nx_exti_model_irq_disable(void) {
    irq_enabled = false;
}
void nx_exti_model_irq_clear(void) {
    irq_pending = false;
}
}
