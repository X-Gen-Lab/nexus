/**
 * \file            native_async_test.cpp
 *
 * \brief           Native finite async SPI and concrete block producer loans.
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
#include "provider.h"
#include <cstring>
#include <gtest/gtest.h>

namespace {

/** \brief Use a fresh model clock with explicit provider-owned static states.
 */
class NativeAsync : public testing::Test {
  protected:
    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 0), NX_SUCCESS);
    }
};

/** \brief Record whether a provider notifies after releasing metadata guards.
 */
struct WakeFacts {
    unsigned calls = 0;
    bool masked = false;
};

/** \brief A bounded model sink preserves observable publisher context. */
nx_result_t record_wake(void* context) {
    auto* facts = static_cast<WakeFacts*>(context);
    ++facts->calls;
    facts->masked = facts->masked || nx_arch_irq_is_masked();
    return NX_SUCCESS;
}

TEST_F(NativeAsync, SPIBorrowWaitsForWireCompletionAndOtherEndpointRejects) {
    nx_native_spi_state_t controller = {};
    nx_native_spi_endpoint_state_t first = {};
    nx_native_spi_endpoint_state_t second = {};
    const nx_spi_port_t bus = {&nx_native_spi_ops, &controller};
    const nx_spi_endpoint_t first_api = {&nx_native_spi_endpoint_ops, &first};
    const nx_spi_endpoint_t second_api = {&nx_native_spi_endpoint_ops, &second};
    ASSERT_EQ(nx_native_spi_port_configure_instance(&controller), NX_SUCCESS);
    ASSERT_EQ(nx_native_spi_configure_instance(&first, &controller, nullptr, 0),
              NX_SUCCESS);
    ASSERT_EQ(
        nx_native_spi_configure_instance(&second, &controller, nullptr, 0),
        NX_SUCCESS);
    nx_native_spi_model_async_configure(&bus, false);
    WakeFacts facts;
    const nx_irq_wake_t wake = {&facts, record_wake, false};
    ASSERT_EQ(nx_spi_port_attach_wake(&bus, &wake, 5), NX_SUCCESS);
    uint8_t tx[] = {17, 29};
    uint8_t rx[2] = {};
    nx_spi_request_t request = {};
    nx_spi_request_t rejected = {};
    nx_request_initialize(&request.base);
    nx_request_initialize(&rejected.base);
    ASSERT_EQ(nx_spi_request_prepare(&request, tx, rx, 2, 100), NX_SUCCESS);
    ASSERT_EQ(nx_spi_request_prepare(&rejected, tx, nullptr, 2, 100),
              NX_SUCCESS);
    ASSERT_EQ(nx_spi_endpoint_submit(&first_api, &request), NX_SUCCESS);
    EXPECT_TRUE(nx_native_spi_model_cs_active(&first_api));
    EXPECT_EQ(facts.calls, 0u);
    EXPECT_EQ(nx_spi_endpoint_submit(&second_api, &rejected), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&rejected.base), NX_REQUEST_READY);
    nx_native_spi_model_irq_step(&bus);
    nx_native_spi_model_irq_step(&bus);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(facts.calls, 0u);
    EXPECT_TRUE(nx_native_spi_model_cs_active(&first_api));
    nx_native_spi_model_irq_step(&bus);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_FALSE(nx_native_spi_model_cs_active(&first_api));
    EXPECT_EQ(facts.calls, 1u);
    EXPECT_FALSE(facts.masked);
    EXPECT_EQ(rx[0], 17);
    EXPECT_EQ(rx[1], 29);
    tx[0] = 99;
    nx_native_spi_model_irq_step(&bus);
    EXPECT_EQ(rx[0], 17);
    EXPECT_EQ(nx_spi_port_stop(&bus), NX_SUCCESS);
    nx_native_spi_model_irq_step(&bus);
    EXPECT_EQ(facts.calls, 1u);
}

TEST_F(NativeAsync, SPIQuarantineRetainsBorrowAndStopUntilDrain) {
    nx_native_spi_state_t controller = {};
    nx_native_spi_endpoint_state_t endpoint = {};
    const nx_spi_port_t bus = {&nx_native_spi_ops, &controller};
    const nx_spi_endpoint_t endpoint_api = {&nx_native_spi_endpoint_ops,
                                            &endpoint};
    ASSERT_EQ(nx_native_spi_port_configure_instance(&controller), NX_SUCCESS);
    ASSERT_EQ(
        nx_native_spi_configure_instance(&endpoint, &controller, nullptr, 0),
        NX_SUCCESS);
    nx_native_spi_model_async_configure(&bus, false);
    nx_native_spi_model_drain_hold(&bus, true);
    uint8_t tx[] = {1, 2};
    nx_spi_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_spi_request_prepare(&request, tx, nullptr, 2, 100),
              NX_SUCCESS);
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint_api, &request), NX_SUCCESS);
    nx_native_spi_model_irq_step(&bus);
    EXPECT_EQ(nx_spi_port_cancel(&bus, &request), NX_SUCCESS);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_spi_port_stop(&bus), NX_ERROR_BUSY);
    EXPECT_EQ(nx_spi_request_prepare(&request, tx, nullptr, 2, 100),
              NX_ERROR_STATE);
    nx_native_spi_model_drain_hold(&bus, false);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    nx_result_t result = NX_SUCCESS;
    size_t count = 0;
    ASSERT_EQ(nx_request_result(&request.base, &result, &count), NX_SUCCESS);
    EXPECT_EQ(result, NX_ERROR_CANCELLED);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(
        nx_spi_endpoint_transfer(&endpoint_api, tx, nullptr, 1, 100, &count),
        NX_ERROR_STATE);
    EXPECT_EQ(nx_spi_port_stop(&bus), NX_SUCCESS);
}

TEST_F(NativeAsync, UARTIDLEBlocksStayImmutableAndLossPrecedesLaterData) {
    nx_native_uart_state_t state = {};
    const nx_uart_port_t uart = {&nx_native_uart_ops, &state};
    uint8_t byte_ring[1] = {};
    const nx_native_uart_config_t config = {NX_UART_RX_BYTES, byte_ring, 1,
                                            nullptr,          0,         false};
    ASSERT_EQ(nx_native_uart_configure_instance(&state, &config), NX_SUCCESS);
    uint8_t data[2][2] = {};
    nx_stream_slot_t slots[2] = {};
    slots[0].data = data[0];
    slots[0].capacity = 2;
    slots[1].data = data[1];
    slots[1].capacity = 2;
    nx_stream_t stream = {};
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 2), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_rx_start(&uart, &stream), NX_SUCCESS);
    uint8_t tx = 3;
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &tx, 1, 100), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&uart, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_uart_model_receive(&uart, 17, 0), NX_SUCCESS);
    ASSERT_EQ(nx_native_uart_model_idle(&uart), NX_SUCCESS);
    nx_stream_block_t first = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &first), NX_SUCCESS);
    EXPECT_EQ(first.length, 1u);
    EXPECT_EQ(first.data[0], 17);
    EXPECT_NE(first.flags & NX_STREAM_BOUNDARY_IDLE, 0u);
    ASSERT_EQ(nx_native_uart_model_receive(&uart, 29, 0), NX_SUCCESS);
    ASSERT_EQ(nx_native_uart_model_receive(&uart, 31, 0), NX_SUCCESS);
    nx_stream_block_t second = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &second), NX_SUCCESS);
    EXPECT_EQ(nx_native_uart_model_receive(&uart, 99, 0), NX_ERROR_OVERFLOW);
    EXPECT_EQ(first.data[0], 17);
    EXPECT_EQ(second.data[0], 29);
    ASSERT_EQ(nx_stream_release(&stream, &first), NX_SUCCESS);
    ASSERT_EQ(nx_native_uart_model_receive(&uart, 37, 0), NX_SUCCESS);
    ASSERT_EQ(nx_native_uart_model_idle(&uart), NX_SUCCESS);
    nx_stream_block_t later = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &later), NX_SUCCESS);
    EXPECT_NE(later.flags & NX_STREAM_BOUNDARY_LOSS, 0u);
    EXPECT_EQ(later.lost_blocks, 1u);
    EXPECT_EQ(nx_uart_port_rx_stop(&uart), NX_ERROR_BUSY);
    EXPECT_EQ(nx_native_uart_model_receive(&uart, 101, 0), NX_ERROR_STATE);
    ASSERT_EQ(nx_stream_release(&stream, &second), NX_SUCCESS);
    ASSERT_EQ(nx_stream_release(&stream, &later), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_stop(&uart), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_stop(&uart), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(NativeAsync, UARTStopPublishesFinalPrefixAndKeepsConsumerStorage) {
    nx_native_uart_state_t state = {};
    const nx_uart_port_t uart = {&nx_native_uart_ops, &state};
    uint8_t ring[1] = {};
    const nx_native_uart_config_t config = {NX_UART_RX_BYTES, ring, 1,
                                            nullptr,          0,    false};
    ASSERT_EQ(nx_native_uart_configure_instance(&state, &config), NX_SUCCESS);
    uint8_t bytes[4] = {};
    nx_stream_slot_t slot = {};
    slot.data = bytes;
    slot.capacity = 4;
    nx_stream_t stream = {};
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_rx_start(&uart, &stream), NX_SUCCESS);
    ASSERT_EQ(nx_native_uart_model_receive(&uart, 17, 0), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_stop(&uart), NX_ERROR_BUSY);
    EXPECT_EQ(nx_native_uart_model_receive(&uart, 29, 0), NX_ERROR_STATE);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.length, 1u);
    EXPECT_EQ(block.data[0], 17);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_IDLE, 0u);
    EXPECT_EQ(nx_native_uart_model_configure(&uart, &config), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_stop(&uart), NX_SUCCESS);
}

TEST_F(NativeAsync, ADCTriggersPublishWholeScansAndNeverOverwriteLoans) {
    nx_native_adc_state_t state = {};
    const nx_adc_port_t adc = {&nx_native_adc_ops, &state};
    const uint16_t samples[] = {17, 29};
    ASSERT_EQ(nx_native_adc_configure_instance(&state, samples, 2, 12, 3300),
              NX_SUCCESS);
    alignas(uint16_t) uint8_t bytes[4] = {};
    nx_stream_slot_t slot = {};
    slot.data = bytes;
    slot.capacity = 4;
    nx_stream_t stream = {};
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_service(&adc), NX_ERROR_STATE);
    ASSERT_EQ(nx_adc_port_stream_start(&adc, &stream, 1000), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_service(&adc), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_sample(&adc, nullptr, 0, 100, nullptr),
              NX_ERROR_INVALID);
    ASSERT_EQ(nx_native_adc_model_trigger(&adc), NX_SUCCESS);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.length, 4u);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_TRIGGER, 0u);
    uint16_t actual[2] = {};
    memcpy(actual, block.data, sizeof(actual));
    EXPECT_EQ(actual[0], 17);
    EXPECT_EQ(actual[1], 29);
    EXPECT_EQ(nx_adc_port_stream_service(&adc), NX_ERROR_BUSY);
    EXPECT_EQ(nx_native_adc_model_trigger(&adc), NX_ERROR_OVERFLOW);
    EXPECT_EQ(nx_adc_port_stream_stop(&adc), NX_ERROR_BUSY);
    EXPECT_EQ(nx_native_adc_model_trigger(&adc), NX_ERROR_STATE);
    EXPECT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_stop(&adc), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_service(&adc), NX_ERROR_STATE);
}

TEST_F(NativeAsync, ADCFailedScanIsNotPublishedAndNextScanReportsLoss) {
    nx_native_adc_state_t state = {};
    const nx_adc_port_t adc = {&nx_native_adc_ops, &state};
    const uint16_t samples[] = {17, 29};
    ASSERT_EQ(nx_native_adc_configure_instance(&state, samples, 2, 12, 3300),
              NX_SUCCESS);
    alignas(uint16_t) uint8_t bytes[4] = {};
    nx_stream_slot_t slot = {};
    slot.data = bytes;
    slot.capacity = 4;
    nx_stream_t stream = {};
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_stream_start(&adc, &stream, 1000), NX_SUCCESS);
    nx_native_adc_model_fault(&adc, 1);
    EXPECT_EQ(nx_native_adc_model_trigger(&adc), NX_ERROR_IO);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    nx_native_adc_model_fault(&adc, SIZE_MAX);
    ASSERT_EQ(nx_native_adc_model_trigger(&adc), NX_SUCCESS);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.length, 4u);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_LOSS, 0u);
    EXPECT_EQ(block.lost_blocks, 1u);
    EXPECT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_stop(&adc), NX_SUCCESS);
}

} /* namespace */
