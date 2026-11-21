/**
 * \file            common_log_uart_test.cpp
 * \brief           Copied log records preserve real UART settlement ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/log_uart.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>

namespace {
struct UartBoundary {
    nx_uart_tx_request_t* active = nullptr;
    bool drain = false;
    bool quarantine = false;
    nx_result_t terminal = NX_SUCCESS;
    nx_result_t admission = NX_SUCCESS;
    unsigned services = 0;
    unsigned cancels = 0;
    static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
        auto& self = *static_cast<UartBoundary*>(context);
        if (self.admission != NX_SUCCESS)
            return self.admission;
        const auto result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        if (result == NX_SUCCESS)
            self.active = request;
        return result;
    }
    static nx_result_t cancel(void* context, nx_uart_tx_request_t*) {
        ++static_cast<UartBoundary*>(context)->cancels;
        return NX_SUCCESS;
    }
    static void service(void* context) {
        auto& self = *static_cast<UartBoundary*>(context);
        ++self.services;
        if (self.active == nullptr)
            return;
        if (self.quarantine) {
            if (nx_request_state(&self.active->base) !=
                NX_REQUEST_QUARANTINED) {
                EXPECT_EQ(nx_request_transition(&self.active->base,
                                                NX_REQUEST_QUARANTINED),
                          NX_SUCCESS);
            }
            return;
        }
        if (self.drain) {
            auto* request = self.active;
            self.active = nullptr;
            nx_request_settle(&request->base, self.terminal, request->length);
        }
    }
    static constexpr nx_uart_ops_t operations{
        submit,  nullptr, cancel,  service, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr};
    nx_uart_port_t port{&operations, this};
};

TEST(CommonLogUart, AdmissionCopiesRecordAndBusyNeverOverwritesBorrowedBytes) {
    UartBoundary uart;
    nx_log_uart_t adapter{};
    std::array<uint8_t, 16> staging{};
    ASSERT_EQ(
        nx_log_uart_init(&adapter, &uart.port, staging.data(), staging.size()),
        NX_SUCCESS);
    auto sink = nx_log_uart_sink(&adapter);
    std::array<char, 5> input{'h', 'e', 'l', 'l', 'o'};
    ASSERT_EQ(
        sink.write(sink.context, NX_LOG_INFO, input.data(), input.size(), 100),
        NX_SUCCESS);
    input.fill('x');
    ASSERT_NE(uart.active, nullptr);
    EXPECT_EQ(std::memcmp(uart.active->data, "hello", 5), 0);
    EXPECT_EQ(sink.write(sink.context, NX_LOG_INFO, "other", 5, 100),
              NX_ERROR_BUSY);
    EXPECT_EQ(std::memcmp(uart.active->data, "hello", 5), 0);
    EXPECT_EQ(adapter.dropped, 1U);
    uart.drain = true;
    EXPECT_EQ(nx_log_uart_service(&adapter), NX_SUCCESS);
    EXPECT_FALSE(adapter.pending);
    EXPECT_EQ(adapter.completed, 1U);
    EXPECT_EQ(sink.write(sink.context, NX_LOG_INFO, "next", 4, 120),
              NX_SUCCESS);
}

TEST(CommonLogUart, StopRetainsQuarantinedStagingAndNeverStopsSharedRx) {
    UartBoundary uart;
    nx_log_uart_t adapter{};
    std::array<uint8_t, 8> staging{};
    ASSERT_EQ(
        nx_log_uart_init(&adapter, &uart.port, staging.data(), staging.size()),
        NX_SUCCESS);
    auto sink = nx_log_uart_sink(&adapter);
    ASSERT_EQ(sink.write(sink.context, NX_LOG_ERROR, "fault", 5, 10),
              NX_SUCCESS);
    uart.quarantine = true;
    EXPECT_EQ(nx_log_uart_stop(&adapter), NX_ERROR_BUSY);
    EXPECT_TRUE(adapter.pending);
    EXPECT_EQ(uart.cancels, 1U);
    EXPECT_EQ(nx_log_uart_stop(&adapter), NX_ERROR_BUSY);
    EXPECT_EQ(uart.cancels, 1U);
    EXPECT_EQ(sink.write(sink.context, NX_LOG_INFO, "x", 1, 10),
              NX_ERROR_STATE);
    uart.quarantine = false;
    uart.drain = true;
    uart.terminal = NX_ERROR_TIMEOUT;
    EXPECT_EQ(nx_log_uart_service(&adapter), NX_ERROR_TIMEOUT);
    EXPECT_FALSE(adapter.pending);
    EXPECT_EQ(nx_log_uart_stop(&adapter), NX_SUCCESS);
}

TEST(CommonLogUart, RejectionAndOversizeNeverCreateBorrow) {
    UartBoundary uart;
    nx_log_uart_t adapter{};
    std::array<uint8_t, 4> staging{};
    ASSERT_EQ(
        nx_log_uart_init(&adapter, &uart.port, staging.data(), staging.size()),
        NX_SUCCESS);
    auto sink = nx_log_uart_sink(&adapter);
    EXPECT_EQ(sink.write(sink.context, NX_LOG_INFO, "large", 5, 100),
              NX_ERROR_OVERFLOW);
    EXPECT_EQ(uart.active, nullptr);
    uart.admission = NX_ERROR_TIMEOUT;
    EXPECT_EQ(sink.write(sink.context, NX_LOG_INFO, "ok", 2, 100),
              NX_ERROR_TIMEOUT);
    EXPECT_FALSE(adapter.pending);
    EXPECT_EQ(uart.active, nullptr);
    uart.admission = NX_SUCCESS;
    EXPECT_EQ(sink.write(sink.context, NX_LOG_INFO, "ok", 2, 200), NX_SUCCESS);
}
} /* namespace */
