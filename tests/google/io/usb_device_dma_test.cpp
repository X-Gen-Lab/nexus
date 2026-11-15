/**
 * \file            usb_device_dma_test.cpp
 * \brief           HS AHB DMA staging, SETUP and complete memory withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/native/model.h"
#include "usb_device.h"
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

class UsbDeviceDma : public testing::Test {
  protected:
    nx_native_usb_device_t model = {};
    nx_usb_endpoint_t endpoints[4] = {};
    nx_usb_event_t events[8] = {};
    nx_usb_dma_storage_t staging[4] = {};
    nx_dma_memory_region_t region = {0x20000000U, 4096U,
                                     NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    nx_usb_request_t request = {};
    uint8_t bytes[513] = {};
    const nx_usb_device_port_t device = {&nx_usb_fs_device_ops,
                                         &model.controller};
    nx_usb_endpoint_port_t Port(size_t index) {
        return {&nx_usb_fs_endpoint_ops, &endpoints[index]};
    }
    void SetUp() override {
        ASSERT_NE(mmap(reinterpret_cast<void*>(0x20000000U), 4096U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0),
                  MAP_FAILED);
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        auto& c = model.controller;
        c.core = NX_USB_CORE_HS_ULPI;
        c.dma = true;
        c.dma_storage = staging;
        c.dma_regions = &region;
        c.dma_region_count = 1U;
        c.endpoints = endpoints;
        c.endpoint_count = 4U;
        c.events = events;
        c.capacity = 8U;
        c.rx_fifo_words = 512U;
        c.poll_limit = 4U;
        c.drain_timeout_us = 20U;
        c.irq_policy = {NX_IRQ_KERNEL_NONE, 82U, 4U, 0U};
        c.irq = {77U, 5U, 3U};
        model.hardware.clock_ready = true;
        model.hardware.endpoint_numbers = 6U;
        for (size_t i = 0U; i < 4U; ++i) {
            endpoints[i].number = static_cast<uint8_t>(i / 2U);
            endpoints[i].in = i % 2U == 0U;
            endpoints[i].transfer = static_cast<uint8_t>(
                i < 2U ? NX_USB_PROFILE_CONTROL : NX_USB_PROFILE_BULK);
            endpoints[i].max_packet = i < 2U ? 64U : 512U;
            staging[i] = {reinterpret_cast<uint8_t*>(0x20000000U + i * 1024U),
                          1024U};
        }
        endpoints[0].fifo_start = 512U;
        endpoints[0].fifo_words = 32U;
        endpoints[2].fifo_start = 544U;
        endpoints[2].fifo_words = 128U;
        nx_request_initialize(&request.base);
    }
    /** \brief External RM0090 DMA writes advance the address and SETUP count.
     * No DCD method, completion state or callback is substituted here. */
    void QueueSetups(const uint8_t* packets, size_t count) {
        ASSERT_GE(count, 1U);
        ASSERT_LE(count, 3U);
        auto& hw = model.hardware;
        const uintptr_t address = hw.registers[0xB14U / 4U];
        const uintptr_t begin = reinterpret_cast<uintptr_t>(staging[1].bytes);
        ASSERT_GE(address, begin);
        ASSERT_LE(address - begin + count * 8U, staging[1].capacity);
        const uint32_t remaining = hw.registers[0xB10U / 4U] >> 29U;
        ASSERT_GE(remaining, count);
        std::memcpy(reinterpret_cast<void*>(address), packets, count * 8U);
        hw.registers[0xB14U / 4U] = static_cast<uint32_t>(address + count * 8U);
        hw.registers[0xB10U / 4U] =
            (hw.registers[0xB10U / 4U] & ~(3U << 29U)) |
            ((remaining - static_cast<uint32_t>(count)) << 29U);
        hw.registers[0xB00U / 4U] &= ~(1U << 31U);
        hw.registers[0xB08U / 4U] |= 8U;
    }
    void Start() {
        ASSERT_EQ(nx_native_usb_initialize(&model), NX_SUCCESS);
        const nx_usb_activation_port_t activation = {&nx_usb_fs_activation_ops,
                                                     &model.controller};
        ASSERT_EQ(nx_usb_activation_set(&activation,
                                        model.controller.snapshot.epoch, 0x81U,
                                        true),
                  NX_SUCCESS);
        ASSERT_EQ(nx_usb_activation_set(
                      &activation, model.controller.snapshot.epoch, 1U, true),
                  NX_SUCCESS);
    }
    void TearDown() override {
        model.hardware.hold_flush = model.hardware.hold_disable = false;
        model.hardware.registers[0x10U / 4U] |= 1U << 31U;
        EXPECT_EQ(nx_usb_device_stop(&device), NX_SUCCESS);
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x20000000U), 4096U), 0);
    }
};

TEST_F(UsbDeviceDma, NonDmaDomainRejectsBeforeClockOrRegisterEffects) {
    region.begin = 0x10000000U;
    EXPECT_EQ(nx_native_usb_initialize(&model), NX_ERROR_INVALID);
    EXPECT_EQ(model.hardware.consumers, 0U);
    EXPECT_EQ(model.hardware.mmio_writes, 0U);
}

TEST_F(UsbDeviceDma, IsochronousDmaKeepsOneTransactionAndAhbWithdrawal) {
    endpoints[2].transfer = endpoints[3].transfer = NX_USB_PROFILE_ISOCHRONOUS;
    endpoints[2].max_packet = endpoints[3].max_packet = 1024U;
    model.controller.rx_fifo_words = 288U;
    endpoints[0].fifo_start = 288U;
    endpoints[2].fifo_start = 320U;
    endpoints[2].fifo_words = 256U;
    Start();
    model.hardware.registers[0x808U / 4U] = 0x3FFFU << 8U;
    model.hardware.registers[0x14U / 4U] |= 1U << 13U;
    nx_usb_controller_irq(&model.controller);
    const auto in = Port(2U);
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, sizeof(bytes), NX_USB_DATA, 0U,
                                200U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&in, &request), NX_SUCCESS);
    EXPECT_EQ(model.hardware.registers[0x930U / 4U],
              (1U << 29U) | (1U << 19U) | sizeof(bytes));
    EXPECT_EQ(model.hardware.registers[0x920U / 4U] & (1U << 16U), 0U);
    ASSERT_EQ(nx_native_usb_complete(&model, 1U, true), NX_SUCCESS);
    model.hardware.registers[0x10U / 4U] &= ~(1U << 31U);
    nx_usb_controller_irq(&model.controller);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    model.hardware.registers[0x10U / 4U] |= 1U << 31U;
    nx_usb_controller_irq(&model.controller);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(model.hardware.registers[0x934U / 4U], 0U);
    const auto out = Port(3U);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, sizeof(bytes), NX_USB_DATA,
                                 0U, 200U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&out, &request), NX_SUCCESS);
    staging[3].bytes[512] = 0xB7U;
    model.hardware.registers[0xB30U / 4U] = (1U << 19U) | 511U;
    ASSERT_EQ(nx_native_usb_complete(&model, 1U, false), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(bytes));
    EXPECT_EQ(bytes[512], 0xB7U);
}

TEST_F(UsbDeviceDma, InUsesOwnedStagingAndNeedsAhbIdleBeforeSettlement) {
    Start();
    bytes[1] = 1U;
    bytes[2] = 2U;
    bytes[3] = 3U;
    const auto port = Port(2U);
    ASSERT_EQ(
        nx_usb_in_prepare(&request, bytes + 1U, 3U, NX_USB_DATA, 0U, 200U),
        NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    EXPECT_NE(model.hardware.registers[8U / 4U] & (1U << 5U), 0U);
    EXPECT_EQ(model.hardware.registers[0x934U / 4U], 0x20000800U);
    EXPECT_EQ(std::memcmp(staging[2].bytes, bytes + 1U, 3U), 0);
    EXPECT_EQ(model.hardware.tx_count[1], 0U);
    ASSERT_EQ(nx_native_usb_complete(&model, 1U, true), NX_SUCCESS);
    model.hardware.registers[0x10U / 4U] &= ~(1U << 31U);
    nx_usb_controller_irq(&model.controller);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    model.hardware.registers[0x10U / 4U] |= (1U << 31U) | (1U << 30U);
    nx_usb_controller_irq(&model.controller);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_NE(model.hardware.registers[0x934U / 4U], 0U);
    model.hardware.registers[0x10U / 4U] &= ~(1U << 30U);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(model.hardware.registers[0x934U / 4U], 0U);
}

TEST_F(UsbDeviceDma, OutOverflowCopiesOnlyTheBorrowedExtentAfterDmaDrain) {
    Start();
    const auto port = Port(3U);
    bytes[2] = 0xEEU;
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 2U, NX_USB_DATA, 0U, 200U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    staging[3].bytes[0] = 1U;
    staging[3].bytes[1] = 2U;
    staging[3].bytes[2] = 3U;
    model.hardware.registers[0xB30U / 4U] = (1U << 19U) | 509U;
    ASSERT_EQ(nx_native_usb_complete(&model, 1U, false), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_OVERFLOW);
    EXPECT_EQ(request.base.transferred, 2U);
    EXPECT_EQ(bytes[0], 1U);
    EXPECT_EQ(bytes[1], 2U);
    EXPECT_EQ(bytes[2], 0xEEU);
    EXPECT_EQ(model.hardware.registers[0xB34U / 4U], 0U);
}

TEST_F(UsbDeviceDma, SetupReceptionUsesInternalAlignedStorageWithoutRxFifo) {
    Start();
    EXPECT_EQ(model.hardware.registers[0xB14U / 4U], 0x20000400U);
    const uint8_t setup[] = {0U, 5U, 7U, 0U, 0U, 0U, 0U, 0U};
    QueueSetups(setup, 1U);
    nx_usb_controller_irq(&model.controller);
    EXPECT_TRUE(model.controller.snapshot.control_pending);
    EXPECT_EQ(model.controller.snapshot.setup.request, 5U);
    EXPECT_EQ(model.controller.snapshot.setup.value, 7U);
    EXPECT_EQ(model.hardware.rx_count, 0U);
}

TEST_F(UsbDeviceDma, LargestOutPacketAndSetupStatusMustFitTheRxFifo) {
    model.controller.rx_fifo_words = 64U;
    EXPECT_EQ(nx_native_usb_initialize(&model), NX_ERROR_INVALID);
    EXPECT_EQ(model.hardware.consumers, 0U);
    EXPECT_EQ(model.hardware.mmio_writes, 0U);
}

TEST_F(UsbDeviceDma, InternalDmaRegistersReserveTheirOwnPhysicalFifoWords) {
    endpoints[2].fifo_start = 896U;
    EXPECT_EQ(nx_native_usb_initialize(&model), NX_ERROR_INVALID);
    EXPECT_EQ(model.hardware.consumers, 0U);
}

TEST_F(UsbDeviceDma, LowerUnusedTransmitFifosAlsoConsumeMinimumRam) {
    model.controller.dma = false;
    model.controller.dma_storage = nullptr;
    model.controller.dma_regions = nullptr;
    model.controller.dma_region_count = 0U;
    endpoints[2].number = 5U;
    endpoints[2].fifo_start = 896U;
    EXPECT_EQ(nx_native_usb_initialize(&model), NX_ERROR_INVALID);
    EXPECT_EQ(model.hardware.consumers, 0U);
}

TEST_F(UsbDeviceDma, AhbErrorRequiresEndpointAndDmaDrainBeforeIoSettlement) {
    Start();
    const auto port = Port(2U);
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 3U, NX_USB_DATA, 0U, 200U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    model.hardware.registers[0x928U / 4U] |= 4U;
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(model.hardware.registers[0x934U / 4U], 0U);
    EXPECT_EQ(model.hardware.registers[0x928U / 4U] & 4U, 0U);
}

/** \brief Protocol closure cannot wait for another endpoint's AHB join. */
class UsbStatusJoin : public UsbDeviceDma,
                      public testing::WithParamInterface<bool> {
  protected:
    nx_usb_request_t status_request = {};
    nx_usb_request_t bulk_request = {};
    void TearDown() override {
        model.hardware.refused_dma_address_offset = 0U;
        UsbDeviceDma::TearDown();
    }
};
TEST_P(UsbStatusJoin, WireStatusEndsAdmissionWhileDmaStorageRemainsBorrowed) {
    Start();
    const uint8_t setup[8] = {0x80U, 6U, 0U, 1U, 0U, 0U, 128U, 0U};
    QueueSetups(setup, 1U);
    nx_usb_controller_irq(&model.controller);
    const uint64_t epoch = model.controller.snapshot.epoch;
    const auto bulk = Port(2U);
    const auto input = Port(0U);
    const auto output = Port(1U);
    nx_request_initialize(&bulk_request.base);
    ASSERT_EQ(NX_SUCCESS, nx_usb_in_prepare(&bulk_request, bytes, 1U,
                                            NX_USB_DATA, 0U, 500U));
    ASSERT_EQ(NX_SUCCESS, nx_usb_endpoint_submit(&bulk, &bulk_request));
    nx_request_initialize(&status_request.base);
    ASSERT_EQ(NX_SUCCESS, nx_usb_out_prepare(&status_request, nullptr, 0U,
                                             NX_USB_STATUS, epoch, 500U));
    ASSERT_EQ(NX_SUCCESS, nx_usb_endpoint_submit(&output, &status_request));
    model.hardware.registers[0xB10U / 4U] = (3U << 29U) | (1U << 19U) | 64U;
    ASSERT_EQ(NX_SUCCESS, nx_native_usb_complete(&model, 0U, false));
    const uint32_t out_mask = model.hardware.registers[0x814U / 4U];
    const uint32_t endpoint_mask = model.hardware.registers[0x81CU / 4U];
    const uint32_t global_mask = model.hardware.registers[0x018U / 4U];
    if (GetParam())
        model.hardware.refused_dma_address_offset = 0xB14U;
    else
        model.hardware.registers[0x10U / 4U] &= ~(1U << 31U);
    nx_usb_controller_irq(&model.controller);
    /* RM0090 endpoint completion -> common mask -> DAINTMSK -> OEPINT
     * -> GINT gate -> NVIC. Check the real source after one bounded IRQ,
     * without an unbounded simulated interrupt loop or a worker mock. */
    const bool completion_irq =
        model.hardware.irq_enabled &&
        (model.hardware.registers[0x008U / 4U] & 1U) != 0U &&
        (model.hardware.registers[0x018U / 4U] & (1U << 19U)) != 0U &&
        (model.hardware.registers[0x81CU / 4U] & (1U << 16U)) != 0U &&
        (model.hardware.registers[0x814U / 4U] & 1U) != 0U &&
        (model.hardware.registers[0xB08U / 4U] & 1U) != 0U;
    EXPECT_FALSE(completion_irq);
    EXPECT_TRUE(model.hardware.irq_enabled);
    EXPECT_EQ(out_mask & 9U, model.hardware.registers[0x814U / 4U] & 9U);
    EXPECT_EQ(endpoint_mask & ((1U << 16U) | 2U),
              model.hardware.registers[0x81CU / 4U] & ((1U << 16U) | 2U));
    EXPECT_EQ(global_mask & ((1U << 18U) | (1U << 19U)),
              model.hardware.registers[0x018U / 4U] &
                  ((1U << 18U) | (1U << 19U)));
    EXPECT_EQ(0U, model.controller.publishers);
    EXPECT_FALSE(model.controller.snapshot.control_pending);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&status_request.base));
    EXPECT_EQ(&status_request, endpoints[1].request);
    ASSERT_EQ(NX_SUCCESS, nx_usb_in_prepare(&request, bytes + 1U, 1U,
                                            NX_USB_DATA, epoch, 500U));
    ASSERT_NE(NX_SUCCESS, nx_usb_endpoint_submit(&input, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    model.hardware.refused_dma_address_offset = 0U;
    model.hardware.registers[0x10U / 4U] |= 1U << 31U;
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&status_request.base));
    EXPECT_EQ(NX_SUCCESS, status_request.base.result);
    EXPECT_EQ(nullptr, endpoints[1].request);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&bulk_request.base));
    EXPECT_EQ(&bulk_request, endpoints[2].request);
    EXPECT_EQ(0x20000800U, model.hardware.registers[0x934U / 4U]);
    EXPECT_EQ(1U, model.hardware.consumers);
}
INSTANTIATE_TEST_SUITE_P(AhbBusyOrAddressWithdrawalRefused, UsbStatusJoin,
                         testing::Values(false, true));

TEST_F(UsbDeviceDma,
       RefusedCompletionAckRetainsTheControllerForThreadRecovery) {
    Start();
    const auto input = Port(2U);
    ASSERT_EQ(NX_SUCCESS,
              nx_usb_in_prepare(&request, bytes, 1U, NX_USB_DATA, 0U, 500U));
    ASSERT_EQ(NX_SUCCESS, nx_usb_endpoint_submit(&input, &request));
    model.hardware.hold_clear = true;
    EXPECT_EQ(NX_SUCCESS, nx_native_usb_complete(&model, 1U, true));
    nx_usb_controller_irq(&model.controller);
    EXPECT_FALSE(model.hardware.irq_enabled);
    EXPECT_TRUE(model.controller.snapshot.closing);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, endpoints[2].request);
    EXPECT_EQ(1U, model.hardware.consumers);
    EXPECT_EQ(NX_SUCCESS, nx_native_clock_advance(20U));
    EXPECT_EQ(NX_SUCCESS, nx_usb_device_service(&device));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    model.hardware.hold_clear = false;
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_service(&device));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(nullptr, endpoints[2].request);
    EXPECT_EQ(1U, model.hardware.consumers);
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_stop(&device));
    EXPECT_EQ(0U, model.hardware.consumers);
}

TEST_F(UsbDeviceDma, ActiveSetupBankIsNotReprogrammedByOrdinaryService) {
    Start();
    auto& hw = model.hardware;
    const uint32_t moving_address = hw.registers[0xB14U / 4U] + 4U;
    const uint32_t moving_size = hw.registers[0xB10U / 4U] - 4U;
    hw.registers[0xB14U / 4U] = moving_address;
    hw.registers[0xB10U / 4U] = moving_size;
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(hw.registers[0xB14U / 4U], moving_address);
    EXPECT_EQ(hw.registers[0xB10U / 4U], moving_size);
}

class UsbSetupBurst : public UsbDeviceDma,
                      public testing::WithParamInterface<unsigned> {};
TEST_P(UsbSetupBurst, OnlyTheLastCompletedSetupIsPublished) {
    Start();
    const uint8_t packets[24] = {0U, 0x41U, 1U, 0U, 0U, 0U, 0U, 0U,
                                 0U, 0x42U, 2U, 0U, 0U, 0U, 0U, 0U,
                                 0U, 0x43U, 3U, 0U, 0U, 0U, 0U, 0U};
    const uint64_t previous = model.controller.snapshot.epoch;
    QueueSetups(packets, GetParam());
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(model.controller.snapshot.epoch, previous + 1U);
    EXPECT_EQ(model.controller.snapshot.setup.request, 0x40U + GetParam());
    EXPECT_EQ(model.controller.snapshot.setup.value, GetParam());
    EXPECT_TRUE(model.controller.snapshot.control_pending);
}
INSTANTIATE_TEST_SUITE_P(TwoOrThreePackets, UsbSetupBurst,
                         testing::Values(2U, 3U));

TEST_F(UsbDeviceDma,
       SetupDmaInProgressIsQuietedWithoutReadingAnIncompleteSlot) {
    Start();
    auto& hw = model.hardware;
    const uint8_t first[8] = {0U, 0x41U, 1U, 0U, 0U, 0U, 0U, 0U};
    const uint8_t second[8] = {0U, 0x42U, 2U, 0U, 0U, 0U, 0U, 0U};
    QueueSetups(first, 1U);
    std::memcpy(staging[1].bytes + 8U, second, 4U);
    hw.registers[0xB14U / 4U] += 4U;
    hw.registers[0xB10U / 4U] &= ~(3U << 29U);
    hw.registers[0xB10U / 4U] |= 1U << 29U;
    hw.registers[0x10U / 4U] &= ~(1U << 31U);
    hw.registers[0x10U / 4U] |= 1U << 30U;
    const uint32_t partial_address = hw.registers[0xB14U / 4U];
    nx_usb_controller_irq(&model.controller);
    EXPECT_FALSE(model.controller.snapshot.control_pending);
    EXPECT_EQ(hw.registers[0xB14U / 4U], partial_address);
    EXPECT_EQ(hw.registers[0xB08U / 4U] & 8U, 0U);
    EXPECT_TRUE(hw.irq_enabled);
    EXPECT_NE(hw.registers[0x814U / 4U] & 8U, 0U);
    std::memcpy(staging[1].bytes + 12U, second + 4U, 4U);
    hw.registers[0xB14U / 4U] += 4U;
    hw.registers[0x10U / 4U] &= ~(1U << 30U);
    hw.registers[0x10U / 4U] |= 1U << 31U;
    nx_usb_controller_irq(&model.controller);
    EXPECT_TRUE(model.controller.snapshot.control_pending);
    EXPECT_EQ(model.controller.snapshot.setup.request, 0x42U);
    EXPECT_EQ(model.controller.snapshot.setup.value, 2U);
}

TEST_F(UsbDeviceDma, OutZeroStagingReservesThreeSetupsAfterAFullDataPacket) {
    staging[1].capacity = 64U;
    EXPECT_EQ(nx_native_usb_initialize(&model), NX_ERROR_INVALID);
    EXPECT_EQ(model.hardware.consumers, 0U);
    EXPECT_EQ(model.hardware.mmio_writes, 0U);
}

TEST_F(UsbDeviceDma, NewSetupDoesNotBecomeAnOldOutPayloadWhileAhbIsBusy) {
    Start();
    const uint8_t initial[8] = {0U, 0x40U, 0U, 0U, 0U, 0U, 64U, 0U};
    QueueSetups(initial, 1U);
    nx_usb_controller_irq(&model.controller);
    const auto output = Port(1U);
    std::memset(bytes, 0xA5, 64U);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 64U, NX_USB_DATA,
                                 model.controller.snapshot.epoch, 500U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&output, &request), NX_SUCCESS);
    std::memset(staging[1].bytes, 0xBC, 64U);
    model.hardware.registers[0xB14U / 4U] += 64U;
    const uint8_t replacement[8] = {0U, 5U, 7U, 0U, 0U, 0U, 0U, 0U};
    QueueSetups(replacement, 1U);
    model.hardware.registers[0xB08U / 4U] |= 1U;
    model.hardware.registers[0x10U / 4U] &= ~(1U << 31U);
    const uint32_t held = model.hardware.registers[0xB14U / 4U];
    nx_usb_controller_irq(&model.controller);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_FALSE(model.controller.snapshot.control_pending);
    EXPECT_EQ(model.hardware.registers[0xB14U / 4U], held);
    for (size_t i = 0U; i < 64U; ++i)
        EXPECT_EQ(bytes[i], 0xA5U);
    model.hardware.registers[0x10U / 4U] |= 1U << 31U;
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_TRUE(model.controller.snapshot.control_pending);
    EXPECT_EQ(model.controller.snapshot.setup.request, 5U);
    EXPECT_EQ(model.controller.snapshot.setup.value, 7U);
}

TEST_F(UsbDeviceDma, SetupOutsideTheOwnedBankCannotPublishAControlEpoch) {
    Start();
    const uint64_t previous = model.controller.snapshot.epoch;
    model.hardware.registers[0xB14U / 4U] = 0x20001008U;
    model.hardware.registers[0xB10U / 4U] = 2U << 29U;
    model.hardware.registers[0xB00U / 4U] &= ~(1U << 31U);
    model.hardware.registers[0xB08U / 4U] |= 8U;
    nx_usb_controller_irq(&model.controller);
    EXPECT_TRUE(model.controller.snapshot.closing);
    EXPECT_FALSE(model.controller.snapshot.control_pending);
    EXPECT_LE(model.controller.snapshot.epoch, previous + 1U);
    EXPECT_EQ(model.hardware.consumers, 1U);
}

class UsbSetupError : public UsbDeviceDma,
                      public testing::WithParamInterface<uint32_t> {};
TEST_P(UsbSetupError, CorruptOrOverwrittenSetupCannotBecomeAValidRequest) {
    Start();
    const uint8_t setup[8] = {0U, 5U, 7U, 0U, 0U, 0U, 0U, 0U};
    QueueSetups(setup, 1U);
    model.hardware.registers[0xB08U / 4U] |= GetParam();
    nx_usb_controller_irq(&model.controller);
    EXPECT_TRUE(model.controller.snapshot.closing);
    EXPECT_FALSE(model.controller.snapshot.control_pending);
    EXPECT_EQ(model.hardware.consumers, 1U);
}
INSTANTIATE_TEST_SUITE_P(AhbErrorOrFourthSetup, UsbSetupError,
                         testing::Values(4U, 64U));

TEST_F(UsbDeviceDma, FullOutAndThreeSetupsFitOneExactGuardedBank) {
    staging[3].capacity = 512U;
    staging[1] = {reinterpret_cast<uint8_t*>(region.begin + region.size - 88U),
                  88U};
    ASSERT_NE(mmap(reinterpret_cast<void*>(region.begin + region.size), 4096U,
                   PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0),
              MAP_FAILED);
    Start();
    const uint8_t initial[8] = {0U, 0x40U, 0U, 0U, 0U, 0U, 64U, 0U};
    QueueSetups(initial, 1U);
    nx_usb_controller_irq(&model.controller);
    const auto output = Port(1U);
    std::memset(bytes, 0xA5, 64U);
    EXPECT_EQ(nx_usb_out_prepare(&request, bytes, 64U, NX_USB_DATA,
                                 model.controller.snapshot.epoch, 500U),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&output, &request), NX_SUCCESS);
    std::memset(staging[1].bytes, 0xBC, 64U);
    model.hardware.registers[0xB14U / 4U] += 64U;
    const uint8_t replacements[24] = {0U, 0x41U, 1U, 0U, 0U, 0U, 0U, 0U,
                                      0U, 0x42U, 2U, 0U, 0U, 0U, 0U, 0U,
                                      0U, 0x43U, 3U, 0U, 0U, 0U, 0U, 0U};
    QueueSetups(replacements, 3U);
    EXPECT_EQ(model.hardware.registers[0xB14U / 4U],
              region.begin + region.size);
    model.hardware.registers[0xB08U / 4U] |= 1U;
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_TRUE(model.controller.snapshot.control_pending);
    EXPECT_EQ(model.controller.snapshot.setup.request, 0x43U);
    EXPECT_EQ(model.controller.snapshot.setup.value, 3U);
    for (size_t i = 0U; i < 64U; ++i)
        EXPECT_EQ(bytes[i], 0xA5U);
    EXPECT_EQ(
        munmap(reinterpret_cast<void*>(region.begin + region.size), 4096U), 0);
}

TEST_F(UsbDeviceDma, ANewCompletedSetupDuringSnapshotDoesNotPublishOldBytes) {
    Start();
    const uint8_t setup[8] = {0U, 0x41U, 1U, 0U, 0U, 0U, 0U, 0U};
    QueueSetups(setup, 1U);
    struct Injection {
        nx_native_usb_hardware_t* hardware;
        unsigned address_reads;
        bool fired;
    } injection = {&model.hardware, 0U, false};
    model.hardware.read_context = &injection;
    model.hardware.before_read = [](void* context, uint32_t offset) {
        auto& change = *static_cast<Injection*>(context);
        if (offset != 0xB14U || ++change.address_reads != 2U)
            return;
        auto& hw = *change.hardware;
        hw.before_read = nullptr;
        const uint8_t next[8] = {0U, 0x42U, 2U, 0U, 0U, 0U, 0U, 0U};
        const uint32_t address = hw.registers[0xB14U / 4U];
        std::memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(address)),
                    next, sizeof(next));
        hw.registers[0xB14U / 4U] += 8U;
        hw.registers[0xB10U / 4U] &= ~(3U << 29U);
        hw.registers[0xB10U / 4U] |= 1U << 29U;
        hw.registers[0xB08U / 4U] |= 8U;
        change.fired = true;
    };
    nx_usb_controller_irq(&model.controller);
    model.hardware.before_read = nullptr;
    model.hardware.read_context = nullptr;
    EXPECT_TRUE(injection.fired);
    EXPECT_FALSE(model.controller.snapshot.control_pending);
    EXPECT_NE(model.hardware.registers[0xB08U / 4U] & 8U, 0U);
    nx_usb_controller_irq(&model.controller);
    EXPECT_TRUE(model.controller.snapshot.control_pending);
    EXPECT_EQ(model.controller.snapshot.setup.request, 0x42U);
    EXPECT_EQ(model.controller.snapshot.setup.value, 2U);
}

TEST_F(UsbDeviceDma, PendingSetupRetainsItsAddressWhenAhbBecomesIdleInDrain) {
    Start();
    const uint8_t initial[8] = {0U, 0x40U, 0U, 0U, 0U, 0U, 64U, 0U};
    QueueSetups(initial, 1U);
    nx_usb_controller_irq(&model.controller);
    const auto output = Port(1U);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 64U, NX_USB_DATA,
                                 model.controller.snapshot.epoch, 500U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&output, &request), NX_SUCCESS);
    const uint8_t replacement[8] = {0U, 0x42U, 2U, 0U, 0U, 0U, 0U, 0U};
    QueueSetups(replacement, 1U);
    model.hardware.registers[0xB08U / 4U] |= 1U;
    model.hardware.registers[0x10U / 4U] &= ~(1U << 31U);
    const uint32_t address = model.hardware.registers[0xB14U / 4U];
    struct Readiness {
        nx_native_usb_hardware_t* hardware;
        unsigned reads;
    } readiness = {&model.hardware, 0U};
    model.hardware.read_context = &readiness;
    model.hardware.before_read = [](void* context, uint32_t offset) {
        auto& ready = *static_cast<Readiness*>(context);
        if (offset == 0x10U && ++ready.reads == 2U)
            ready.hardware->registers[offset / 4U] |= 1U << 31U;
    };
    nx_usb_controller_irq(&model.controller);
    model.hardware.before_read = nullptr;
    model.hardware.read_context = nullptr;
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(model.hardware.registers[0xB14U / 4U], address);
    model.hardware.registers[0x10U / 4U] |= 1U << 31U;
    nx_usb_controller_irq(&model.controller);
    EXPECT_FALSE(model.controller.snapshot.closing);
    EXPECT_TRUE(model.controller.snapshot.control_pending);
    EXPECT_EQ(model.controller.snapshot.setup.request, 0x42U);
    EXPECT_EQ(model.controller.snapshot.setup.value, 2U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}

TEST_F(UsbDeviceDma, SetupDmaThatStartsBetweenRegisterReadsIsRetried) {
    Start();
    const uint8_t setup[8] = {0U, 0x41U, 1U, 0U, 0U, 0U, 0U, 0U};
    const uint8_t next[8] = {0U, 0x42U, 2U, 0U, 0U, 0U, 0U, 0U};
    QueueSetups(setup, 1U);
    struct Injection {
        nx_native_usb_hardware_t* hardware;
        bool fired;
    } injection = {&model.hardware, false};
    model.hardware.read_context = &injection;
    model.hardware.before_read = [](void* context, uint32_t offset) {
        auto& change = *static_cast<Injection*>(context);
        if (offset != 0xB10U || change.fired)
            return;
        auto& hw = *change.hardware;
        const uint8_t prefix[4] = {0U, 0x42U, 2U, 0U};
        const uint32_t address = hw.registers[0xB14U / 4U];
        std::memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(address)),
                    prefix, sizeof(prefix));
        hw.registers[0xB14U / 4U] += 4U;
        hw.registers[0xB10U / 4U] &= ~(3U << 29U);
        hw.registers[0xB10U / 4U] |= 1U << 29U;
        hw.registers[0x10U / 4U] &= ~(1U << 31U);
        hw.registers[0x10U / 4U] |= 1U << 30U;
        change.fired = true;
    };
    nx_usb_controller_irq(&model.controller);
    model.hardware.before_read = nullptr;
    model.hardware.read_context = nullptr;
    EXPECT_TRUE(injection.fired);
    EXPECT_FALSE(model.controller.snapshot.closing);
    EXPECT_FALSE(model.controller.snapshot.control_pending);
    std::memcpy(staging[1].bytes + 12U, next + 4U, 4U);
    model.hardware.registers[0xB14U / 4U] =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(staging[1].bytes)) +
        16U;
    model.hardware.registers[0xB08U / 4U] |= 8U;
    model.hardware.registers[0x10U / 4U] &= ~(1U << 30U);
    model.hardware.registers[0x10U / 4U] |= 1U << 31U;
    nx_usb_controller_irq(&model.controller);
    EXPECT_FALSE(model.controller.snapshot.closing);
    EXPECT_TRUE(model.controller.snapshot.control_pending);
    EXPECT_EQ(model.controller.snapshot.setup.request, 0x42U);
    EXPECT_EQ(model.controller.snapshot.setup.value, 2U);
}

/** \brief STATUS ACK changes protocol permission before AHB loan withdrawal. */
class UsbRemoteAck : public UsbDeviceDma,
                     public testing::WithParamInterface<unsigned> {
  protected:
    nx_usb_remote_runtime_t remote{};
    nx_usb_remote_wakeup_request_t pulse{};
    void SetUp() override {
        UsbDeviceDma::SetUp();
        model.controller.remote = &remote;
        nx_request_initialize(&pulse.base);
    }
    void TearDown() override {
        model.hardware.refused_dma_address_offset = 0U;
        model.hardware.registers[0x10U / 4U] = 1U << 31U;
        (void)nx_native_clock_advance(2001U);
        if (model.controller.snapshot.started)
            (void)nx_usb_device_service(&device);
        UsbDeviceDma::TearDown();
    }
    void Control(uint8_t number, uint16_t value, bool hold_join = false,
                 unsigned withdraw_first = 0U) {
        const uint8_t setup[8] = {0U,
                                  number,
                                  static_cast<uint8_t>(value),
                                  static_cast<uint8_t>(value >> 8U),
                                  0U,
                                  0U,
                                  0U,
                                  0U};
        QueueSetups(setup, 1U);
        nx_usb_controller_irq(&model.controller);
        if (number == 5U) {
            ASSERT_EQ(NX_SUCCESS, nx_usb_device_address(
                                      &device, model.controller.snapshot.epoch,
                                      static_cast<uint8_t>(value)));
        }
        ASSERT_EQ(NX_SUCCESS,
                  nx_usb_in_prepare(&request, nullptr, 0U, NX_USB_STATUS,
                                    model.controller.snapshot.epoch,
                                    nx_time_now_us() + 30000U));
        const auto input = Port(0U);
        ASSERT_EQ(NX_SUCCESS, nx_usb_endpoint_submit(&input, &request));
        if (withdraw_first != 0U) {
            model.hardware.registers[0x10U / 4U] &= ~(1U << 31U);
            model.hardware.hold_disable = true;
            if (withdraw_first == 1U) {
                ASSERT_EQ(NX_SUCCESS, nx_usb_endpoint_cancel(&input, &request));
            } else {
                ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(30001U));
                ASSERT_EQ(NX_SUCCESS, nx_usb_device_service(&device));
            }
            ASSERT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
        }
        ASSERT_EQ(NX_SUCCESS, nx_native_usb_complete(&model, 0U, true));
        model.hardware.hold_disable = false;
        if (hold_join)
            model.hardware.registers[0x10U / 4U] &= ~(1U << 31U);
        nx_usb_controller_irq(&model.controller);
        if (!hold_join) {
            ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
            ASSERT_EQ(NX_SUCCESS, request.base.result);
        }
    }
};
TEST_P(UsbRemoteAck,
       WirePermissionCommitsWhileDmaStatusStorageRemainsBorrowed) {
    Start();
    Control(5U, 3U);
    Control(9U, 1U);
    if (GetParam() != 0U)
        Control(3U, 1U);
    const uint8_t numbers[] = {3U, 1U, 9U, 9U, 1U, 1U};
    const uint16_t values[] = {1U, 1U, 0U, 1U, 1U, 1U};
    Control(numbers[GetParam()], values[GetParam()], true,
            GetParam() >= 4U ? GetParam() - 3U : 0U);
    ASSERT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, endpoints[0].request);
    EXPECT_NE(0U, model.hardware.registers[0x914U / 4U]);
    const bool authorized = GetParam() == 0U;
    nx_usb_device_state_t state{};
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_state(&device, &state));
    EXPECT_EQ(authorized, state.remote_wakeup_enabled);
    EXPECT_FALSE(state.control_pending);
    model.hardware.registers[0x808U / 4U] |= 1U;
    model.hardware.registers[0x14U / 4U] |= 1U << 11U;
    nx_usb_controller_irq(&model.controller);
    ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(5001U));
    ASSERT_EQ(NX_SUCCESS,
              nx_request_prepare(&pulse.base, nx_time_now_us() + 10000U));
    pulse.max_service_delay_us = 100U;
    EXPECT_EQ(authorized ? NX_SUCCESS : NX_ERROR_PERMISSION,
              nx_usb_device_remote_wakeup(&device, &pulse));
    if (!authorized) {
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&pulse.base));
        EXPECT_EQ(nullptr, remote.request);
        EXPECT_EQ(0U, model.hardware.registers[0x804U / 4U] & 1U);
    }
    /* Wire permission never releases the independent STATUS DMA storage. */
    EXPECT_EQ(&request, endpoints[0].request);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    model.hardware.registers[0x10U / 4U] |= 1U << 31U;
    if (GetParam() >= 4U) {
        /* Withdrawal also retains its independent EPDISD observation. */
        ASSERT_EQ(NX_SUCCESS, nx_usb_device_service(&device));
        EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        EXPECT_EQ(&request, endpoints[0].request);
        model.hardware.registers[0x908U / 4U] |= 2U;
    }
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_service(&device));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
INSTANTIATE_TEST_SUITE_P(AcknowledgedFeatureAndConfiguration, UsbRemoteAck,
                         testing::Values(0U, 1U, 2U, 3U, 4U, 5U));
