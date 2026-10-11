/**
 * \file            irq_wake_stm32_test.cpp
 * \brief           Production STM32 UART cold binding preserves policy and sink
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_provider.h"
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>

class Stm32WakeSink {
  public:
    MOCK_METHOD(nx_result_t, notify, ());
    static nx_result_t callback(void* context) {
        return static_cast<Stm32WakeSink*>(context)->notify();
    }
};

TEST(Stm32IrqWake, RejectedCeilingCannotReplaceExistingSink) {
    nx_stm32_uart_state_t state = {};
    state.initialized = true;
    state.irq = USART1_IRQn;
    const nx_uart_port_t port = {&nx_stm32_uart_ops, &state};
    testing::StrictMock<Stm32WakeSink> existing;
    testing::StrictMock<Stm32WakeSink> rejected;
    const nx_irq_wake_t safe = {&existing, Stm32WakeSink::callback, false};
    const nx_irq_wake_t unsafe = {&rejected, Stm32WakeSink::callback, true};
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &safe, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &unsafe, 6U),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(state.wake, &safe);
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &unsafe, 1U), NX_SUCCESS);
    EXPECT_EQ(state.wake, &unsafe);
    EXPECT_EQ(nx_uart_port_attach_wake(&port, nullptr, 0U), NX_SUCCESS);
    EXPECT_EQ(state.wake, nullptr);
}

TEST(Stm32IrqWake, InvalidPublisherRejectsBeforeNvicReadOrSinkChange) {
    nx_stm32_uart_state_t state = {};
    state.initialized = true;
    const nx_uart_port_t port = {&nx_stm32_uart_ops, &state};
    testing::StrictMock<Stm32WakeSink> observer;
    const nx_irq_wake_t wake = {&observer, Stm32WakeSink::callback, true};
    state.irq = static_cast<IRQn_Type>(-1);
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_ERROR_INVALID);
    EXPECT_EQ(state.wake, nullptr);
    state.irq = static_cast<IRQn_Type>(82);
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_ERROR_INVALID);
    EXPECT_EQ(state.wake, nullptr);
    state.irq = static_cast<IRQn_Type>(81);
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_SUCCESS);
    EXPECT_EQ(state.wake, &wake);
}
