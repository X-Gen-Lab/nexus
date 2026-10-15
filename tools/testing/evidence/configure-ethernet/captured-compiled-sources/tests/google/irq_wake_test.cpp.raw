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
#include "nexus/arch/arch.h"
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

class BaselineIrqWakeProvider : public testing::TestWithParam<uint16_t> {};

TEST_P(BaselineIrqWakeProvider, ExplicitPrimaskPolicyBindsActualUartModel) {
    nx_native_uart_state_t state = {};
    const nx_uart_port_t port = {&nx_native_uart_ops, &state};
    uint8_t rx[2] = {};
    const nx_irq_policy_t policy = {NX_IRQ_KERNEL_PRIMASK, GetParam(), 2U, 0U};
    const nx_irq_source_t source = {static_cast<int16_t>(GetParam() - 1U), 0U,
                                    0U};
    nx_native_uart_config_t config = {};
    config.profile = NX_UART_RX_BYTES;
    config.rx_storage = rx;
    config.rx_capacity = sizeof(rx);
    ASSERT_EQ(nx_native_uart_configure_instance(&state, &config), NX_SUCCESS);
    ASSERT_EQ(nx_native_uart_model_irq_configure(&port, &policy, &source),
              NX_SUCCESS);
    testing::StrictMock<IrqWakeObserver> observer;
    const nx_irq_wake_t wake = {&observer, IrqWakeObserver::callback, true};
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(state.wake, nullptr);
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &wake, 0U), NX_SUCCESS);
    EXPECT_CALL(observer, notify()).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_native_uart_receive_instance(&state, 42U, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_attach_wake(&port, nullptr, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}

INSTANTIATE_TEST_SUITE_P(M0M0PlusM23, BaselineIrqWakeProvider,
                         testing::Values<uint16_t>(32U, 32U, 240U));

TEST(IrqWakeProvider, RejectedPolicyDoesNotReplaceExistingBorrowedSink) {
    nx_native_uart_state_t state = {};
    const nx_uart_port_t port = {&nx_native_uart_ops, &state};
    uint8_t rx[2] = {};
    const nx_irq_policy_t policy = {NX_IRQ_KERNEL_NONE, 32U, 2U, 0U};
    const nx_irq_source_t source = {0, 0U, 0U};
    nx_native_uart_config_t config = {};
    config.profile = NX_UART_RX_BYTES;
    config.rx_storage = rx;
    config.rx_capacity = sizeof(rx);
    ASSERT_EQ(nx_native_uart_configure_instance(&state, &config), NX_SUCCESS);
    ASSERT_EQ(nx_native_uart_model_irq_configure(&port, &policy, &source),
              NX_SUCCESS);
    testing::StrictMock<IrqWakeObserver> existing;
    testing::StrictMock<IrqWakeObserver> rejected;
    const nx_irq_wake_t safe = {&existing, IrqWakeObserver::callback, false};
    const nx_irq_wake_t unsafe = {&rejected, IrqWakeObserver::callback, true};
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &safe, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &unsafe, 0U),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(state.wake, &safe);
    EXPECT_CALL(existing, notify()).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_native_uart_receive_instance(&state, 42U, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}

TEST(IrqWakeProvider, MalformedModelFactsRejectBeforeChangingPort) {
    nx_native_uart_state_t state = {};
    const nx_uart_port_t port = {&nx_native_uart_ops, &state};
    const nx_irq_policy_t policy = {NX_IRQ_KERNEL_PRIMASK, 32U, 2U, 0U};
    nx_irq_source_t source = {32, 0U, 0U};
    uint8_t rx[2] = {};
    nx_native_uart_config_t config = {};
    config.profile = NX_UART_RX_BYTES;
    config.rx_storage = rx;
    config.rx_capacity = sizeof(rx);
    ASSERT_EQ(nx_native_uart_configure_instance(&state, &config), NX_SUCCESS);
    EXPECT_EQ(nx_native_uart_model_irq_configure(&port, &policy, nullptr),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_native_uart_model_irq_configure(&port, &policy, &source),
              NX_ERROR_INVALID);
    EXPECT_EQ(state.irq_policy.kernel, NX_IRQ_KERNEL_BASEPRI);
    EXPECT_EQ(state.irq_source.irq_number, 0);
    source.irq_number = 31;
    ASSERT_EQ(nx_native_uart_model_irq_configure(&port, &policy, &source),
              NX_SUCCESS);
    EXPECT_EQ(state.irq_policy.kernel, NX_IRQ_KERNEL_PRIMASK);
    EXPECT_EQ(state.irq_source.irq_number, 31);
    EXPECT_EQ(nx_native_uart_model_irq_configure(&port, nullptr, &source),
              NX_ERROR_INVALID);
    EXPECT_EQ(state.irq_policy.kernel, NX_IRQ_KERNEL_PRIMASK);
    EXPECT_EQ(state.irq_source.irq_number, 31);
}

TEST(IrqWakeProvider, ModelFactsAreCopiedAndLiveSinkPreventsReconfiguration) {
    nx_native_uart_state_t state = {};
    const nx_uart_port_t port = {&nx_native_uart_ops, &state};
    uint8_t rx[2] = {};
    nx_native_uart_config_t config = {};
    config.profile = NX_UART_RX_BYTES;
    config.rx_storage = rx;
    config.rx_capacity = sizeof(rx);
    ASSERT_EQ(nx_native_uart_configure_instance(&state, &config), NX_SUCCESS);
    nx_irq_policy_t policy = {NX_IRQ_KERNEL_PRIMASK, 32U, 2U, 0U};
    nx_irq_source_t source = {31, 0U, 0U};
    ASSERT_EQ(nx_native_uart_model_irq_configure(&port, &policy, &source),
              NX_SUCCESS);
    policy.kernel = NX_IRQ_KERNEL_NONE;
    source.irq_number = 0;
    testing::StrictMock<IrqWakeObserver> observer;
    const nx_irq_wake_t wake = {&observer, IrqWakeObserver::callback, true};
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &wake, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_native_uart_model_irq_configure(&port, &policy, &source),
              NX_ERROR_BUSY);
    EXPECT_EQ(state.wake, &wake);
    EXPECT_EQ(state.irq_policy.kernel, NX_IRQ_KERNEL_PRIMASK);
    EXPECT_EQ(state.irq_source.irq_number, 31);
    EXPECT_CALL(observer, notify()).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_native_uart_receive_instance(&state, 42U, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_native_uart_model_irq_configure(&port, &policy, &source),
              NX_ERROR_STATE);
}

TEST(IrqWakeProvider,
     ActiveUsersAndMaskedCallerRejectColdFactsWithoutMutation) {
    nx_native_uart_state_t state = {};
    const nx_uart_port_t port = {&nx_native_uart_ops, &state};
    const nx_irq_policy_t policy = {NX_IRQ_KERNEL_PRIMASK, 32U, 2U, 0U};
    const nx_irq_source_t source = {31, 0U, 0U};
    uint8_t rx[2] = {};
    nx_native_uart_config_t config = {};
    config.profile = NX_UART_RX_BYTES;
    config.rx_storage = rx;
    config.rx_capacity = sizeof(rx);
    ASSERT_EQ(nx_native_uart_configure_instance(&state, &config), NX_SUCCESS);
    const nx_uart_port_t malformed = {nullptr, &state};
    EXPECT_EQ(nx_native_uart_model_irq_configure(nullptr, &policy, &source),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_native_uart_model_irq_configure(&malformed, &policy, &source),
              NX_ERROR_INVALID);
    nx_uart_tx_request_t request = {};
    state.active = &request;
    EXPECT_EQ(nx_native_uart_model_irq_configure(&port, &policy, &source),
              NX_ERROR_BUSY);
    EXPECT_EQ(state.active, &request);
    state.active = nullptr;
    nx_stream_t stream = {};
    state.rx_stream = &stream;
    EXPECT_EQ(nx_native_uart_model_irq_configure(&port, &policy, &source),
              NX_ERROR_BUSY);
    EXPECT_EQ(state.rx_stream, &stream);
    state.rx_stream = nullptr;
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    EXPECT_EQ(nx_native_uart_model_irq_configure(&port, &policy, &source),
              NX_ERROR_CONTEXT);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(saved);
    EXPECT_EQ(state.irq_policy.kernel, NX_IRQ_KERNEL_BASEPRI);
    EXPECT_EQ(state.irq_source.irq_number, 0);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}
