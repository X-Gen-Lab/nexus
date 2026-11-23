/**
 * \file            uart_byte_stream_test.cpp
 * \brief           Copied UART bytes retain their inner request until join
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/uart_stream.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <gtest/gtest.h>
#include <vector>
namespace {
class UartByteStream : public testing::Test {
  protected:
    nx_uart_stream_t adapter{};
    std::array<uint8_t, 4> buffer{};
    nx_uart_ops_t ops{};
    nx_uart_port_t port{&ops, this};
    nx_uart_tx_request_t* held{};
    nx_time_us_t now{10U};
    std::deque<uint8_t> incoming;
    std::vector<uint8_t> sent;
    nx_result_t submission{NX_SUCCESS};
    nx_result_t terminal{NX_SUCCESS};
    bool finish{};
    bool quarantine{};
    bool loss{};
    bool inline_error{};
    unsigned submits{};
    unsigned cancels{};
    unsigned stops{};
    static nx_time_us_t clock(void* context) {
        return static_cast<UartByteStream*>(context)->now;
    }
    static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
        auto& self = *static_cast<UartByteStream*>(context);
        ++self.submits;
        if (self.submission != NX_SUCCESS) {
            return self.submission;
        }
        nx_result_t result =
            nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        if (result != NX_SUCCESS) {
            return result;
        }
        if (self.inline_error) {
            nx_request_settle(&request->base, NX_ERROR_IO, 0U);
        } else {
            self.held = request;
        }
        return NX_SUCCESS;
    }
    static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
        auto& self = *static_cast<UartByteStream*>(context);
        ++self.cancels;
        EXPECT_EQ(request, self.held);
        if (self.quarantine) {
            EXPECT_EQ(
                nx_request_transition(&request->base, NX_REQUEST_QUARANTINED),
                NX_SUCCESS);
            return NX_ERROR_IO;
        }
        self.finish = true;
        self.terminal = NX_ERROR_CANCELLED;
        return NX_SUCCESS;
    }
    static void service(void* context) {
        auto& self = *static_cast<UartByteStream*>(context);
        if (self.finish && self.held != nullptr) {
            auto* request = self.held;
            self.held = nullptr;
            if (self.terminal == NX_SUCCESS) {
                self.sent.assign(request->data,
                                 request->data + request->length);
            }
            nx_request_settle(&request->base, self.terminal,
                              self.terminal == NX_SUCCESS ? request->length
                                                          : 0U);
        }
    }
    static nx_result_t read(void* context, uint8_t* data, size_t capacity,
                            size_t* count) {
        auto& self = *static_cast<UartByteStream*>(context);
        *count = std::min(capacity, self.incoming.size());
        for (size_t i = 0U; i < *count; ++i) {
            data[i] = self.incoming.front();
            self.incoming.pop_front();
        }
        return self.loss ? NX_ERROR_OVERFLOW
               : *count  ? NX_SUCCESS
                         : NX_ERROR_EMPTY;
    }
    static nx_result_t stop(void* context) {
        ++static_cast<UartByteStream*>(context)->stops;
        return NX_SUCCESS;
    }
    void SetUp() override {
        ops.submit = submit;
        ops.cancel = cancel;
        ops.service = service;
        ops.read_bytes = read;
        ops.stop = stop;
        ASSERT_EQ(nx_uart_stream_init(&adapter, &port, {clock, this},
                                      buffer.data(), buffer.size()),
                  NX_SUCCESS);
    }
    void TearDown() override {
        if (held != nullptr) {
            quarantine = false;
            EXPECT_EQ(nx_uart_stream_stop(&adapter), NX_SUCCESS);
            EXPECT_EQ(held, nullptr);
        }
    }
    const nx_byte_stream_t* face() {
        return nx_uart_stream_face(&adapter);
    }
    nx_result_t write(const uint8_t* bytes, size_t count, size_t* accepted) {
        return face()->ops->write(face()->context, bytes, count, accepted,
                                  100U);
    }
};

TEST_F(UartByteStream, CopyDoesNotReleaseOrOverwriteTheInnerProviderBorrow) {
    uint8_t source[]{1U, 2U, 3U, 4U, 5U};
    size_t count{};
    ASSERT_EQ(write(source, sizeof(source), &count), NX_SUCCESS);
    EXPECT_EQ(count, buffer.size());
    ASSERT_NE(held, nullptr);
    source[0] = 99U;
    EXPECT_EQ(held->data[0], 1U);
    EXPECT_EQ(write(source, sizeof(source), &count), NX_ERROR_BUSY);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(held->data[0], 1U);
    EXPECT_EQ(nx_uart_stream_service(&adapter), NX_ERROR_BUSY);
    finish = true;
    ASSERT_EQ(nx_uart_stream_service(&adapter), NX_SUCCESS);
    EXPECT_EQ(sent, (std::vector<uint8_t>{1U, 2U, 3U, 4U}));
    EXPECT_EQ(write(source, 1U, &count), NX_SUCCESS);
}

TEST_F(UartByteStream, FailedCancellationRetainsStorageUntilActualSettlement) {
    const uint8_t source[]{1U};
    size_t count{};
    ASSERT_EQ(write(source, sizeof(source), &count), NX_SUCCESS);
    quarantine = true;
    EXPECT_EQ(nx_uart_stream_stop(&adapter), NX_ERROR_IO);
    ASSERT_NE(held, nullptr);
    EXPECT_EQ(nx_request_state(&held->base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_uart_stream_init(&adapter, &port, {clock, this}, buffer.data(),
                                  buffer.size()),
              NX_ERROR_STATE);
    quarantine = false;
    EXPECT_EQ(nx_uart_stream_stop(&adapter), NX_SUCCESS);
    EXPECT_EQ(held, nullptr);
    EXPECT_EQ(stops, 0U);
    EXPECT_EQ(write(source, sizeof(source), &count), NX_ERROR_STATE);
}

TEST_F(UartByteStream, LateAndInlineTxErrorsSurfaceBeforeASecondAdmission) {
    const uint8_t source[]{1U};
    size_t count{};
    ASSERT_EQ(write(source, sizeof(source), &count), NX_SUCCESS);
    terminal = NX_ERROR_IO;
    finish = true;
    EXPECT_EQ(nx_uart_stream_service(&adapter), NX_ERROR_IO);
    EXPECT_EQ(write(source, sizeof(source), &count), NX_ERROR_IO);
    EXPECT_EQ(submits, 1U);
    ASSERT_EQ(nx_uart_stream_stop(&adapter), NX_SUCCESS);
    ASSERT_EQ(nx_uart_stream_init(&adapter, &port, {clock, this}, buffer.data(),
                                  buffer.size()),
              NX_SUCCESS);
    inline_error = true;
    ASSERT_EQ(write(source, sizeof(source), &count), NX_SUCCESS);
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(nx_uart_stream_service(&adapter), NX_ERROR_IO);
}

TEST_F(UartByteStream, RxLossClosesFramingWithoutPublishingACopiedPrefix) {
    incoming = {1U, 2U};
    loss = true;
    uint8_t data[8]{};
    size_t count{42U};
    EXPECT_EQ(
        face()->ops->read(face()->context, data, sizeof(data), &count, 100U),
        NX_ERROR_OVERFLOW);
    EXPECT_EQ(count, 0U);
    const uint8_t source[]{1U};
    EXPECT_EQ(write(source, 1U, &count), NX_ERROR_OVERFLOW);
    EXPECT_EQ(submits, 0U);
}

TEST_F(UartByteStream, EmptySerialQueueIsBusyAndClockRegressionIsNotEof) {
    uint8_t data[8]{};
    size_t count{42U};
    EXPECT_EQ(
        face()->ops->read(face()->context, data, sizeof(data), &count, 100U),
        NX_ERROR_BUSY);
    EXPECT_EQ(count, 0U);
    now = 9U;
    incoming = {1U};
    EXPECT_EQ(
        face()->ops->read(face()->context, data, sizeof(data), &count, 100U),
        NX_ERROR_STATE);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(incoming.size(), 1U);
}

TEST_F(UartByteStream, RejectedSubmissionHasNoNewBorrowAndDoesNotLatchBusy) {
    submission = NX_ERROR_BUSY;
    const uint8_t source[]{1U};
    size_t count{};
    EXPECT_EQ(write(source, 1U, &count), NX_ERROR_BUSY);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(held, nullptr);
    submission = NX_SUCCESS;
    EXPECT_EQ(write(source, 1U, &count), NX_SUCCESS);
    EXPECT_EQ(count, 1U);
}

TEST_F(UartByteStream, InvalidBufferAliasesAndUnsupportedRxRejectWithoutIo) {
    size_t count{42U};
    EXPECT_EQ(write(buffer.data(), 1U, &count), NX_ERROR_INVALID);
    EXPECT_EQ(submits, 0U);
    nx_uart_stream_t other{};
    nx_uart_ops_t unsupported = ops;
    unsupported.read_bytes = nullptr;
    const nx_uart_port_t no_bytes{&unsupported, this};
    EXPECT_EQ(nx_uart_stream_init(&other, &no_bytes, {clock, this},
                                  buffer.data(), buffer.size()),
              NX_ERROR_UNSUPPORTED);
}
} /* namespace */
