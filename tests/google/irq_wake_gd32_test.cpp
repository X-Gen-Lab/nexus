/**
 * \file            irq_wake_gd32_test.cpp
 * \brief           Real GD32 UART uses actual model IRQ priority and grouping
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
extern bool g_gd32_model_isr;
extern uint32_t g_gd32_model_mask;
/** \brief This cold binding fixture never injects a hardware UART event. */
void USART0_IRQHandler(void) {
}
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

class Gd32WakeSink {
  public:
    MOCK_METHOD(nx_result_t, notify, ());
    static nx_result_t callback(void* context) {
        return static_cast<Gd32WakeSink*>(context)->notify();
    }
};

class Gd32IrqWake : public testing::Test {
  protected:
    nx_gd32_uart_state_t state = {};
    const nx_uart_port_t port = {&nx_gd32_uart_ops, &state};
    void SetUp() override {
        std::memset(&g_gd32_model_nvic, 0, sizeof(g_gd32_model_nvic));
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        state.controller = &nx_gd32_usart0_controller;
        state.initialized = true;
        NVIC_SetPriority(USART0_IRQn, 5U);
    }
};

TEST_F(Gd32IrqWake, CallerCannotWeakenTheExplicitKernelCeiling) {
    testing::StrictMock<Gd32WakeSink> observer;
    const nx_irq_wake_t wake = {&observer, Gd32WakeSink::callback, true};
    NVIC_SetPriority(USART0_IRQn, 4U);
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 1U), NX_ERROR_PERMISSION);
    EXPECT_EQ(state.wake, nullptr);
    EXPECT_EQ(g_gd32_model_mask, 0U);
    NVIC_SetPriority(USART0_IRQn, 5U);
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 1U), NX_SUCCESS);
    EXPECT_EQ(state.wake, &wake);
}

TEST_F(Gd32IrqWake, ActualSubpriorityGroupingOnlyRestrictsKernelTargets) {
    testing::StrictMock<Gd32WakeSink> existing;
    testing::StrictMock<Gd32WakeSink> rejected;
    const nx_irq_wake_t safe = {&existing, Gd32WakeSink::callback, false};
    const nx_irq_wake_t unsafe = {&rejected, Gd32WakeSink::callback, true};
    NVIC->priority_group = 4U;
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &safe, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &unsafe, 5U),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(state.wake, &safe);
    EXPECT_EQ(g_gd32_model_mask, 0U);
    NVIC->priority_group = 3U;
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &unsafe, 5U), NX_SUCCESS);
    EXPECT_EQ(state.wake, &unsafe);
    NVIC->priority_group = 7U;
    EXPECT_EQ(nx_uart_port_attach_wake(&port, nullptr, 0U), NX_SUCCESS);
    EXPECT_EQ(state.wake, nullptr);
}

TEST_F(Gd32IrqWake, InvalidPublisherRejectsBeforeNvicReadOrSinkChange) {
    testing::StrictMock<Gd32WakeSink> observer;
    const nx_irq_wake_t wake = {&observer, Gd32WakeSink::callback, true};
    nx_gd32_uart_controller_t invalid = nx_gd32_usart0_controller;
    invalid.irq = -1;
    state.controller = &invalid;
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_ERROR_INVALID);
    EXPECT_EQ(state.wake, nullptr);
    invalid.irq = 91;
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_ERROR_INVALID);
    EXPECT_EQ(state.wake, nullptr);
    EXPECT_EQ(g_gd32_model_mask, 0U);
}

TEST_F(Gd32IrqWake, IllegalControlContextLeavesIncomingMaskAndSinkUnchanged) {
    testing::StrictMock<Gd32WakeSink> observer;
    const nx_irq_wake_t wake = {&observer, Gd32WakeSink::callback, true};
    g_gd32_model_isr = true;
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_ERROR_CONTEXT);
    EXPECT_EQ(state.wake, nullptr);
    g_gd32_model_isr = false;
    g_gd32_model_mask = 1U;
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_ERROR_CONTEXT);
    EXPECT_EQ(state.wake, nullptr);
    EXPECT_EQ(g_gd32_model_mask, 1U);
    g_gd32_model_mask = 0U;
}
