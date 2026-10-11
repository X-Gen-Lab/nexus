/**
 * \file            uart_rs485_test.cpp
 * \brief           RS485 timing and retained-loan external port contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/native/model.h"
#include "nexus/io/uart_rs485.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

class Rs485Ports {
  public:
    MOCK_METHOD(nx_result_t, Validate, (const nx_uart_tx_request_t*));
    MOCK_METHOD(nx_result_t, Write, (uint32_t set, uint32_t reset));
    MOCK_METHOD(nx_result_t, Start, (nx_uart_tx_request_t*));
    MOCK_METHOD(nx_result_t, Cancel, (nx_uart_tx_request_t*));
    MOCK_METHOD(void, Service, ());
    MOCK_METHOD(nx_result_t, Stop, ());
};

static nx_result_t validate(void* context,
                            const nx_uart_tx_request_t* request) {
    return static_cast<Rs485Ports*>(context)->Validate(request);
}
static nx_result_t write(void* context, uint32_t set, uint32_t reset) {
    return static_cast<Rs485Ports*>(context)->Write(set, reset);
}
static nx_result_t start(void* context, nx_uart_tx_request_t* request) {
    return static_cast<Rs485Ports*>(context)->Start(request);
}
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    return static_cast<Rs485Ports*>(context)->Cancel(request);
}
static void service(void* context) {
    static_cast<Rs485Ports*>(context)->Service();
}
static nx_result_t stop(void* context) {
    return static_cast<Rs485Ports*>(context)->Stop();
}

class UartRs485 : public testing::Test {
  protected:
    testing::NiceMock<Rs485Ports> hardware;
    nx_uart_ops_t transport_ops = {};
    nx_gpio_ops_t gpio_ops = {};
    const nx_uart_port_t transport = {&transport_ops, &hardware};
    const nx_gpio_port_t gpio = {&gpio_ops, &hardware};
    const nx_uart_rs485_config_t config = {&transport, &gpio, validate, 8U,
                                           10U,        20U,   true};
    nx_uart_rs485_state_t state = {};
    const nx_uart_port_t port = {&nx_uart_rs485_ops, &state};
    nx_uart_tx_request_t request = {};
    nx_uart_tx_request_t* executing = nullptr;
    uint8_t payload[3] = {1U, 2U, 3U};
    bool de = false;

    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        transport_ops.start_admitted = start;
        transport_ops.cancel = cancel;
        transport_ops.service = service;
        transport_ops.stop = stop;
        gpio_ops.write = write;
        ON_CALL(hardware, Validate(testing::_))
            .WillByDefault(testing::Return(NX_SUCCESS));
        ON_CALL(hardware, Write(testing::_, testing::_))
            .WillByDefault(
                testing::Invoke([this](uint32_t set, uint32_t reset) {
                    if ((set & 8U) != 0U) {
                        de = true;
                    }
                    if ((reset & 8U) != 0U) {
                        de = false;
                    }
                    return NX_SUCCESS;
                }));
        ON_CALL(hardware, Start(testing::_))
            .WillByDefault(testing::Invoke([this](nx_uart_tx_request_t* inner) {
                EXPECT_TRUE(de);
                EXPECT_EQ(nx_request_state(&inner->base), NX_REQUEST_QUEUED);
                executing = inner;
                return nx_request_transition(&inner->base, NX_REQUEST_ACTIVE);
            }));
        ON_CALL(hardware, Cancel(testing::_))
            .WillByDefault(testing::Return(NX_SUCCESS));
        ON_CALL(hardware, Stop()).WillByDefault(testing::Return(NX_SUCCESS));
        ASSERT_EQ(nx_uart_rs485_initialize(&state, &config), NX_SUCCESS);
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_uart_tx_prepare(&request, payload, sizeof(payload), 1000U),
                  NX_SUCCESS);
    }
    void advance(nx_time_us_t delta) {
        ASSERT_EQ(nx_native_clock_advance(delta), NX_SUCCESS);
        nx_uart_port_service(&port);
    }
    void complete(nx_result_t result, size_t count) {
        ASSERT_NE(executing, nullptr);
        auto* inner = executing;
        executing = nullptr;
        nx_request_settle(&inner->base, result, count);
        nx_uart_port_service(&port);
    }
};

TEST_F(UartRs485, RejectsInvalidTransportBeforeDirectionOrBorrow) {
    EXPECT_CALL(hardware, Validate(&request))
        .WillOnce(testing::Return(NX_ERROR_PERMISSION));
    EXPECT_CALL(hardware, Write(testing::_, testing::_)).Times(0);
    EXPECT_CALL(hardware, Start(testing::_)).Times(0);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_FALSE(de);
}

TEST_F(UartRs485, SetupThenTrueTransportSettlementThenMinimumHold) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_TRUE(de);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUEUED);
    advance(9U);
    EXPECT_EQ(executing, nullptr);
    advance(1U);
    ASSERT_NE(executing, nullptr);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    complete(NX_SUCCESS, sizeof(payload));
    EXPECT_TRUE(de);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.inner.data, nullptr);
    advance(19U);
    EXPECT_TRUE(de);
    advance(1U);
    EXPECT_FALSE(de);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(payload));
    EXPECT_EQ(state.active, nullptr);
}

TEST_F(UartRs485, CancelDuringSetupNeverStartsUartAndWithdrawsDirection) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_CALL(hardware, Start(testing::_)).Times(0);
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_FALSE(de);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(UartRs485, SetupConsumesAbsoluteDeadlineAndDoesNotStartExpiredTransfer) {
    ASSERT_EQ(nx_uart_tx_prepare(&request, payload, sizeof(payload), 105U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_CALL(hardware, Start(testing::_)).Times(0);
    advance(10U);
    EXPECT_FALSE(de);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
}

TEST_F(UartRs485, ActiveCancelAndFailedDrainRetainOuterBorrowAndDirection) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    advance(10U);
    ASSERT_NE(executing, nullptr);
    EXPECT_CALL(hardware, Cancel(executing))
        .WillOnce(testing::Return(NX_SUCCESS));
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_request_transition(&executing->base, NX_REQUEST_DRAINING),
              NX_SUCCESS);
    ASSERT_EQ(nx_request_transition(&executing->base, NX_REQUEST_QUARANTINED),
              NX_SUCCESS);
    advance(500U);
    EXPECT_TRUE(de);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(state.inner.data, payload);
    complete(NX_ERROR_CANCELLED, 0U);
    advance(20U);
    EXPECT_FALSE(de);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}

TEST_F(UartRs485, FailedDirectionWithdrawalQuarantinesUntilActualRetry) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    advance(10U);
    complete(NX_SUCCESS, sizeof(payload));
    EXPECT_CALL(hardware, Write(0U, 8U)).WillOnce(testing::Return(NX_ERROR_IO));
    advance(20U);
    EXPECT_TRUE(de);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(state.inner.data, nullptr);
    EXPECT_CALL(hardware, Write(0U, 8U))
        .WillOnce(testing::Invoke([this](uint32_t, uint32_t) {
            de = false;
            return NX_SUCCESS;
        }));
    nx_uart_port_service(&port);
    EXPECT_FALSE(de);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}

TEST_F(UartRs485, StopClosesAdmissionAndDrainsDirectionBeforeTransportStop) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    advance(10U);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_ERROR_BUSY);
    nx_uart_tx_request_t second = {};
    nx_request_initialize(&second.base);
    ASSERT_EQ(nx_uart_tx_prepare(&second, payload, sizeof(payload), 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &second), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_READY);
    EXPECT_TRUE(de);
    complete(NX_ERROR_CANCELLED, 0U);
    advance(20U);
    EXPECT_CALL(hardware, Stop()).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    EXPECT_FALSE(de);
}

TEST_F(UartRs485, SuccessAlreadySettledInTransportWinsLaterCancel) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    advance(10U);
    auto* inner = executing;
    executing = nullptr;
    nx_request_settle(&inner->base, NX_SUCCESS, sizeof(payload));
    EXPECT_CALL(hardware, Cancel(testing::_)).Times(0);
    EXPECT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    advance(20U);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

class NativeUartRs485 : public testing::Test {
  protected:
    uint8_t rx[8] = {};
    uint8_t log[8] = {};
    const nx_native_uart_config_t uart_config = {
        NX_UART_RX_BYTES, rx, sizeof(rx), log, sizeof(log), false};
    const nx_uart_rs485_config_t config = {nx_native_uart,
                                           nx_native_gpio,
                                           nx_native_uart_tx_validate,
                                           8U,
                                           10U,
                                           20U,
                                           true};
    nx_uart_rs485_state_t adapter = {};
    const nx_uart_port_t port = {&nx_uart_rs485_ops, &adapter};
    nx_uart_tx_request_t request = {};
    uint8_t byte = 0x42U;
    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        ASSERT_EQ(nx_native_gpio_configure(8U, 0U), NX_SUCCESS);
        ASSERT_EQ(nx_native_uart_configure(&uart_config), NX_SUCCESS);
        ASSERT_EQ(nx_uart_rs485_initialize(&adapter, &config), NX_SUCCESS);
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_uart_tx_prepare(&request, &byte, 1U, 1000U), NX_SUCCESS);
    }
};

TEST_F(NativeUartRs485,
       NativeWireTcAndSelectedDirectionHaveIndependentLifetimes) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_native_gpio_output(), 8U);
    ASSERT_EQ(nx_native_clock_advance(10U), NX_SUCCESS);
    nx_uart_port_service(&port);
    nx_native_uart_irq_step();
    nx_uart_port_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(log[0], byte);
    nx_native_uart_irq_step();
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_native_gpio_output(), 8U);
    ASSERT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_native_gpio_output(), 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 1U);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}

TEST_F(NativeUartRs485, NativeHeldDrainRetainsOuterDirectionUntilRealRetry) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(10U), NX_SUCCESS);
    nx_uart_port_service(&port);
    nx_native_uart_fault(false, true);
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_native_gpio_output(), 8U);
    EXPECT_EQ(adapter.inner.data, &byte);
    nx_native_uart_fault(false, false);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_native_gpio_output(), 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(adapter.inner.data, nullptr);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}

TEST_F(UartRs485, InitRejectsConfigOverlappingControlBeforeAnyDirectionStore) {
    nx_uart_rs485_state_t fresh = {};
    static_assert(sizeof(fresh) - offsetof(nx_uart_rs485_state_t, inner) >=
                  sizeof(config));
    auto* destination = reinterpret_cast<unsigned char*>(&fresh) +
                        offsetof(nx_uart_rs485_state_t, inner);
    std::memcpy(destination, &config, sizeof(config));
    auto* overlap =
        reinterpret_cast<const nx_uart_rs485_config_t*>(&fresh.inner);
    EXPECT_CALL(hardware, Write(testing::_, testing::_)).Times(0);
    EXPECT_EQ(nx_uart_rs485_initialize(&fresh, overlap), NX_ERROR_INVALID);
    EXPECT_FALSE(fresh.initialized);
}

TEST_F(UartRs485, InvalidDirectionMaskRejectsBeforeStartupStore) {
    nx_uart_rs485_state_t fresh = {};
    nx_uart_rs485_config_t bad = config;
    bad.mask = 3U;
    EXPECT_CALL(hardware, Write(testing::_, testing::_)).Times(0);
    EXPECT_EQ(nx_uart_rs485_initialize(&fresh, &bad), NX_ERROR_INVALID);
}

TEST_F(UartRs485, FailedTransportStartWithdrawsDirectionBeforeSettlingOuter) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_CALL(hardware, Start(testing::_))
        .WillOnce(testing::Return(NX_ERROR_IO));
    advance(10U);
    EXPECT_FALSE(de);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(state.inner.data, nullptr);
}
