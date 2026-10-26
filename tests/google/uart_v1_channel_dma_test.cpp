/**
 * \file            uart_v1_channel_dma_test.cpp
 * \brief           Actual legacy channel DMA borrow and RX loan contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/native/model.h"
#include "uart_v1_channel_dma.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

static nx_uart_v1_registers_t* s_uart;
static volatile uint32_t* s_flags;
static volatile uint32_t* s_clear;
static volatile uint32_t* s_refused;
static unsigned s_writes;
static bool s_stop_refused;
static const nx_uart_v1_dma_config_t* s_stopped_config;
extern "C" uint32_t nx_uart_v1_model_read(const volatile uint32_t* reg) {
    uint32_t value = *reg;
    if (reg == &s_uart->data)
        s_uart->status &= ~UINT32_C(0x3f);
    return value;
}
extern "C" void nx_uart_v1_model_write(volatile uint32_t* reg, uint32_t value) {
    s_writes++;
    if (reg == s_refused)
        return;
    if (reg == s_clear) {
        *s_flags &= ~value;
        *reg = value;
    } else if (reg == &s_uart->status)
        *reg &= value;
    else
        *reg = value;
}
static nx_result_t stop_irqs(void* context) {
    s_stopped_config = static_cast<nx_uart_v1_dma_state_t*>(context)->config;
    return s_stop_refused ? NX_ERROR_IO : NX_SUCCESS;
}
static nx_result_t validate_wake(void*, const nx_irq_wake_t*, uint8_t ceiling) {
    return ceiling == 0U ? NX_SUCCESS : NX_ERROR_PERMISSION;
}
class UartV1ChannelDma : public testing::Test {
  protected:
    nx_uart_v1_registers_t uart = {};
    nx_uart_v1_channel_t tx = {}, rx = {};
    uint32_t flags = 0U, clear = 0U;
    uint8_t ring[8] = {}, payload[4] = {1U, 2U, 3U, 4U};
    uint8_t blocks[2][8] = {};
    nx_dma_memory_region_t regions[2] = {};
    nx_uart_v1_dma_config_t config = {}, replacement_config = {};
    nx_uart_v1_dma_duplex_state_t owner = {};
    nx_uart_v1_dma_state_t& state = owner.uart;
    nx_uart_port_t port = {&nx_uart_v1_channel_dma_tx_ops, &state};
    nx_uart_tx_request_t request = {};
    nx_stream_slot_t slots[2] = {};
    nx_stream_t stream = {};
    void SetUp() override {
        s_uart = &uart;
        s_flags = &flags;
        s_clear = &clear;
        s_refused = nullptr;
        s_writes = 0U;
        s_stop_refused = false;
        s_stopped_config = nullptr;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        regions[0] = {reinterpret_cast<uintptr_t>(payload), sizeof(payload),
                      NX_DMA_MEMORY_READ};
        regions[1] = {reinterpret_cast<uintptr_t>(blocks), sizeof(blocks),
                      NX_DMA_MEMORY_WRITE};
        config.registers = &uart;
        config.tx = &tx;
        config.flags = &flags;
        config.clear = &clear;
        config.regions = regions;
        config.region_count = 2U;
        config.tx_shift = 24U;
        config.rx_shift = 20U;
        config.rx_storage = ring;
        config.rx_capacity = sizeof(ring);
        config.baud = 115200U;
        config.stop_irqs = stop_irqs;
        config.validate_wake = validate_wake;
        config.profile = NX_UART_RX_BYTES;
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_uart_tx_prepare(&request, payload, sizeof(payload), 1000U),
                  NX_SUCCESS);
    }
    void Start(bool blocks_profile = false) {
        if (blocks_profile) {
            config.profile = NX_UART_RX_BLOCKS;
            config.receive = &owner.receive;
            config.rx = &rx;
            config.rx_capacity = 0U;
            port.ops = &nx_uart_v1_channel_dma_ops;
        }
        ASSERT_EQ(blocks_profile ? nx_uart_v1_channel_dma_initialize(
                                       &state, &config, 8000000U)
                                 : nx_uart_v1_channel_dma_tx_initialize(
                                       &state, &config, 8000000U),
                  NX_SUCCESS);
    }
    void UartIrq() {
        if (config.profile == NX_UART_RX_BLOCKS)
            nx_uart_v1_channel_dma_irq(&state);
        else
            nx_uart_v1_channel_dma_tx_irq(&state);
    }
    void ChannelIrq(bool receive) {
        if (config.profile == NX_UART_RX_BLOCKS)
            nx_uart_v1_channel_dma_channel_irq(&state, receive);
        else
            nx_uart_v1_channel_dma_tx_channel_irq(&state);
    }
    void StartRx() {
        Start(true);
        for (size_t i = 0; i < 2U; ++i) {
            slots[i].data = blocks[i];
            slots[i].capacity = sizeof(blocks[i]);
        }
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 2U), NX_SUCCESS);
        ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    }
};
TEST_F(UartV1ChannelDma, AdmitsOnlyReviewedClockWithoutEffects) {
    EXPECT_EQ(nx_uart_v1_channel_dma_tx_initialize(&state, &config, 16000000U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(s_writes, 0U);
    Start();
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(uart.baud, 69U);
}
TEST_F(UartV1ChannelDma, RejectsByteRingOverlappingTxChannelBeforeMmio) {
    config.rx_storage = &tx;
    config.rx_capacity = sizeof(tx);
    EXPECT_EQ(nx_uart_v1_channel_dma_tx_initialize(&state, &config, 8000000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(s_writes, 0U);
    EXPECT_FALSE(state.retained);
    EXPECT_FALSE(state.initialized);
}
TEST_F(UartV1ChannelDma, RejectsByteRingOverlappingConfigBeforeBinding) {
    config.rx_storage = &config;
    config.rx_capacity = sizeof(config);
    EXPECT_EQ(nx_uart_v1_channel_dma_tx_initialize(&state, &config, 8000000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(s_writes, 0U);
    EXPECT_EQ(state.config, nullptr);
    EXPECT_FALSE(state.retained);
}
TEST_F(UartV1ChannelDma, ConfigNeverBecomesRequestOrDmaPayload) {
    regions[0] = {reinterpret_cast<uintptr_t>(&config), sizeof(config),
                  NX_DMA_MEMORY_READ};
    Start();
    unsigned before = s_writes;
    EXPECT_EQ(nx_uart_port_submit(
                  &port, reinterpret_cast<nx_uart_tx_request_t*>(&config)),
              NX_ERROR_INVALID);
    ASSERT_EQ(nx_uart_tx_prepare(&request,
                                 reinterpret_cast<const uint8_t*>(&config),
                                 sizeof(config), 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(s_writes, before);
}
TEST_F(UartV1ChannelDma, ConfigNeverBecomesByteOutputCountOrWake) {
    Start();
    size_t count = 9U;
    EXPECT_EQ(nx_uart_port_read_bytes(&port,
                                      reinterpret_cast<uint8_t*>(&config),
                                      sizeof(config), &count),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_uart_port_read_bytes(&port, payload, sizeof(payload),
                                      reinterpret_cast<size_t*>(&config)),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_uart_port_attach_wake(
                  &port, reinterpret_cast<const nx_irq_wake_t*>(&config), 0U),
              NX_ERROR_INVALID);
    EXPECT_EQ(count, 9U);
    EXPECT_EQ(state.wake, nullptr);
}
TEST_F(UartV1ChannelDma, ConfigNeverBecomesStreamMetadataOrReceivePayload) {
    regions[1] = {reinterpret_cast<uintptr_t>(&config), sizeof(config),
                  NX_DMA_MEMORY_WRITE};
    Start(true);
    unsigned before = s_writes;
    EXPECT_EQ(
        nx_uart_port_rx_start(&port, reinterpret_cast<nx_stream_t*>(&config)),
        NX_ERROR_INVALID);
    stream.slots = reinterpret_cast<nx_stream_slot_t*>(&config);
    stream.count = 1U;
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_INVALID);
    slots[0].data = reinterpret_cast<uint8_t*>(&config);
    slots[0].capacity = sizeof(config);
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 1U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_INVALID);
    EXPECT_EQ(owner.receive.stream, nullptr);
    EXPECT_FALSE(owner.receive.filling);
    EXPECT_EQ(s_writes, before);
}
TEST_F(UartV1ChannelDma, WakeValidationRejectionRestoresColdBinding) {
    config.irq_policy.syscall_ceiling = 1U;
    EXPECT_EQ(nx_uart_v1_channel_dma_tx_initialize(&state, &config, 8000000U),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(state.config, nullptr);
    EXPECT_FALSE(state.retained);
    EXPECT_EQ(s_writes, 0U);
}
TEST_F(UartV1ChannelDma, OutstandingPublisherPreventsAnyColdBinding) {
    state.wake_publishers = 1U;
    EXPECT_EQ(nx_uart_v1_channel_dma_tx_initialize(&state, &config, 8000000U),
              NX_ERROR_BUSY);
    EXPECT_EQ(state.config, nullptr);
    EXPECT_EQ(state.wake_publishers, 1U);
    EXPECT_EQ(s_writes, 0U);
}
TEST_F(UartV1ChannelDma, StoppedOwnerCanBindAnotherImmutableConfig) {
    Start();
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    replacement_config = config;
    replacement_config.baud = 57600U;
    EXPECT_EQ(nx_uart_v1_channel_dma_tx_initialize(&state, &replacement_config,
                                                   8000000U),
              NX_SUCCESS);
    EXPECT_EQ(state.config, &replacement_config);
    EXPECT_EQ(uart.baud, 139U);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}
TEST_F(UartV1ChannelDma, UnboundOwnerNeverAccessesHardwareOrBorrows) {
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_INVALID);
    nx_uart_v1_channel_dma_tx_irq(&state);
    nx_uart_v1_channel_dma_tx_channel_irq(&state);
    EXPECT_EQ(state.config, nullptr);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(s_writes, 0U);
}
TEST_F(UartV1ChannelDma, FailedInitializationRetainsConfigUntilStopRetry) {
    s_refused = &uart.baud;
    s_stop_refused = true;
    EXPECT_EQ(nx_uart_v1_channel_dma_tx_initialize(&state, &config, 8000000U),
              NX_ERROR_IO);
    ASSERT_TRUE(state.retained);
    ASSERT_FALSE(state.initialized);
    EXPECT_EQ(state.config, &config);
    nx_uart_v1_dma_config_t replacement = config;
    replacement.baud = 57600U;
    unsigned before = s_writes;
    EXPECT_EQ(
        nx_uart_v1_channel_dma_tx_initialize(&state, &replacement, 8000000U),
        NX_ERROR_BUSY);
    EXPECT_EQ(state.config, &config);
    EXPECT_EQ(s_writes, before);
    s_refused = nullptr;
    s_stop_refused = false;
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(s_stopped_config, &config);
    EXPECT_FALSE(state.retained);
}
TEST_F(UartV1ChannelDma, FailedStopRetainsConfigUntilSameIrqsWithdraw) {
    Start();
    s_stop_refused = true;
    EXPECT_EQ(nx_uart_port_stop(&port), NX_ERROR_IO);
    ASSERT_TRUE(state.retained);
    nx_uart_v1_dma_config_t replacement = config;
    replacement.baud = 57600U;
    unsigned before = s_writes;
    EXPECT_EQ(
        nx_uart_v1_channel_dma_tx_initialize(&state, &replacement, 8000000U),
        NX_ERROR_BUSY);
    EXPECT_EQ(state.config, &config);
    EXPECT_EQ(s_writes, before);
    s_stop_refused = false;
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(s_stopped_config, &config);
    EXPECT_FALSE(state.retained);
}
TEST_F(UartV1ChannelDma, TxOnlySelectionKeepsIrqBytesAndRejectsBlocks) {
    port.ops = &nx_uart_v1_channel_dma_tx_ops;
    ASSERT_EQ(nx_uart_v1_channel_dma_tx_initialize(&state, &config, 8000000U),
              NX_SUCCESS);
    uart.status = 1U << 5U;
    uart.data = 0x73U;
    UartIrq();
    uint8_t output[2] = {};
    size_t count = 0U;
    EXPECT_EQ(nx_uart_port_read_bytes(&port, output, sizeof(output), &count),
              NX_SUCCESS);
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(output[0], 0x73U);
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}
TEST_F(UartV1ChannelDma, RejectedTxReinitializeNeverCancelsExistingBorrow) {
    Start();
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    unsigned before = s_writes;
    const nx_uart_v1_dma_config_t replacement = config;
    EXPECT_EQ(
        nx_uart_v1_channel_dma_tx_initialize(&state, &replacement, 8000000U),
        NX_ERROR_BUSY);
    EXPECT_EQ(s_writes, before);
    EXPECT_FALSE(state.closing);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(state.config, &config);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
}
TEST_F(UartV1ChannelDma, RejectedDuplexReinitializeKeepsExistingTxBorrow) {
    Start(true);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    unsigned before = s_writes;
    const nx_uart_v1_dma_config_t replacement = config;
    EXPECT_EQ(nx_uart_v1_channel_dma_initialize(&state, &replacement, 8000000U),
              NX_ERROR_BUSY);
    EXPECT_EQ(s_writes, before);
    EXPECT_FALSE(state.closing);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(state.config, &config);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
}
TEST_F(UartV1ChannelDma, EmbeddedReceiveStateNeverBecomesDmaPayload) {
    regions[0] = {reinterpret_cast<uintptr_t>(&owner.receive),
                  sizeof(owner.receive), NX_DMA_MEMORY_READ};
    Start(true);
    ASSERT_EQ(nx_uart_tx_prepare(
                  &request, reinterpret_cast<const uint8_t*>(&owner.receive),
                  sizeof(owner.receive), 1000U),
              NX_SUCCESS);
    unsigned before = s_writes;
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(s_writes, before);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
}
TEST_F(UartV1ChannelDma, DmaCompleteDoesNotSettleUntilWireTc) {
    Start();
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(tx.count, sizeof(payload));
    EXPECT_EQ(tx.control & 1U, 1U);
    tx.count = 0U;
    flags = 2U << config.tx_shift;
    ChannelIrq(false);
    nx_uart_port_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(tx.control & 1U, 0U);
    uart.status |= 1U << 6U;
    UartIrq();
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(payload));
}
TEST_F(UartV1ChannelDma, RefusedWithdrawalRetainsAcceptedBorrow) {
    Start();
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.count = 0U;
    flags = 2U << config.tx_shift;
    s_refused = &tx.control;
    uart.status = 1U << 6U;
    ChannelIrq(false);
    nx_native_clock_configure(true, 2000U);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    s_refused = nullptr;
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}
TEST_F(UartV1ChannelDma, UnstartedCancelDoesNotWaitForInventedTc) {
    Start();
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}
TEST_F(UartV1ChannelDma, OutOfDomainRejectedBeforeBorrowAndMmio) {
    Start();
    uint8_t foreign = 7U;
    ASSERT_EQ(nx_uart_tx_prepare(&request, &foreign, 1U, 1000U), NX_SUCCESS);
    unsigned before = s_writes;
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(s_writes, before);
}
TEST_F(UartV1ChannelDma, FirstObservedCompletionAfterDeadlineIsTimeout) {
    Start();
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.count = 0U;
    flags = 2U << config.tx_shift;
    uart.status = 1U << 6U;
    ASSERT_EQ(nx_native_clock_configure(true, 1001U), NX_SUCCESS);
    ChannelIrq(false);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
}
TEST_F(UartV1ChannelDma, PartialCancelBorrowsUntilRealWireDrain) {
    Start();
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.count = 2U;
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(state.active, &request);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    uart.status = 1U << 6U;
    UartIrq();
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(UartV1ChannelDma, SeparateInstancesNeverSettleEachOthersRequest) {
    Start();
    nx_uart_v1_registers_t other_uart = {};
    nx_uart_v1_channel_t other_tx = {};
    uint32_t other_flags = 0U, other_clear = 0U;
    uint8_t other_ring[4] = {}, other_payload[2] = {7U, 8U};
    nx_dma_memory_region_t other_region = {
        reinterpret_cast<uintptr_t>(other_payload), sizeof(other_payload),
        NX_DMA_MEMORY_READ};
    nx_uart_v1_dma_config_t other_config = {};
    nx_uart_v1_dma_state_t other = {};
    other_config.registers = &other_uart;
    other_config.tx = &other_tx;
    other_config.flags = &other_flags;
    other_config.clear = &other_clear;
    other_config.tx_shift = 12U;
    other_config.rx_shift = 16U;
    other_config.regions = &other_region;
    other_config.region_count = 1U;
    other_config.rx_storage = other_ring;
    other_config.rx_capacity = sizeof(other_ring);
    other_config.baud = 115200U;
    other_config.stop_irqs = stop_irqs;
    other_config.validate_wake = validate_wake;
    ASSERT_EQ(
        nx_uart_v1_channel_dma_tx_initialize(&other, &other_config, 8000000U),
        NX_SUCCESS);
    const nx_uart_port_t other_face = {&nx_uart_v1_channel_dma_tx_ops, &other};
    nx_uart_tx_request_t other_request = {};
    nx_request_initialize(&other_request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&other_request, other_payload,
                                 sizeof(other_payload), 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&other_face, &other_request), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.count = 0U;
    flags = 2U << config.tx_shift;
    uart.status = 1U << 6U;
    ChannelIrq(false);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_request_state(&other_request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(other.active, &other_request);
    EXPECT_EQ(other_tx.count, sizeof(other_payload));
    EXPECT_EQ(nx_uart_port_stop(&other_face), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&other_request.base), NX_REQUEST_SETTLED);
}
TEST_F(UartV1ChannelDma, RxPublishFollowsDisableAndStableCount) {
    StartRx();
    ASSERT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    rx.count = 0U;
    flags = 2U << config.rx_shift;
    ChannelIrq(true);
    EXPECT_NE(slots[0].state, NX_STREAM_SLOT_READY);
    nx_uart_port_service(&port);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
    EXPECT_EQ(slots[0].length, sizeof(blocks[0]));
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FILLING);
}
TEST_F(UartV1ChannelDma, RxWithdrawalFailureNeverReturnsProducerLoan) {
    StartRx();
    rx.count = 0U;
    flags = 2U << config.rx_shift;
    s_refused = &rx.control;
    ChannelIrq(true);
    nx_uart_port_service(&port);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_TRUE(owner.receive.rx_quarantined);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_ERROR_BUSY);
    s_refused = nullptr;
    nx_uart_port_service(&port);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
}
TEST_F(UartV1ChannelDma, BackpressureNeverOverwritesConsumerLoan) {
    StartRx();
    for (size_t i = 0; i < 2U; ++i) {
        rx.count = 0U;
        flags = 2U << config.rx_shift;
        ChannelIrq(true);
        nx_uart_port_service(&port);
    }
    nx_stream_block_t loan = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &loan), NX_SUCCESS);
    EXPECT_EQ(rx.control & 1U, 0U);
    nx_uart_port_service(&port);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(rx.control & 1U, 0U);
    ASSERT_EQ(nx_stream_release(&stream, &loan), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
}
TEST_F(UartV1ChannelDma, IdleBoundaryReturnsOnlyObservedPrefix) {
    StartRx();
    rx.count = 5U;
    uart.status = 1U << 4U;
    UartIrq();
    nx_uart_port_service(&port);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
    EXPECT_EQ(slots[0].length, 3U);
    EXPECT_NE(slots[0].flags & NX_STREAM_BOUNDARY_IDLE, 0U);
}
TEST_F(UartV1ChannelDma, ByteDiscardedAfterNormalDmaBoundaryReportsLoss) {
    StartRx();
    rx.count = 0U;
    flags = 2U << config.rx_shift;
    uart.status = 1U << 5U;
    uart.data = 0x55U;
    ChannelIrq(true);
    nx_uart_port_service(&port);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
    EXPECT_NE(slots[0].flags & NX_STREAM_BOUNDARY_LOSS, 0U);
    EXPECT_EQ(slots[0].lost_blocks, 1U);
}
TEST_F(UartV1ChannelDma, StopDoesNotRevokeReadyOrConsumerLoans) {
    StartRx();
    rx.count = 0U;
    flags = 2U << config.rx_shift;
    ChannelIrq(true);
    nx_uart_port_service(&port);
    nx_stream_block_t loan = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &loan), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_BORROWED);
    ASSERT_EQ(nx_stream_release(&stream, &loan), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
}
TEST_F(UartV1ChannelDma, RepeatedRxStopAfterPortStopHasNoMmioEffects) {
    StartRx();
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    unsigned before = s_writes;
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
    EXPECT_EQ(s_writes, before);
}
TEST_F(UartV1ChannelDma, CapturedWakePublisherMustJoinBeforeStorageReturns) {
    Start();
    testing::StrictMock<testing::MockFunction<nx_result_t()>> observer;
    const nx_irq_wake_t wake = {
        &observer,
        [](void* raw) -> nx_result_t {
            return static_cast<testing::MockFunction<nx_result_t()>*>(raw)
                ->Call();
        },
        false};
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &wake, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.count = 0U;
    flags = 2U << config.tx_shift;
    uart.status = 1U << 6U;
    EXPECT_CALL(observer, Call()).WillOnce(testing::Invoke([&] {
        nx_uart_port_service(&port);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        EXPECT_EQ(nx_uart_port_stop(&port), NX_ERROR_BUSY);
        EXPECT_EQ(state.wake, &wake);
        return NX_SUCCESS;
    }));
    ChannelIrq(false);
    EXPECT_EQ(state.wake_publishers, 0U);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}
