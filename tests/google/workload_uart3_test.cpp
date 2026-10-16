/**
 * \file            workload_uart3_test.cpp
 * \brief           UART workload retains its selected instance through drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" int nx_workload_uart3_main(void);

namespace {
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

/** \brief Mock only hardware lifecycle and the selected UART provider. */
class Ports {
  public:
    MOCK_METHOD(nx_platform_start_result_t, start, ());
    MOCK_METHOD(nx_result_t, stop, ());
    MOCK_METHOD(const nx_uart_port_t*, factory, (nx_uart_id_t id));
    MOCK_METHOD(nx_result_t, submit, (nx_uart_tx_request_t * request));
    MOCK_METHOD(void, service, ());
};
Ports* ports;
int uart3_context;

/** \brief Submit through the immutable instance selected by the assembly. */
nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    EXPECT_EQ(context, &uart3_context);
    return ports->submit(request);
}

/** \brief Service retains the same context until settlement. */
void service(void* context) {
    EXPECT_EQ(context, &uart3_context);
    ports->service();
}

/** \brief Supply only the provider operations used by this workload. */
nx_uart_ops_t operations() {
    nx_uart_ops_t result{};
    result.submit = submit;
    result.service = service;
    return result;
}
const nx_uart_ops_t uart3_operations = operations();
const nx_uart_port_t uart3 = {&uart3_operations, &uart3_context};

class UART3Workload : public ::testing::Test {
  protected:
    StrictMock<Ports> hardware;
    void SetUp() override {
        ports = &hardware;
    }
    void TearDown() override {
        ports = nullptr;
    }
};

TEST_F(UART3Workload, StartsSelectedInstanceAndStopsOnlyAfterSettlement) {
    nx_uart_tx_request_t* active = nullptr;
    InSequence sequence;
    EXPECT_CALL(hardware, start())
        .WillOnce(
            Return(nx_platform_start_result_t{NX_SUCCESS, NX_SUCCESS, 0}));
    EXPECT_CALL(hardware, factory(NEXUS_WORKLOAD_UART_ID))
        .WillOnce(Return(&uart3));
    EXPECT_CALL(hardware, submit(_))
        .WillOnce(Invoke([&](nx_uart_tx_request_t* request) {
            EXPECT_EQ(request->length, 2u);
            EXPECT_EQ(request->data[0], 'N');
            EXPECT_EQ(request->data[1], 'X');
            EXPECT_EQ(request->base.deadline, 101000u);
            active = request;
            return nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        }));
    EXPECT_CALL(hardware, service()).WillOnce(Invoke([&] {
        EXPECT_EQ(nx_request_state(&active->base), NX_REQUEST_ACTIVE);
    }));
    EXPECT_CALL(hardware, service()).WillOnce(Invoke([&] {
        nx_request_settle(&active->base, NX_SUCCESS, 2u);
    }));
    EXPECT_CALL(hardware, stop()).WillOnce(Invoke([&] {
        EXPECT_EQ(nx_request_state(&active->base), NX_REQUEST_SETTLED);
        return NX_SUCCESS;
    }));
    EXPECT_EQ(nx_workload_uart3_main(), 0);
}

TEST_F(UART3Workload, FailedStartupDoesNotTouchAnUnusablePort) {
    EXPECT_CALL(hardware, start())
        .WillOnce(
            Return(nx_platform_start_result_t{NX_ERROR_IO, NX_SUCCESS, 0}));
    EXPECT_EQ(nx_workload_uart3_main(), 1);
}

TEST_F(UART3Workload, RejectedSubmitDoesNotServiceOrAssumePlatformStopped) {
    EXPECT_CALL(hardware, start())
        .WillOnce(
            Return(nx_platform_start_result_t{NX_SUCCESS, NX_SUCCESS, 0}));
    EXPECT_CALL(hardware, factory(NEXUS_WORKLOAD_UART_ID))
        .WillOnce(Return(&uart3));
    EXPECT_CALL(hardware, submit(_)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_workload_uart3_main(), 5);
}
} /* namespace */

extern "C" {
/** \brief Exercise the public typed factory without any legacy binding alias.
 */
const nx_uart_port_t* nx_factory_uart(nx_uart_id_t id) {
    return ports->factory(id);
}

/** \brief Record hardware startup without modeling the production owner. */
nx_platform_start_result_t nx_platform_start(void) {
    return ports->start();
}

/** \brief Observe release after the workload drains its provider. */
nx_result_t nx_platform_stop(void) {
    return ports->stop();
}

/** \brief Inject one fixed monotonic epoch for request deadline assertions. */
nx_time_us_t nx_time_now_us(void) {
    return 1000u;
}
}
