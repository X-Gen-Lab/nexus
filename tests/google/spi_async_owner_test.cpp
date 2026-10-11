/**
 * \file            spi_async_owner_test.cpp
 * \brief           Async SPI owner preserves bus identity and private loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/spi_async_owner.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using ::testing::Return;
using ::testing::StrictMock;

/** \brief Replace provider interactions while retaining real owner logic. */
class SPIProvider {
  public:
    MOCK_METHOD(nx_result_t, submit, (nx_spi_request_t * request));
    MOCK_METHOD(nx_result_t, cancel, (nx_spi_request_t * request));
    MOCK_METHOD(void, service, ());
};

/** \brief Submit through one endpoint's retained controller context. */
nx_result_t submit(void* context, nx_spi_request_t* request) {
    return static_cast<SPIProvider*>(context)->submit(request);
}
/** \brief Keep cancellation separate from settled buffer ownership. */
nx_result_t cancel(void* context, nx_spi_request_t* request) {
    return static_cast<SPIProvider*>(context)->cancel(request);
}
/** \brief Advance the explicitly bound controller. */
void service(void* context) {
    static_cast<SPIProvider*>(context)->service();
}
/** \brief Compare controller identity before any endpoint admission. */
bool on_port(const void* context, const nx_spi_port_t* port) {
    return context == port->context;
}
/** \brief Define exactly the bridge's required controller capabilities. */
nx_spi_ops_t bus_operations() {
    nx_spi_ops_t ops{};
    ops.cancel = cancel;
    ops.service = service;
    return ops;
}
/** \brief Define an async endpoint with explicit controller membership. */
nx_spi_endpoint_ops_t endpoint_operations() {
    nx_spi_endpoint_ops_t ops{};
    ops.submit = submit;
    ops.on_port = on_port;
    return ops;
}
const auto bus_ops = bus_operations();
const auto endpoint_ops = endpoint_operations();

TEST(SPIAsyncOwner, RejectsWrongBusBeforeTouchingProviderOrPayload) {
    StrictMock<SPIProvider> first;
    StrictMock<SPIProvider> other;
    const nx_spi_port_t bus{&bus_ops, &first};
    const nx_spi_endpoint_t wrong{&endpoint_ops, &other};
    nx_spi_async_owner_executor_t executor{};
    const auto bridge = nx_spi_async_owner_executor_port(&executor, &bus);
    uint8_t bytes[] = {1, 2};
    nx_spi_owner_operation_t operation{&wrong, bytes, bytes, sizeof(bytes)};
    EXPECT_EQ(bridge.start(bridge.context, &operation, NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&executor.inner.base), NX_REQUEST_READY);
    EXPECT_EQ(executor.inner.tx, nullptr);
    EXPECT_EQ(executor.inner.rx, nullptr);
}

TEST(SPIAsyncOwner, RejectionDetachesPrivateBuffersAndPreservesReady) {
    StrictMock<SPIProvider> provider;
    const nx_spi_port_t bus{&bus_ops, &provider};
    const nx_spi_endpoint_t endpoint{&endpoint_ops, &provider};
    nx_spi_async_owner_executor_t executor{};
    const auto bridge = nx_spi_async_owner_executor_port(&executor, &bus);
    uint8_t bytes[] = {1, 2};
    nx_spi_owner_operation_t operation{&endpoint, bytes, bytes, sizeof(bytes)};
    EXPECT_CALL(provider, submit(&executor.inner))
        .WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(bridge.start(bridge.context, &operation, NX_DEADLINE_NEVER),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&executor.inner.base), NX_REQUEST_READY);
    EXPECT_FALSE(executor.pending);
    EXPECT_EQ(executor.inner.tx, nullptr);
    EXPECT_EQ(executor.inner.rx, nullptr);
}

TEST(SPIAsyncOwner, CancellationCannotReleaseQuarantinedStorage) {
    StrictMock<SPIProvider> provider;
    const nx_spi_port_t bus{&bus_ops, &provider};
    const nx_spi_endpoint_t endpoint{&endpoint_ops, &provider};
    nx_spi_async_owner_executor_t executor{};
    const auto bridge = nx_spi_async_owner_executor_port(&executor, &bus);
    uint8_t bytes[] = {1, 2};
    nx_spi_owner_operation_t operation{&endpoint, bytes, bytes, sizeof(bytes)};
    EXPECT_CALL(provider, submit(&executor.inner))
        .WillOnce(::testing::Invoke([](nx_spi_request_t* request) {
            return nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        }));
    ASSERT_EQ(bridge.start(bridge.context, &operation, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_CALL(provider, service()).WillOnce(Return());
    EXPECT_CALL(provider, cancel(&executor.inner)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(bridge.cancel(bridge.context), NX_SUCCESS);
    EXPECT_TRUE(executor.pending);
    EXPECT_CALL(provider, service()).WillOnce(::testing::Invoke([&] {
        EXPECT_EQ(
            nx_request_transition(&executor.inner.base, NX_REQUEST_QUARANTINED),
            NX_SUCCESS);
    }));
    nx_result_t result = NX_SUCCESS;
    size_t transferred = 0;
    EXPECT_EQ(bridge.service(bridge.context, &result, &transferred),
              NX_ERROR_IO);
    EXPECT_EQ(executor.inner.rx, bytes);
    EXPECT_CALL(provider, service()).WillOnce(::testing::Invoke([&] {
        nx_request_settle(&executor.inner.base, NX_ERROR_CANCELLED, 0);
    }));
    EXPECT_EQ(bridge.service(bridge.context, &result, &transferred),
              NX_SUCCESS);
    EXPECT_EQ(result, NX_ERROR_CANCELLED);
    EXPECT_FALSE(executor.pending);
    EXPECT_EQ(executor.inner.tx, nullptr);
    EXPECT_EQ(executor.inner.rx, nullptr);
}
} /* namespace */
