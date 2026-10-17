/**
 * \file            uart_formats_test.cpp
 * \brief           UART formats and independent nine-bit request ownership.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/uart_word.h"
#include "uart_word_provider.h"
}
#include "nexus/io/native/model.h"
#include <gtest/gtest.h>
#include <limits>

TEST(UartFormat, ExplicitDataBitsExcludeTheParityBit) {
    EXPECT_EQ(nx_uart_format_validate(
                  {7U, NX_UART_PARITY_EVEN, NX_UART_STOP_1, NX_UART_FLOW_NONE}),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_format_validate({8U, NX_UART_PARITY_ODD, NX_UART_STOP_2,
                                       NX_UART_FLOW_RTS_CTS}),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_format_validate(
                  {9U, NX_UART_PARITY_NONE, NX_UART_STOP_1, NX_UART_FLOW_CTS}),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_format_validate(
                  {9U, NX_UART_PARITY_ODD, NX_UART_STOP_1, NX_UART_FLOW_NONE}),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_uart_format_validate(
                  {7U, NX_UART_PARITY_NONE, NX_UART_STOP_1, NX_UART_FLOW_NONE}),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_uart_format_validate({8U, NX_UART_PARITY_NONE,
                                       static_cast<nx_uart_stop_t>(3),
                                       NX_UART_FLOW_NONE}),
              NX_ERROR_INVALID);
}

class UartWord : public testing::Test {
  protected:
    uint16_t received[4] = {};
    uint16_t log[4] = {};
    uint16_t payload[3] = {0x1FFU, 0x100U, 0x55U};
    nx_native_uart_word_state_t state = {};
    nx_uart_word_port_t port = {&nx_native_uart_word_ops, &state};
    nx_uart_word_tx_request_t request = {};
    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 10U), NX_SUCCESS);
        ASSERT_EQ(nx_native_uart_word_initialize(&state, received, 4U, log, 4U),
                  NX_SUCCESS);
        nx_request_initialize(&request.base);
    }
    void prepare(size_t words = 3U) {
        ASSERT_EQ(nx_uart_word_tx_prepare(&request, payload, words, 1000U),
                  NX_SUCCESS);
    }
};

TEST_F(UartWord, FullNineBitsAndWireTcRemainSeparateFromLoadedWords) {
    prepare();
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
    for (size_t i = 0; i < 3U; ++i) {
        nx_native_uart_word_irq(&state);
    }
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(log[0], 0x1FFU);
    EXPECT_EQ(log[1], 0x100U);
    EXPECT_EQ(log[2], 0x55U);
    nx_native_uart_word_irq(&state);
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 3U);
    EXPECT_EQ(state.active, nullptr);
}

TEST_F(UartWord, RejectsOverflowMisalignmentAndDescriptorOverlap) {
    const auto* misaligned = reinterpret_cast<const uint16_t*>(
        reinterpret_cast<const unsigned char*>(payload) + 1U);
    EXPECT_EQ(nx_uart_word_tx_prepare(&request, misaligned, 1U, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_uart_word_tx_prepare(
                  &request, payload, std::numeric_limits<size_t>::max(), 1000U),
              NX_ERROR_INVALID);
    auto* self = reinterpret_cast<const uint16_t*>(&request);
    EXPECT_EQ(nx_uart_word_tx_prepare(&request, self, 1U, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
}

TEST_F(UartWord, RejectedPayloadDoesNotRetainReferencesOrTouchTheWire) {
    prepare();
    payload[1] = 0x200U;
    EXPECT_EQ(nx_uart_word_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(state.logged, 0U);
}

TEST_F(UartWord, CancelRetainsTheBorrowUntilPhysicalDrain) {
    prepare();
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
    nx_native_uart_word_irq(&state);
    state.hold_drain = true;
    ASSERT_EQ(nx_uart_word_port_cancel(&port, &request), NX_SUCCESS);
    nx_uart_word_port_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(nx_uart_word_tx_prepare(&request, payload, 1U, 1000U),
              NX_ERROR_STATE);
    state.hold_drain = false;
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(state.active, nullptr);
    nx_native_uart_word_irq(&state);
    EXPECT_EQ(state.logged, 1U);
}

TEST_F(UartWord, ReceivePreservesNinthBitAndReportsBoundedOverflow) {
    for (uint16_t i = 0; i < 5U; ++i) {
        nx_native_uart_word_receive(&state, static_cast<uint16_t>(0x100U + i));
    }
    uint16_t output[4] = {};
    size_t count = 99U;
    EXPECT_EQ(nx_uart_word_port_read(&port, output, 4U, &count),
              NX_ERROR_OVERFLOW);
    ASSERT_EQ(count, 4U);
    EXPECT_EQ(output[0], 0x100U);
    EXPECT_EQ(output[3], 0x103U);
    EXPECT_EQ(nx_uart_word_port_read(&port, output, 4U, &count),
              NX_ERROR_EMPTY);
    EXPECT_EQ(count, 0U);
}

TEST_F(UartWord, ExpiredAdmissionAndOtherInstanceLeaveBothPortsIdle) {
    prepare();
    ASSERT_EQ(nx_native_clock_advance(1000U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_word_port_submit(&port, &request), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    nx_native_uart_word_state_t other = {};
    uint16_t other_rx[2] = {};
    ASSERT_EQ(nx_native_uart_word_initialize(&other, other_rx, 2U, nullptr, 0U),
              NX_SUCCESS);
    nx_native_uart_word_receive(&other, 0x1ABU);
    EXPECT_EQ(state.count, 0U);
    EXPECT_EQ(other.count, 1U);
}

TEST_F(UartWord, ReadRejectsCountAliasingProviderMetadata) {
    nx_native_uart_word_receive(&state, 0x1ABU);
    uint16_t output = 0U;
    EXPECT_EQ(nx_uart_word_port_read(&port, &output, 1U, &state.count),
              NX_ERROR_INVALID);
    EXPECT_EQ(state.count, 1U);
    EXPECT_EQ(output, 0U);
}

TEST_F(UartWord, ReadRejectsDestinationAliasingTheActivePayload) {
    prepare();
    ASSERT_EQ(nx_uart_word_port_submit(&port, &request), NX_SUCCESS);
    nx_native_uart_word_receive(&state, 0x11U);
    size_t count = 99U;
    EXPECT_EQ(nx_uart_word_port_read(&port, payload, 3U, &count),
              NX_ERROR_INVALID);
    EXPECT_EQ(payload[0], 0x1FFU);
    EXPECT_EQ(state.count, 1U);
    ASSERT_EQ(nx_uart_word_port_cancel(&port, &request), NX_SUCCESS);
    nx_uart_word_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}
