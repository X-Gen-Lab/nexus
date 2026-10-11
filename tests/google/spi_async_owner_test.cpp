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

/** \brief Mock the wire boundary while exercising the real polling bridge. */
class PollingProvider {
  public:
    MOCK_METHOD(nx_result_t, transfer,
                (const uint8_t*, uint8_t*, size_t, nx_time_us_t, size_t*));
};

/** \brief A child owns its CS identity and points at one shared controller. */
struct PollingEndpoint {
    PollingProvider* controller;
};

/** \brief Poll only after the bridge has proved the endpoint's controller. */
nx_result_t polling_transfer(void* context, const uint8_t* tx, uint8_t* rx,
                             size_t length, nx_time_us_t deadline,
                             size_t* transferred) {
    const auto* endpoint = static_cast<PollingEndpoint*>(context);
    return endpoint->controller->transfer(tx, rx, length, deadline,
                                          transferred);
}

/** \brief Distinct endpoints may share the same controller identity. */
bool polling_on_port(const void* context, const nx_spi_port_t* port) {
    return static_cast<const PollingEndpoint*>(context)->controller ==
           port->context;
}

/** \brief Keep the polling mode free of asynchronous requirements. */
nx_spi_endpoint_ops_t polling_endpoint_operations() {
    nx_spi_endpoint_ops_t ops{};
    ops.transfer = polling_transfer;
    ops.on_port = polling_on_port;
    return ops;
}
const auto polling_endpoint_ops = polling_endpoint_operations();

TEST(SPIPollingOwner, RejectsForeignEndpointWithoutWireEffects) {
    StrictMock<PollingProvider> first;
    StrictMock<PollingProvider> other;
    const nx_spi_port_t bus{&bus_ops, &first};
    PollingEndpoint foreign{&other};
    const nx_spi_endpoint_t wrong{&polling_endpoint_ops, &foreign};
    nx_spi_owner_executor_t executor{};
    const auto bridge = nx_spi_owner_executor_port(&executor, &bus);
    uint8_t tx[] = {1, 2};
    uint8_t rx[] = {0, 0};
    nx_spi_owner_operation_t operation{&wrong, tx, rx, sizeof(tx)};
    EXPECT_CALL(other, transfer(tx, rx, sizeof(tx), 1000, ::testing::_))
        .Times(0);
    EXPECT_EQ(bridge.start(bridge.context, &operation, 1000), NX_ERROR_INVALID);
    EXPECT_FALSE(executor.pending);
    EXPECT_EQ(rx[0], 0U);
    EXPECT_EQ(rx[1], 0U);
}

TEST(SPIPollingOwner, AllowsDistinctChipSelectsOnBoundController) {
    StrictMock<PollingProvider> provider;
    const nx_spi_port_t bus{&bus_ops, &provider};
    PollingEndpoint first{&provider};
    PollingEndpoint second{&provider};
    const nx_spi_endpoint_t first_face{&polling_endpoint_ops, &first};
    const nx_spi_endpoint_t second_face{&polling_endpoint_ops, &second};
    nx_spi_owner_executor_t executor{};
    const auto bridge = nx_spi_owner_executor_port(&executor, &bus);
    uint8_t tx[] = {1, 2};
    uint8_t rx[] = {0, 0};
    for (const auto* face : {&first_face, &second_face}) {
        nx_spi_owner_operation_t operation{face, tx, rx, sizeof(tx)};
        EXPECT_CALL(provider, transfer(tx, rx, sizeof(tx), 1000, ::testing::_))
            .WillOnce(::testing::DoAll(::testing::SetArgPointee<4>(sizeof(tx)),
                                       Return(NX_SUCCESS)));
        ASSERT_EQ(bridge.start(bridge.context, &operation, 1000), NX_SUCCESS);
        EXPECT_TRUE(executor.pending);
        EXPECT_EQ(bridge.start(bridge.context, &operation, 1000),
                  NX_ERROR_BUSY);
        nx_result_t result = NX_ERROR_IO;
        size_t transferred = 0;
        ASSERT_EQ(bridge.service(bridge.context, &result, &transferred),
                  NX_SUCCESS);
        EXPECT_EQ(result, NX_SUCCESS);
        EXPECT_EQ(transferred, sizeof(tx));
        EXPECT_FALSE(executor.pending);
    }
}

TEST(SPIPollingOwner, RejectsIncompleteBindingAndMissingIdentityMethod) {
    nx_spi_owner_executor_t executor{};
    EXPECT_EQ(nx_spi_owner_executor_port(nullptr, nullptr).start, nullptr);
    EXPECT_EQ(nx_spi_owner_executor_port(&executor, nullptr).start, nullptr);
    StrictMock<PollingProvider> provider;
    const nx_spi_port_t bus{&bus_ops, &provider};
    auto methods = polling_endpoint_ops;
    methods.on_port = nullptr;
    PollingEndpoint device{&provider};
    const nx_spi_endpoint_t endpoint{&methods, &device};
    const auto bridge = nx_spi_owner_executor_port(&executor, &bus);
    uint8_t tx = 1;
    nx_spi_owner_operation_t operation{&endpoint, &tx, nullptr, 1};
    EXPECT_EQ(bridge.start(bridge.context, &operation, 1000), NX_ERROR_INVALID);
    EXPECT_EQ(bridge.start(bridge.context, nullptr, 1000), NX_ERROR_INVALID);
    EXPECT_FALSE(executor.pending);
}

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
