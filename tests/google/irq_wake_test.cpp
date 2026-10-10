/**
 * \file            irq_wake_test.cpp
 *
 * \brief           Actual provider events latch facts before optional IRQ wake
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/native/model.h"
#include "provider.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

class IrqWakeObserver {
  public:
    MOCK_METHOD(nx_result_t, notify, ());
    static nx_result_t callback(void* context) {
        return static_cast<IrqWakeObserver*>(context)->notify();
    }
};

TEST(IrqWakeProvider, UartLatchesRxBeforeWakeAndStopsPublishingOnDetach) {
    nx_native_uart_state_t state = {};
    nx_uart_port_t port = {&nx_native_uart_ops, &state};
    uint8_t rx[2] = {};
    nx_native_uart_config_t config = {};
    config.profile = NX_UART_RX_BYTES;
    config.rx_storage = rx;
    config.rx_capacity = sizeof(rx);
    ASSERT_EQ(nx_native_uart_configure_instance(&state, &config), NX_SUCCESS);
    IrqWakeObserver observer;
    const nx_irq_wake_t wake = {&observer, IrqWakeObserver::callback, true};
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 6U), NX_ERROR_PERMISSION);
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_SUCCESS);
    EXPECT_CALL(observer, notify()).WillOnce(testing::Invoke([&port]() {
        uint8_t byte = 0U;
        size_t count = 0U;
        EXPECT_EQ(nx_uart_port_read_bytes(&port, &byte, 1U, &count),
                  NX_SUCCESS);
        EXPECT_EQ(byte, 42U);
        EXPECT_EQ(count, 1U);
        return NX_SUCCESS;
    }));
    EXPECT_EQ(nx_native_uart_receive_instance(&state, 42U, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_attach_wake(&port, nullptr, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_native_uart_receive_instance(&state, 43U, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_ERROR_STATE);
}

TEST(IrqWakeProvider, ExtiOverflowWakesConsumerWithoutOverwritingEvents) {
    nx_native_exti_state_t state = {};
    nx_exti_port_t port = {&nx_native_exti_ops, &state};
    nx_exti_event_t event[1] = {};
    ASSERT_EQ(nx_native_exti_configure_instance(&state, 3U, NX_EXTI_RISING,
                                                event, 1U),
              NX_SUCCESS);
    IrqWakeObserver observer;
    const nx_irq_wake_t wake = {&observer, IrqWakeObserver::callback, false};
    ASSERT_EQ(nx_exti_port_attach_wake(&port, &wake, 0U), NX_SUCCESS);
    EXPECT_CALL(observer, notify())
        .Times(2)
        .WillRepeatedly(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_native_exti_emit_instance(&state, 3U, NX_EXTI_RISING),
              NX_SUCCESS);
    EXPECT_EQ(nx_native_exti_emit_instance(&state, 3U, NX_EXTI_RISING),
              NX_ERROR_OVERFLOW);
    nx_exti_event_t received[2] = {};
    size_t count = 0U;
    EXPECT_EQ(nx_exti_port_read(&port, received, 2U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 2U);
    EXPECT_EQ(received[0].line, 3U);
    EXPECT_EQ(received[1].flags, NX_EXTI_EVENT_LOSS);
    EXPECT_EQ(nx_exti_port_stop(&port), NX_SUCCESS);
}
