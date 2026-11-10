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
    model.hardware.registers[0x10U / 4U] |= 1U << 31U;
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
    std::memcpy(staging[1].bytes, setup, sizeof(setup));
    model.hardware.registers[0xB00U / 4U] &= ~(1U << 31U);
    model.hardware.registers[0xB08U / 4U] |= 8U;
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
