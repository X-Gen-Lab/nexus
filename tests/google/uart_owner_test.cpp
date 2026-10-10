/**
 * \file            uart_owner_test.cpp
 * \brief           UART owner accepts only providers with a complete executor
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/uart_owner.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using ::testing::Return;
using ::testing::StrictMock;

/** \brief Mock only hardware interactions used by the explicit owner. */
class UARTProvider {
  public:
    MOCK_METHOD(nx_result_t, submit, (nx_uart_tx_request_t * request));
    MOCK_METHOD(nx_result_t, cancel, (nx_uart_tx_request_t * request));
    MOCK_METHOD(void, service, ());
};

/** \brief Forward admission to this exact provider instance. */
nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    return static_cast<UARTProvider*>(context)->submit(request);
}
/** \brief Forward cancellation without assuming that it settles storage. */
nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    return static_cast<UARTProvider*>(context)->cancel(request);
}
/** \brief Advance hardware only on the explicit execution owner. */
void service(void* context) {
    static_cast<UARTProvider*>(context)->service();
}
/** \brief Select the exact mandatory async executor methods. */
nx_uart_ops_t executor_operations() {
    nx_uart_ops_t ops{};
    ops.submit = submit;
    ops.cancel = cancel;
    ops.service = service;
    return ops;
}
const nx_uart_ops_t operations = executor_operations();

TEST(UARTOwnerContract, RejectsIncompleteProviderBeforeAdvertisingExecutor) {
    StrictMock<UARTProvider> provider;
    nx_uart_owner_executor_t executor{};
    const nx_uart_port_t missing = {nullptr, &provider};
    const auto invalid = nx_uart_owner_executor_port(&executor, &missing);
    EXPECT_EQ(invalid.context, nullptr);
    EXPECT_EQ(invalid.start, nullptr);
    nx_uart_ops_t incomplete = operations;
    incomplete.cancel = nullptr;
    const nx_uart_port_t no_cancel = {&incomplete, &provider};
    EXPECT_EQ(nx_uart_owner_executor_port(&executor, &no_cancel).start,
              nullptr);
}

TEST(UARTOwnerContract, RejectedSubmissionRetainsNeitherBorrowNorPayload) {
    StrictMock<UARTProvider> provider;
    const nx_uart_port_t port = {&operations, &provider};
    nx_uart_owner_executor_t executor{};
    const auto bridge = nx_uart_owner_executor_port(&executor, &port);
    const uint8_t bytes[] = {1, 2};
    nx_uart_owner_operation_t operation{bytes, sizeof(bytes)};
    EXPECT_CALL(provider, submit(&executor.inner))
        .WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(bridge.start(bridge.context, &operation, NX_DEADLINE_NEVER),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&executor.inner.base), NX_REQUEST_READY);
    EXPECT_EQ(executor.inner.data, nullptr);
    EXPECT_FALSE(executor.pending);
}

TEST(UARTOwnerContract, QuarantineRetainsPayloadUntilProviderDrainProof) {
    StrictMock<UARTProvider> provider;
    const nx_uart_port_t port = {&operations, &provider};
    nx_uart_owner_executor_t executor{};
    const auto bridge = nx_uart_owner_executor_port(&executor, &port);
    const uint8_t bytes[] = {1, 2};
    nx_uart_owner_operation_t operation{bytes, sizeof(bytes)};
    EXPECT_CALL(provider, submit(&executor.inner))
        .WillOnce(::testing::Invoke([](nx_uart_tx_request_t* request) {
            return nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        }));
    ASSERT_EQ(bridge.start(bridge.context, &operation, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_CALL(provider, service()).WillOnce(::testing::Invoke([&] {
        EXPECT_EQ(
            nx_request_transition(&executor.inner.base, NX_REQUEST_QUARANTINED),
            NX_SUCCESS);
    }));
    nx_result_t result = NX_SUCCESS;
    size_t transferred = 0;
    EXPECT_EQ(bridge.service(bridge.context, &result, &transferred),
              NX_ERROR_IO);
    EXPECT_TRUE(executor.pending);
    EXPECT_EQ(executor.inner.data, bytes);
    EXPECT_CALL(provider, service()).WillOnce(::testing::Invoke([&] {
        nx_request_settle(&executor.inner.base, NX_ERROR_CANCELLED, 0);
    }));
    EXPECT_EQ(bridge.service(bridge.context, &result, &transferred),
              NX_SUCCESS);
    EXPECT_EQ(result, NX_ERROR_CANCELLED);
    EXPECT_FALSE(executor.pending);
    EXPECT_EQ(executor.inner.data, nullptr);
}
} /* namespace */
