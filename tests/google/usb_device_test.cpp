/**
 * \file            usb_device_test.cpp
 * \brief           Real full-speed PIO ownership and control regressions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/native/model.h"
#include "usb_device.h"
}
#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

class UsbDevice : public testing::Test {
  protected:
    nx_native_usb_device_t model = {};
    nx_usb_endpoint_t endpoints[4] = {};
    nx_usb_event_t events[4] = {};
    nx_usb_request_t request = {};
    uint8_t bytes[65] = {};
    nx_irq_wake_t wake = {nullptr, [](void*) { return NX_SUCCESS; }, false};
    const nx_usb_device_port_t device = {&nx_usb_fs_device_ops,
                                         &model.controller};
    nx_usb_endpoint_port_t Port(unsigned index) {
        return {&nx_usb_fs_endpoint_ops, &endpoints[index]};
    }
    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        auto& c = model.controller;
        c.endpoints = endpoints;
        c.endpoint_count = 4;
        c.events = events;
        c.capacity = 4;
        c.rx_fifo_words = 128;
        c.poll_limit = 4;
        c.drain_timeout_us = 20;
        c.irq_policy = {NX_IRQ_KERNEL_BASEPRI, 82, 4, 5};
        c.irq = {67, 5, 3};
        for (unsigned index = 0; index < 4; ++index) {
            endpoints[index].number = static_cast<uint8_t>(index / 2U);
            endpoints[index].in = (index % 2U) == 0;
            endpoints[index].bulk = index >= 2;
            endpoints[index].max_packet = 64;
        }
        endpoints[0].fifo_start = 128;
        endpoints[0].fifo_words = 32;
        endpoints[2].fifo_start = 160;
        endpoints[2].fifo_words = 32;
        model.hardware.clock_ready = true;
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_native_usb_initialize(&model), NX_SUCCESS);
    }
    void TearDown() override {
        model.hardware.hold_disable = false;
        model.hardware.hold_flush = false;
        model.hardware.hold_irq_disable = false;
        model.hardware.hold_clear = false;
        model.hardware.hold_gonak = false;
        model.hardware.hold_disconnect = model.hardware.hold_in_nak = false;
        model.hardware.on_source_disable = nullptr;
        model.hardware.registers[8U / 4U] &= ~(1U << 5U);
        EXPECT_EQ(nx_usb_device_stop(&device), NX_SUCCESS);
    }
    void ReceiveSetup(const uint8_t* data) {
        ASSERT_EQ(nx_native_usb_receive(&model, 0, data, 8, true), NX_SUCCESS);
        nx_usb_controller_irq(&model.controller);
    }
};
TEST_F(UsbDevice, FifoWriteDoesNotCompleteHostAcknowledgement) {
    auto port = Port(2);
    bytes[0] = 42;
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 1, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(model.hardware.tx_count[1], 1U);
    ASSERT_EQ(nx_native_usb_complete(&model, 1, true), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 1U);
    EXPECT_EQ(endpoints[2].request, nullptr);
}
TEST_F(UsbDevice, ShortOutCompletesOnlyAfterDataAndTransferFacts) {
    auto port = Port(3);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 64, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    const uint8_t packet[3] = {1, 2, 3};
    ASSERT_EQ(nx_native_usb_receive(&model, 1, packet, 3, false), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    ASSERT_EQ(nx_native_usb_complete(&model, 1, false), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(request.base.transferred, 3U);
    EXPECT_EQ(bytes[2], 3U);
    EXPECT_EQ(bytes[3], 0U);
}
TEST_F(UsbDevice, OutOverflowCannotWriteBeyondCallerCapacity) {
    auto port = Port(3);
    bytes[2] = 0xA5;
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 2, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    const uint8_t packet[3] = {1, 2, 3};
    ASSERT_EQ(nx_native_usb_receive(&model, 1, packet, 3, false), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_OVERFLOW);
    EXPECT_EQ(bytes[2], 0xA5);
}
TEST_F(UsbDevice, FailedDisableKeepsRequestBorrowedThroughDrainDeadline) {
    auto port = Port(2);
    model.hardware.hold_disable = true;
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 1, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_cancel(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(20), NX_SUCCESS);
    ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_usb_device_stop(&device), NX_ERROR_BUSY);
    EXPECT_EQ(endpoints[2].request, &request);
    EXPECT_EQ(model.hardware.consumers, 1U);
}
TEST_F(UsbDevice, FifoFlushFailureCannotSettleDisabledEndpoint) {
    auto port = Port(2);
    model.hardware.hold_flush = true;
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 1, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_cancel(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(20), NX_SUCCESS);
    ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(endpoints[2].request, &request);
}
TEST_F(UsbDevice, StopCannotReleaseWakeOrClockWithAnEnabledIrq) {
    ASSERT_EQ(nx_usb_device_attach_wake(&device, &wake, 0), NX_SUCCESS);
    model.hardware.hold_irq_disable = true;
    EXPECT_EQ(nx_usb_device_stop(&device), NX_ERROR_BUSY);
    EXPECT_EQ(model.controller.wake, &wake);
    EXPECT_EQ(model.hardware.consumers, 1U);
}
TEST_F(UsbDevice, NewSetupInvalidatesOldControlBeforeReusingStorage) {
    const uint8_t setup[8] = {0x80, 6, 0, 1, 0, 0, 4, 0};
    ReceiveSetup(setup);
    auto port = Port(0);
    uint64_t epoch = model.controller.snapshot.setup.epoch;
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 4, NX_USB_DATA, epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    model.hardware.hold_disable = true;
    ReceiveSetup(setup);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(nx_usb_device_stop(&device), NX_ERROR_BUSY);
    EXPECT_GT(model.controller.snapshot.epoch, epoch);
}
TEST_F(UsbDevice, SetupDecisionCannotStallAnAlreadyPublishedNewEpoch) {
    const uint8_t old_setup[8] = {0, 1, 0, 0, 0, 0, 1, 0};
    ReceiveSetup(old_setup);
    auto out = Port(1);
    uint64_t old_epoch = model.controller.snapshot.epoch;
    ASSERT_EQ(
        nx_usb_out_prepare(&request, bytes, 1, NX_USB_DATA, old_epoch, 200),
        NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&out, &request), NX_SUCCESS);
    const uint8_t data[1] = {42};
    ASSERT_EQ(nx_native_usb_receive(&model, 0, data, 1, false), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 0, false), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(request.base.result, NX_SUCCESS);
    nx_usb_device_state_t cached = {};
    ASSERT_EQ(nx_usb_device_state(&device, &cached), NX_SUCCESS);
    ASSERT_EQ(cached.epoch, old_epoch);
    ASSERT_TRUE(cached.control_pending);

    /* A real SETUP IRQ can run after this cached decision. Its normal progress
     * already clears setup_pending, so that flag cannot substitute for epoch.
     */
    const uint8_t next_setup[8] = {0x80, 6, 0, 1, 0, 0, 4, 0};
    ReceiveSetup(next_setup);
    ASSERT_GT(model.controller.snapshot.epoch, cached.epoch);
    ASSERT_FALSE(model.controller.setup_pending);
    ASSERT_EQ(endpoints[0].request, nullptr);
    ASSERT_EQ(endpoints[1].request, nullptr);
    uint32_t before = model.hardware.registers[0x900U / 4U];
    ASSERT_EQ(before & ((1U << 31U) | (1U << 21U)), 0U);
    ASSERT_FALSE(endpoints[0].halted);
    ASSERT_FALSE(endpoints[1].halted);
    const nx_usb_setup_decision_port_t decision = {
        &nx_usb_fs_setup_decision_ops, &model.controller};
    size_t reads = model.hardware.mmio_reads;
    size_t writes = model.hardware.mmio_writes;
    EXPECT_EQ(nx_usb_setup_reject(&decision, cached.epoch), NX_ERROR_STATE);
    EXPECT_EQ(model.hardware.mmio_reads, reads);
    EXPECT_EQ(model.hardware.mmio_writes, writes);
    EXPECT_EQ(model.hardware.registers[0x900U / 4U], before);
    EXPECT_FALSE(endpoints[0].halted);
    EXPECT_FALSE(endpoints[1].halted);
    EXPECT_TRUE(model.controller.snapshot.control_pending);

    ASSERT_EQ(nx_usb_setup_reject(&decision, model.controller.snapshot.epoch),
              NX_SUCCESS);
    EXPECT_GT(model.hardware.mmio_reads, reads);
    EXPECT_GT(model.hardware.mmio_writes, writes);
    EXPECT_NE(model.hardware.registers[0x900U / 4U] & (1U << 21U), 0U);
    EXPECT_TRUE(endpoints[0].halted);
    EXPECT_FALSE(endpoints[1].halted);
}
TEST_F(UsbDevice, SetupDecisionCannotRevokeAnActiveControlOutLoan) {
    const uint8_t setup[8] = {0, 1, 0, 0, 0, 0, 1, 0};
    ReceiveSetup(setup);
    auto out = Port(1);
    uint64_t epoch = model.controller.snapshot.epoch;
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 1, NX_USB_DATA, epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&out, &request), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    ASSERT_EQ(endpoints[0].request, nullptr);
    ASSERT_EQ(endpoints[1].request, &request);
    uint32_t before = model.hardware.registers[0x900U / 4U];
    const nx_usb_setup_decision_port_t decision = {
        &nx_usb_fs_setup_decision_ops, &model.controller};
    size_t writes = model.hardware.mmio_writes;
    EXPECT_EQ(nx_usb_setup_reject(&decision, epoch), NX_ERROR_BUSY);
    EXPECT_EQ(model.hardware.mmio_writes, writes);
    EXPECT_EQ(model.hardware.registers[0x900U / 4U], before);
    EXPECT_EQ(endpoints[1].request, &request);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(request.out, bytes);
    EXPECT_EQ(model.hardware.consumers, 1U);
    EXPECT_FALSE(endpoints[0].halted);
    EXPECT_FALSE(model.hardware.out_zero_disable_requested);

    /* Only the explicit controller owner requests the independent full-stop
     * proof. The rejected decision never cancels or releases the OUT0 loan. */
    ASSERT_EQ(nx_usb_device_stop(&device), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(endpoints[1].request, nullptr);
    EXPECT_EQ(model.hardware.consumers, 0U);
}
TEST_F(UsbDevice, AddressChangesOnlyAfterMatchingStatusInAcknowledgement) {
    const uint8_t setup[8] = {0, 5, 7, 0, 0, 0, 0, 0};
    ReceiveSetup(setup);
    uint64_t epoch = model.controller.snapshot.epoch;
    ASSERT_EQ(nx_usb_device_address(&device, epoch, 7), NX_SUCCESS);
    EXPECT_EQ(model.hardware.registers[0x800 / 4] & 0x7F0U, 0U);
    auto port = Port(0);
    ASSERT_EQ(
        nx_usb_in_prepare(&request, nullptr, 0, NX_USB_STATUS, epoch, 200),
        NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 0, true), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(model.hardware.registers[0x800 / 4] & 0x7F0U, 7U << 4U);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}
TEST_F(UsbDevice, ResetDoesNotRevokeAnUnprovedEndpointLoan) {
    auto port = Port(2);
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 4, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    model.hardware.hold_flush = true;
    nx_native_usb_reset(&model);
    nx_usb_controller_irq(&model.controller);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_usb_device_stop(&device), NX_ERROR_BUSY);
}
TEST_F(UsbDevice, StaleCompletionCannotFinishANewRequestEpoch) {
    auto port = Port(2);
    model.hardware.registers[(0x900U + 0x20U + 8U) / 4U] = 1;
    model.hardware.hold_clear = true;
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 4, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&port, &request), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
TEST_F(UsbDevice, IllegalPacketAndIncomingMaskRejectWithNoBorrow) {
    auto port = Port(2);
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 65, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&port, &request), NX_ERROR_INVALID);
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 4, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    auto mask = nx_arch_irq_save();
    EXPECT_EQ(nx_usb_endpoint_submit(&port, &request), NX_ERROR_CONTEXT);
    nx_arch_irq_restore(mask);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
TEST_F(UsbDevice, StallRequiresIdleAndClearResetsBulkDataPid) {
    auto port = Port(2);
    ASSERT_EQ(nx_usb_endpoint_stall(&port, true), NX_SUCCESS);
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 4, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&port, &request), NX_ERROR_STATE);
    ASSERT_EQ(nx_usb_endpoint_stall(&port, false), NX_SUCCESS);
    EXPECT_EQ(model.hardware.registers[0x920U / 4U] & (1U << 21U), 0U);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_stall(&port, true), NX_ERROR_BUSY);
}
TEST_F(UsbDevice, FullEventsPreserveAuthoritativeLatestSetup) {
    const uint8_t setup[8] = {0x80, 6, 0, 1, 0, 0, 4, 0};
    for (unsigned i = 0; i < 6; ++i) {
        ReceiveSetup(setup);
    }
    nx_usb_device_state_t state = {};
    ASSERT_EQ(nx_usb_device_state(&device, &state), NX_SUCCESS);
    EXPECT_EQ(state.setup.epoch, 6U);
    EXPECT_EQ(state.lost_events, 2U);
    nx_usb_event_t output[5] = {};
    size_t count = 0;
    ASSERT_EQ(nx_usb_device_read_events(&device, output, 5, &count),
              NX_SUCCESS);
    EXPECT_EQ(count, 5U);
    EXPECT_EQ(output[4].kind, NX_USB_EVENT_LOSS);
    EXPECT_EQ(output[4].lost, 2U);
}

TEST_F(UsbDevice, SubsequentControlPacketsAvoidReservedEpZeroPidCommands) {
    const uint8_t setup[8] = {0x80, 6, 0, 1, 0, 0, 68, 0};
    ReceiveSetup(setup);
    auto port = Port(0);
    uint64_t epoch = model.controller.snapshot.epoch;
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 64, NX_USB_DATA, epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 0, true), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 4, NX_USB_DATA, epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    EXPECT_FALSE(model.hardware.reserved_ep0_pid_command);
    EXPECT_EQ(model.hardware.registers[0x900U / 4U] &
                  ((3U << 28U) | (1U << 16U)),
              0U);
}
TEST_F(UsbDevice, SetupPhaseInterruptDoesNotBlockTheFollowingStatusOut) {
    const uint8_t setup[8] = {0x80, 6, 0, 1, 0, 0, 4, 0};
    model.hardware.registers[(0xB00U + 8U) / 4U] |= 8U;
    ReceiveSetup(setup);
    auto in = Port(0);
    auto out = Port(1);
    uint64_t epoch = model.controller.snapshot.epoch;
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 4, NX_USB_DATA, epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&in, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 0, true), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    ASSERT_EQ(
        nx_usb_out_prepare(&request, nullptr, 0, NX_USB_STATUS, epoch, 200),
        NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&out, &request), NX_SUCCESS);
}
TEST_F(UsbDevice, ResetIrqDoesNotUseTheColdUnboundedAcknowledgementLoop) {
    model.controller.poll_limit = 10000;
    model.hardware.reset_reads = 0;
    model.hardware.hold_flush = true;
    nx_native_usb_reset(&model);
    nx_usb_controller_irq(&model.controller);
    EXPECT_LE(model.hardware.reset_reads, 50U);
    EXPECT_TRUE(model.controller.reset_pending);
}
TEST_F(UsbDevice, EnumerationDoneIsAcknowledgedFromTheObservedSpeedFact) {
    model.hardware.registers[0x808U / 4U] = 6U;
    model.hardware.registers[0x14U / 4U] |= 1U << 13U;
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(model.hardware.registers[0x14U / 4U] & (1U << 13U), 0U);
}
TEST_F(UsbDevice, OutCompletionWaitsForTheBoundedRxHeaderToBeConsumed) {
    auto port = Port(3);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 64, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    for (unsigned i = 0; i < 8; ++i) {
        ASSERT_EQ(nx_native_usb_receive(&model, 2, nullptr, 0, false),
                  NX_SUCCESS);
    }
    const uint8_t data[3] = {4, 5, 6};
    ASSERT_EQ(nx_native_usb_receive(&model, 1, data, 3, false), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 1, false), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 3U);
    EXPECT_EQ(bytes[2], 6U);
}
TEST_F(UsbDevice, PacketBuffersCannotAliasTheClockConsumerControl) {
    auto port = Port(3);
    auto pointer = reinterpret_cast<uint8_t*>(&model.hardware.consumers);
    ASSERT_EQ(nx_usb_out_prepare(&request, pointer, sizeof(uint32_t),
                                 NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
TEST_F(UsbDevice, RestartDoesNotReuseAnEarlierControlEpoch) {
    const uint8_t setup[8] = {0, 5, 7, 0, 0, 0, 0, 0};
    ReceiveSetup(setup);
    uint64_t old = model.controller.snapshot.epoch;
    ASSERT_EQ(nx_usb_device_stop(&device), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_initialize(&model), NX_SUCCESS);
    ReceiveSetup(setup);
    EXPECT_GT(model.controller.snapshot.epoch, old);
}

TEST_F(UsbDevice, CancellingOutRestoresPeerAdmissionAfterActualDrain) {
    auto port = Port(3);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 4, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_cancel(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(model.hardware.registers[0x804U / 4U] & 8U, 0U);
}
TEST_F(UsbDevice, UnknownNonControlOutEnableCannotBeOverwrittenAtAdmission) {
    auto port = Port(3);
    model.hardware.registers[0xB20U / 4U] |= 1U << 31U;
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 4, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&port, &request), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
TEST_F(UsbDevice, PendingResetAtGlobalEnableRemainsObservableAfterColdStart) {
    ASSERT_EQ(nx_usb_device_stop(&device), NX_SUCCESS);
    model.hardware.registers[0x14U / 4U] |= 1U << 12U;
    model.hardware.on_interrupt_enable = [](void* context) {
        auto m = static_cast<nx_native_usb_device_t*>(context);
        nx_native_usb_reset(m);
        nx_usb_controller_irq(&m->controller);
    };
    model.hardware.interrupt_context = &model;
    ASSERT_EQ(nx_native_usb_initialize(&model), NX_SUCCESS);
    model.hardware.on_interrupt_enable = nullptr;
    nx_usb_event_t output = {};
    size_t count = 0;
    EXPECT_EQ(nx_usb_device_read_events(&device, &output, 1, &count),
              NX_SUCCESS);
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(output.kind, NX_USB_EVENT_RESET);
}
TEST_F(UsbDevice, StopAndDetachJoinTheInFlightWakePublisher) {
    struct Barrier {
        std::mutex mutex;
        std::condition_variable cv;
        bool entered = false;
        bool released = false;
    } barrier;
    nx_irq_wake_t blocking = {&barrier,
                              [](void* context) {
                                  auto b = static_cast<Barrier*>(context);
                                  std::unique_lock<std::mutex> lock(b->mutex);
                                  b->entered = true;
                                  b->cv.notify_one();
                                  b->cv.wait(lock, [&] { return b->released; });
                                  return NX_SUCCESS;
                              },
                              false};
    ASSERT_EQ(nx_usb_device_attach_wake(&device, &blocking, 0), NX_SUCCESS);
    std::thread irq([&] { nx_usb_controller_irq(&model.controller); });
    bool entered = false;
    {
        std::unique_lock<std::mutex> lock(barrier.mutex);
        entered = barrier.cv.wait_for(lock, std::chrono::seconds(1),
                                      [&] { return barrier.entered; });
    }
    EXPECT_TRUE(entered);
    if (entered) {
        EXPECT_EQ(nx_usb_device_attach_wake(&device, nullptr, 0),
                  NX_ERROR_BUSY);
        EXPECT_EQ(nx_usb_device_stop(&device), NX_ERROR_BUSY);
        EXPECT_EQ(model.hardware.consumers, 1U);
    }
    {
        std::lock_guard<std::mutex> lock(barrier.mutex);
        barrier.released = true;
    }
    barrier.cv.notify_one();
    irq.join();
    EXPECT_EQ(nx_usb_device_stop(&device), NX_SUCCESS);
}

TEST_F(UsbDevice, StateOutputsCannotOverwriteControllerOrClockStorage) {
    const auto original = model.controller;
    const auto hardware = model.hardware;
    auto result = nx_usb_device_state(
        &device, reinterpret_cast<nx_usb_device_state_t*>(&model.controller));
    model.controller = original;
    EXPECT_EQ(result, NX_ERROR_INVALID);
    result = nx_usb_device_state(
        &device,
        reinterpret_cast<nx_usb_device_state_t*>(&model.hardware.consumers));
    model.hardware = hardware;
    EXPECT_EQ(result, NX_ERROR_INVALID);
}
TEST_F(UsbDevice, EventCountCannotAliasControllerQueueMetadata) {
    const uint8_t setup[8] = {0, 5, 7, 0, 0, 0, 0, 0};
    ReceiveSetup(setup);
    nx_usb_event_t output = {};
    auto result =
        nx_usb_device_read_events(&device, &output, 1, &model.controller.count);
    auto count = model.controller.count;
    model.controller.count = 1;
    EXPECT_EQ(result, NX_ERROR_INVALID);
    EXPECT_EQ(count, 1U);
}
TEST_F(UsbDevice, EventOutputsCannotOverwriteClockOrAcceptedOutRequest) {
    const uint8_t setup[8] = {0, 5, 7, 0, 0, 0, 0, 0};
    ReceiveSetup(setup);
    size_t count = 77;
    auto original = model.controller;
    const auto hardware = model.hardware;
    auto result = nx_usb_device_read_events(
        &device, reinterpret_cast<nx_usb_event_t*>(&model.hardware.consumers),
        1, &count);
    model.controller = original;
    model.hardware = hardware;
    EXPECT_EQ(result, NX_ERROR_INVALID);
    EXPECT_EQ(count, 77U);
    auto port = Port(3);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 64, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    const auto saved_request = request;
    count = 77;
    result = nx_usb_device_read_events(
        &device, reinterpret_cast<nx_usb_event_t*>(&request), 1, &count);
    model.controller = original;
    request = saved_request;
    EXPECT_EQ(result, NX_ERROR_INVALID);
    EXPECT_EQ(count, 77U);
}
TEST_F(UsbDevice, AttachedWakeObjectCannotBecomeAnOutBuffer) {
    ASSERT_EQ(nx_usb_device_attach_wake(&device, &wake, 0), NX_SUCCESS);
    auto port = Port(3);
    ASSERT_EQ(nx_usb_out_prepare(&request, reinterpret_cast<uint8_t*>(&wake),
                                 sizeof(wake), NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
TEST_F(UsbDevice, WakeCannotAttachInsideAnAlreadyBorrowedOutBuffer) {
    auto port = Port(3);
    ASSERT_EQ(nx_usb_out_prepare(&request, reinterpret_cast<uint8_t*>(&wake),
                                 sizeof(wake), NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_usb_device_attach_wake(&device, &wake, 0), NX_ERROR_INVALID);
    EXPECT_EQ(model.controller.wake, nullptr);
}
TEST_F(UsbDevice, EventPlanCannotOverwriteAnImmutableClockTable) {
    ASSERT_EQ(nx_usb_device_stop(&device), NX_SUCCESS);
    union Storage {
        nx_usb_clock_ops_t ops;
        nx_usb_event_t events[4];
    } storage = {};
    storage.ops = *model.controller.clock.ops;
    model.controller.clock.ops = &storage.ops;
    model.controller.events = storage.events;
    EXPECT_EQ(nx_usb_controller_initialize(&model.controller),
              NX_ERROR_INVALID);
    EXPECT_EQ(model.hardware.consumers, 0U);
    model.controller.clock.ops = nullptr;
    /* Stop any wrongly accepted cold plan before the local immutable table
     * expires.
     */
    if (model.controller.snapshot.started) {
        model.controller.clock.ops = &storage.ops;
        EXPECT_EQ(nx_usb_device_stop(&device), NX_SUCCESS);
    }
    model.controller.events = events;
}

TEST_F(UsbDevice, ReadOnlyTxEmptyFlagCannotPreventColdStartOrSettlement) {
    EXPECT_NE(nx_native_usb_read(reinterpret_cast<uintptr_t>(&model.hardware),
                                 0x908U) &
                  128U,
              0U);
    auto port = Port(2);
    ASSERT_EQ(nx_usb_in_prepare(&request, nullptr, 0, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 1, true), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}
TEST_F(UsbDevice, EmptyInTokenIsRetryInformationRatherThanATransferFailure) {
    auto port = Port(2);
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 1, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    model.hardware.registers[0x928U / 4U] |= 16U;
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    ASSERT_EQ(nx_native_usb_complete(&model, 1, true), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}
TEST_F(UsbDevice, OutSetupDoneIsNotAnErrorOfTheFollowingControlData) {
    const uint8_t setup[8] = {0, 1, 0, 0, 0, 0, 4, 0};
    ReceiveSetup(setup);
    auto port = Port(1);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 4, NX_USB_DATA,
                                 model.controller.snapshot.epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    model.hardware.registers[0xB08U / 4U] |= 8U;
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
}
TEST_F(UsbDevice, ResetDiscardsAnUnorderedPendingSetupFromThePriorBusEpoch) {
    const uint8_t setup[8] = {0, 5, 7, 0, 0, 0, 0, 0};
    ASSERT_EQ(nx_native_usb_receive(&model, 0, setup, 8, true), NX_SUCCESS);
    nx_native_usb_reset(&model);
    nx_usb_controller_irq(&model.controller);
    EXPECT_FALSE(model.controller.snapshot.control_pending);
    EXPECT_EQ(
        nx_usb_device_address(&device, model.controller.snapshot.epoch, 7),
        NX_ERROR_STATE);
    nx_usb_event_t output[2] = {};
    size_t count = 0;
    ASSERT_EQ(nx_usb_device_read_events(&device, output, 2, &count),
              NX_SUCCESS);
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(output[0].kind, NX_USB_EVENT_RESET);
}
TEST_F(UsbDevice, InDisableWaitsForAnObservedNakEffectiveState) {
    auto port = Port(2);
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 4, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    model.hardware.hold_in_nak = true;
    ASSERT_EQ(nx_usb_endpoint_cancel(&port, &request), NX_SUCCESS);
    EXPECT_FALSE(model.hardware.early_in_disable);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    model.hardware.hold_in_nak = false;
    ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}
TEST_F(UsbDevice, OutZeroFixedDisableBitUsesProvedPioFifoWithdrawal) {
    /* OUT0 EPD is fixed zero.  Global OUT NAK + consumed RX FIFO are the
     * full-stop proof for PIO; individual cancel retains the loan because
     * SETUP can bypass NAK. No DMA pointer exists in this implementation.
     */
    const uint8_t setup[8] = {0, 1, 0, 0, 0, 0, 4, 0};
    ReceiveSetup(setup);
    auto port = Port(1);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 4, NX_USB_DATA,
                                 model.controller.snapshot.epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_cancel(&port, &request), NX_SUCCESS);
    EXPECT_FALSE(model.hardware.out_zero_disable_requested);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    ASSERT_EQ(nx_native_clock_advance(20), NX_SUCCESS);
    ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_usb_device_stop(&device), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}
TEST_F(UsbDevice, OutZeroStopRetainsLoanWhenDisconnectReadbackFails) {
    const uint8_t setup[8] = {0, 1, 0, 0, 0, 0, 4, 0};
    ReceiveSetup(setup);
    auto port = Port(1);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 4, NX_USB_DATA,
                                 model.controller.snapshot.epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    model.hardware.hold_disconnect = true;
    EXPECT_EQ(nx_usb_device_stop(&device), NX_ERROR_BUSY);
    EXPECT_EQ(endpoints[1].request, &request);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(model.hardware.consumers, 1U);
}
TEST_F(UsbDevice, OutZeroStopRejectsAForeignEnabledDmaEngine) {
    const uint8_t setup[8] = {0, 1, 0, 0, 0, 0, 4, 0};
    ReceiveSetup(setup);
    auto port = Port(1);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 4, NX_USB_DATA,
                                 model.controller.snapshot.epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    model.hardware.registers[8U / 4U] |= 1U << 5U;
    EXPECT_EQ(nx_usb_device_stop(&device), NX_ERROR_BUSY);
    EXPECT_EQ(endpoints[1].request, &request);
    EXPECT_EQ(model.hardware.consumers, 1U);
}
TEST_F(UsbDevice, LateSetupCannotReopenAClosingControlEpoch) {
    model.hardware.hold_irq_disable = true;
    ASSERT_EQ(nx_usb_device_stop(&device), NX_ERROR_BUSY);
    const uint8_t setup[8] = {0, 5, 7, 0, 0, 0, 0, 0};
    ASSERT_EQ(nx_native_usb_receive(&model, 0, setup, 8, true), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_FALSE(model.controller.snapshot.control_pending);
    EXPECT_EQ(
        nx_usb_device_address(&device, model.controller.snapshot.epoch, 7),
        NX_ERROR_STATE);
}
TEST_F(UsbDevice, OutZeroStopWaitsForRxFifoAndIrqDetachBeforeSettlement) {
    const uint8_t setup[8] = {0, 1, 0, 0, 0, 0, 4, 0};
    ReceiveSetup(setup);
    auto port = Port(1);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 4, NX_USB_DATA,
                                 model.controller.snapshot.epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    for (unsigned i = 0; i < 9; ++i) {
        ASSERT_EQ(nx_native_usb_receive(&model, 0, nullptr, 0, false),
                  NX_SUCCESS);
    }
    EXPECT_EQ(nx_usb_device_stop(&device), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    model.hardware.hold_irq_disable = true;
    EXPECT_EQ(nx_usb_device_stop(&device), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    model.hardware.hold_irq_disable = false;
    EXPECT_EQ(nx_usb_device_stop(&device), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}
TEST_F(UsbDevice, LateDisconnectLossCannotRevokeAnOutZeroLoan) {
    const uint8_t setup[8] = {0, 1, 0, 0, 0, 0, 4, 0};
    ReceiveSetup(setup);
    auto port = Port(1);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 4, NX_USB_DATA,
                                 model.controller.snapshot.epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&port, &request), NX_SUCCESS);
    model.hardware.on_source_disable = [](void* context) {
        auto hw = static_cast<nx_native_usb_hardware_t*>(context);
        hw->registers[0x804U / 4U] &= ~2U;
    };
    model.hardware.disable_context = &model.hardware;
    EXPECT_EQ(nx_usb_device_stop(&device), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(model.hardware.consumers, 1U);
    model.hardware.on_source_disable = nullptr;
}
TEST_F(UsbDevice, StallDoesNotModifyAnUnknownEnabledBulkEndpoint) {
    auto port = Port(2);
    auto original = model.hardware.registers[0x920U / 4U] | (1U << 31U);
    model.hardware.registers[0x920U / 4U] = original;
    EXPECT_EQ(nx_usb_endpoint_stall(&port, true), NX_ERROR_BUSY);
    EXPECT_EQ(model.hardware.registers[0x920U / 4U], original);
}
TEST_F(UsbDevice, ClearStallRequiresActualBulkDataZeroReadback) {
    auto port = Port(2);
    ASSERT_EQ(nx_usb_endpoint_stall(&port, true), NX_SUCCESS);
    model.hardware.registers[0x920U / 4U] |= 1U << 16U;
    model.hardware.hold_bulk_pid = true;
    EXPECT_EQ(nx_usb_endpoint_stall(&port, false), NX_ERROR_IO);
    EXPECT_TRUE(endpoints[2].halted);
    model.hardware.hold_bulk_pid = false;
}

TEST(UsbMmioFacts, RefusedAddressAndStallClearsPreserveSelectedHardwareBits) {
    nx_native_usb_hardware_t hardware = {};
    auto base = reinterpret_cast<uintptr_t>(&hardware);
    hardware.registers[0x800U / 4U] = 3U | (7U << 4U) | (1U << 11U);
    hardware.hold_address_clear = true;
    nx_native_usb_write(base, 0x800U, 3U | (2U << 11U));
    EXPECT_EQ(hardware.registers[0x800U / 4U], 3U | (7U << 4U) | (2U << 11U));
    hardware.hold_address_clear = false;
    nx_native_usb_write(base, 0x800U, 3U | (2U << 11U));
    EXPECT_EQ(hardware.registers[0x800U / 4U], 3U | (2U << 11U));
    for (uint8_t mask : {uint8_t{1}, uint8_t{16}}) {
        hardware.registers[0x900U / 4U] = 1U << 21U;
        hardware.registers[0xB00U / 4U] = 1U << 21U;
        hardware.hold_stall_clear_mask = mask;
        nx_native_usb_write(base, 0x900U, 1U << 15U);
        nx_native_usb_write(base, 0xB00U, 1U << 15U);
        EXPECT_EQ(hardware.registers[0x900U / 4U] & (1U << 21U),
                  mask == 1U ? 1U << 21U : 0U);
        EXPECT_EQ(hardware.registers[0xB00U / 4U] & (1U << 21U),
                  mask == 16U ? 1U << 21U : 0U);
    }
}

TEST_F(UsbDevice, ResetKeepsAdmissionClosedUntilAddressZeroIsObserved) {
    const uint8_t setup[8] = {0, 5, 7, 0, 0, 0, 0, 0};
    ReceiveSetup(setup);
    auto in0 = Port(0);
    auto bulk = Port(2);
    uint64_t epoch = model.controller.snapshot.epoch;
    ASSERT_EQ(nx_usb_device_address(&device, epoch, 7), NX_SUCCESS);
    ASSERT_EQ(
        nx_usb_in_prepare(&request, nullptr, 0, NX_USB_STATUS, epoch, 200),
        NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&in0, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 0, true), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    ASSERT_EQ(request.base.result, NX_SUCCESS);
    model.hardware.registers[0x800U / 4U] |= 1U << 11U;
    model.hardware.hold_address_clear = true;
    nx_native_usb_reset(&model);
    nx_usb_controller_irq(&model.controller);
    nx_usb_device_state_t snapshot = {};
    ASSERT_EQ(nx_usb_device_state(&device, &snapshot), NX_SUCCESS);
    EXPECT_EQ(model.hardware.registers[0x800U / 4U],
              3U | (7U << 4U) | (1U << 11U));
    EXPECT_EQ(snapshot.address, 7U);
    EXPECT_FALSE(snapshot.control_pending);
    EXPECT_EQ(snapshot.epoch, epoch + 1U);
    EXPECT_TRUE(model.controller.reset_pending);
    EXPECT_EQ(model.hardware.consumers, 1U);
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 1, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&bulk, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    model.hardware.hold_address_clear = false;
    ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
    ASSERT_EQ(nx_usb_device_state(&device, &snapshot), NX_SUCCESS);
    EXPECT_EQ(model.hardware.registers[0x800U / 4U], 3U | (1U << 11U));
    EXPECT_EQ(snapshot.address, 0U);
    EXPECT_EQ(snapshot.epoch, epoch + 1U);
    EXPECT_FALSE(model.controller.reset_pending);
    ASSERT_EQ(nx_usb_endpoint_submit(&bulk, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 1, true), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}

TEST_F(UsbDevice, ResetClearsSelectedBulkHaltOnlyAfterHardwareReadback) {
    auto in = Port(2);
    auto out = Port(3);
    ASSERT_EQ(nx_usb_endpoint_stall(&in, true), NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_stall(&out, true), NX_SUCCESS);
    model.hardware.hold_stall_clear_mask = 1U << 5U;
    nx_native_usb_reset(&model);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(model.hardware.registers[0x920U / 4U] & (1U << 21U), 0U);
    EXPECT_NE(model.hardware.registers[0xB20U / 4U] & (1U << 21U), 0U);
    EXPECT_TRUE(model.controller.reset_pending);
    EXPECT_TRUE(endpoints[2].halted);
    EXPECT_TRUE(endpoints[3].halted);
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 1, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&in, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    model.hardware.hold_stall_clear_mask = 0;
    ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
    EXPECT_FALSE(model.controller.reset_pending);
    EXPECT_FALSE(endpoints[2].halted);
    EXPECT_FALSE(endpoints[3].halted);
    EXPECT_EQ(model.hardware.registers[0x920U / 4U] & (1U << 21U), 0U);
    EXPECT_EQ(model.hardware.registers[0xB20U / 4U] & (1U << 21U), 0U);
    ASSERT_EQ(nx_usb_endpoint_submit(&in, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 1, true), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    ASSERT_EQ(nx_usb_out_prepare(&request, nullptr, 0, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&out, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_receive(&model, 1, nullptr, 0, false), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 1, false), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_FALSE(model.hardware.reserved_ep0_pid_command);
}

TEST_F(UsbDevice, SetupKeepsControlAdmissionBlockedUntilBothStallsAreClear) {
    const uint8_t setup[8] = {0, 1, 0, 0, 0, 0, 0, 0};
    auto in = Port(0);
    auto out = Port(1);
    for (uint8_t mask : {uint8_t{1}, uint8_t{16}}) {
        ASSERT_EQ(nx_usb_endpoint_stall(&in, true), NX_SUCCESS);
        ASSERT_EQ(nx_usb_endpoint_stall(&out, true), NX_SUCCESS);
        model.hardware.hold_stall_clear_mask = mask;
        ReceiveSetup(setup);
        uint64_t epoch = model.controller.snapshot.epoch;
        EXPECT_TRUE(model.controller.setup_pending);
        EXPECT_TRUE(endpoints[0].halted);
        EXPECT_TRUE(endpoints[1].halted);
        uint32_t held = mask == 1U ? 0x900U : 0xB00U;
        EXPECT_NE(model.hardware.registers[held / 4U] & (1U << 21U), 0U);
        ASSERT_EQ(
            nx_usb_in_prepare(&request, nullptr, 0, NX_USB_STATUS, epoch, 200),
            NX_SUCCESS);
        ASSERT_NE(nx_usb_endpoint_submit(&in, &request), NX_SUCCESS);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
        model.hardware.hold_stall_clear_mask = 0;
        ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
        EXPECT_EQ(model.controller.snapshot.epoch, epoch);
        EXPECT_FALSE(model.controller.setup_pending);
        EXPECT_FALSE(endpoints[0].halted);
        EXPECT_FALSE(endpoints[1].halted);
        EXPECT_EQ(model.hardware.registers[0x900U / 4U] & (1U << 21U), 0U);
        EXPECT_EQ(model.hardware.registers[0xB00U / 4U] & (1U << 21U), 0U);
        ASSERT_EQ(nx_usb_endpoint_submit(&in, &request), NX_SUCCESS);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
        ASSERT_EQ(nx_native_usb_complete(&model, 0, true), NX_SUCCESS);
        nx_usb_controller_irq(&model.controller);
        EXPECT_EQ(request.base.result, NX_SUCCESS);
        EXPECT_FALSE(model.controller.snapshot.control_pending);
    }
    EXPECT_FALSE(model.hardware.reserved_ep0_pid_command);
}

TEST_F(UsbDevice, ControlShortResponseRequiresTerminatingZlpAcknowledgement) {
    const uint8_t setup[8] = {0x80, 6, 0, 1, 0, 0, 128, 0};
    ReceiveSetup(setup);
    auto in = Port(0);
    auto out = Port(1);
    uint64_t epoch = model.controller.snapshot.epoch;
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 64, NX_USB_DATA, epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&in, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 0, true), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    ASSERT_EQ(request.base.result, NX_SUCCESS);
    ASSERT_EQ(
        nx_usb_out_prepare(&request, nullptr, 0, NX_USB_STATUS, epoch, 200),
        NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&out, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    ASSERT_EQ(nx_usb_in_prepare(&request, nullptr, 0, NX_USB_DATA, epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&in, &request), NX_SUCCESS);
    ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(model.hardware.tx_count[0], 0U);
    ASSERT_EQ(nx_native_usb_complete(&model, 0, true), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(
        nx_usb_out_prepare(&request, nullptr, 0, NX_USB_STATUS, epoch, 200),
        NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&out, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 0, false), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_TRUE(model.controller.snapshot.control_pending);
    ASSERT_EQ(nx_native_usb_receive(&model, 0, nullptr, 0, false), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_FALSE(model.controller.snapshot.control_pending);
    EXPECT_FALSE(model.hardware.reserved_ep0_pid_command);
}

TEST_F(UsbDevice, ExactControlLengthRejectsAnExtraDataZlp) {
    const uint8_t setup[8] = {0x80, 6, 0, 1, 0, 0, 64, 0};
    ReceiveSetup(setup);
    auto in = Port(0);
    auto out = Port(1);
    uint64_t epoch = model.controller.snapshot.epoch;
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 64, NX_USB_DATA, epoch, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&in, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 0, true), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    ASSERT_EQ(nx_usb_in_prepare(&request, nullptr, 0, NX_USB_DATA, epoch, 200),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&in, &request), NX_ERROR_INVALID);
    ASSERT_EQ(
        nx_usb_out_prepare(&request, nullptr, 0, NX_USB_STATUS, epoch, 200),
        NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&out, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_receive(&model, 0, nullptr, 0, false), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    ASSERT_EQ(nx_native_usb_complete(&model, 0, false), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_FALSE(model.controller.snapshot.control_pending);
}

TEST_F(UsbDevice, SupersededTerminatingZlpRetainsItsLoanUntilActualDrain) {
    const uint8_t read[8] = {0x80, 6, 0, 1, 0, 0, 128, 0};
    const uint8_t next[8] = {0, 1, 0, 0, 0, 0, 0, 0};
    auto in = Port(0);
    for (bool reset : {false, true}) {
        ReceiveSetup(read);
        uint64_t epoch = model.controller.snapshot.epoch;
        ASSERT_EQ(
            nx_usb_in_prepare(&request, bytes, 64, NX_USB_DATA, epoch, 200),
            NX_SUCCESS);
        ASSERT_EQ(nx_usb_endpoint_submit(&in, &request), NX_SUCCESS);
        ASSERT_EQ(nx_native_usb_complete(&model, 0, true), NX_SUCCESS);
        nx_usb_controller_irq(&model.controller);
        ASSERT_EQ(
            nx_usb_in_prepare(&request, nullptr, 0, NX_USB_DATA, epoch, 200),
            NX_SUCCESS);
        ASSERT_EQ(nx_usb_endpoint_submit(&in, &request), NX_SUCCESS);
        model.hardware.hold_disable = true;
        if (reset) {
            nx_native_usb_reset(&model);
            nx_usb_controller_irq(&model.controller);
        } else {
            ReceiveSetup(next);
        }
        EXPECT_GT(model.controller.snapshot.epoch, epoch);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
        EXPECT_EQ(endpoints[0].request, &request);
        EXPECT_EQ(model.hardware.consumers, 1U);
        ASSERT_EQ(nx_native_clock_advance(20), NX_SUCCESS);
        ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
        EXPECT_EQ(endpoints[0].request, &request);
        model.hardware.hold_disable = false;
        ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
        EXPECT_EQ(endpoints[0].request, nullptr);
        ASSERT_EQ(
            nx_usb_in_prepare(&request, nullptr, 0, NX_USB_STATUS, epoch, 200),
            NX_SUCCESS);
        EXPECT_EQ(nx_usb_endpoint_submit(&in, &request), NX_ERROR_INVALID);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    }
    EXPECT_FALSE(model.hardware.reserved_ep0_pid_command);
}

TEST_F(UsbDevice, ResetRetainsHaltsUntilNonOutZeroDirectionsAreInactive) {
    for (unsigned index : {0U, 2U, 3U}) {
        auto port = Port(index);
        ASSERT_EQ(nx_usb_endpoint_stall(&port, true), NX_SUCCESS);
        uint32_t offset = (endpoints[index].in ? 0x900U : 0xB00U) +
                          uint32_t{endpoints[index].number} * 32U;
        model.hardware.registers[offset / 4U] |= 1U << 31U;
        nx_native_usb_reset(&model);
        nx_usb_controller_irq(&model.controller);
        nx_usb_device_state_t snapshot = {};
        ASSERT_EQ(nx_usb_device_state(&device, &snapshot), NX_SUCCESS);
        EXPECT_TRUE(snapshot.started);
        EXPECT_FALSE(snapshot.closing);
        EXPECT_FALSE(snapshot.control_pending);
        EXPECT_TRUE(model.controller.reset_pending);
        EXPECT_TRUE(endpoints[index].halted);
        EXPECT_NE(model.hardware.registers[offset / 4U] & (1U << 31U), 0U);
        EXPECT_NE(model.hardware.registers[offset / 4U] & (1U << 21U), 0U);
        EXPECT_EQ(model.hardware.consumers, 1U);
        nx_result_t prepared =
            endpoints[index].in
                ? nx_usb_in_prepare(&request, bytes, 1, NX_USB_DATA, 0, 200)
                : nx_usb_out_prepare(&request, bytes, 1, NX_USB_DATA, 0, 200);
        ASSERT_EQ(prepared, NX_SUCCESS);
        EXPECT_EQ(nx_usb_endpoint_submit(&port, &request), NX_ERROR_STATE);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
        model.hardware.registers[offset / 4U] &= ~(1U << 31U);
        ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
        EXPECT_FALSE(model.controller.reset_pending);
        EXPECT_FALSE(endpoints[index].halted);
        EXPECT_EQ(model.hardware.registers[offset / 4U] & (1U << 21U), 0U);
    }
    EXPECT_FALSE(model.hardware.reserved_ep0_pid_command);
}

TEST_F(UsbDevice, ResetRetainsBulkHaltUntilReadableDataZeroIsObserved) {
    for (unsigned index : {2U, 3U}) {
        auto port = Port(index);
        ASSERT_EQ(nx_usb_endpoint_stall(&port, true), NX_SUCCESS);
        uint32_t offset = (endpoints[index].in ? 0x900U : 0xB00U) + 32U;
        model.hardware.registers[offset / 4U] |= 1U << 16U;
        model.hardware.hold_bulk_pid = true;
        nx_native_usb_reset(&model);
        nx_usb_controller_irq(&model.controller);
        EXPECT_EQ(model.hardware.registers[offset / 4U] & (1U << 21U), 0U);
        EXPECT_NE(model.hardware.registers[offset / 4U] & (1U << 16U), 0U);
        EXPECT_TRUE(endpoints[index].halted);
        EXPECT_TRUE(model.controller.reset_pending);
        EXPECT_EQ(model.hardware.consumers, 1U);
        nx_result_t prepared =
            endpoints[index].in
                ? nx_usb_in_prepare(&request, bytes, 1, NX_USB_DATA, 0, 200)
                : nx_usb_out_prepare(&request, bytes, 1, NX_USB_DATA, 0, 200);
        ASSERT_EQ(prepared, NX_SUCCESS);
        EXPECT_EQ(nx_usb_endpoint_submit(&port, &request), NX_ERROR_STATE);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
        model.hardware.hold_bulk_pid = false;
        ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
        EXPECT_EQ(model.hardware.registers[offset / 4U] & (1U << 16U), 0U);
        EXPECT_FALSE(endpoints[index].halted);
        EXPECT_FALSE(model.controller.reset_pending);
    }
    EXPECT_FALSE(model.hardware.reserved_ep0_pid_command);
}

TEST_F(UsbDevice, UnprovedResetKeepsObservedOutNakAfterActualLoanDrain) {
    const uint8_t payload[1] = {42};
    auto in0 = Port(0);
    auto out = Port(3);
    ASSERT_EQ(nx_usb_endpoint_stall(&in0, true), NX_SUCCESS);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 1, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&out, &request), NX_SUCCESS);
    model.hardware.hold_disable = true;
    ASSERT_EQ(nx_usb_endpoint_cancel(&out, &request), NX_SUCCESS);
    ASSERT_NE(model.hardware.registers[0x804U / 4U] & 8U, 0U);
    EXPECT_EQ(endpoints[3].request, &request);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    model.hardware.hold_stall_clear_mask = 1U;
    nx_native_usb_reset(&model);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(endpoints[3].request, &request);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    model.hardware.hold_disable = false;
    ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
    EXPECT_EQ(endpoints[3].request, nullptr);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_NE(model.hardware.registers[0x900U / 4U] & (1U << 21U), 0U);
    EXPECT_NE(model.hardware.registers[0x804U / 4U] & 8U, 0U);
    EXPECT_TRUE(model.controller.reset_pending);
    EXPECT_EQ(model.hardware.consumers, 1U);
    nx_usb_device_state_t snapshot = {};
    ASSERT_EQ(nx_usb_device_state(&device, &snapshot), NX_SUCCESS);
    EXPECT_TRUE(snapshot.started);
    EXPECT_FALSE(snapshot.control_pending);
    ASSERT_EQ(nx_usb_out_prepare(&request, bytes, 1, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&out, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    model.hardware.hold_stall_clear_mask = 0;
    ASSERT_EQ(nx_usb_device_service(&device), NX_SUCCESS);
    EXPECT_FALSE(model.controller.reset_pending);
    EXPECT_EQ(model.hardware.registers[0x804U / 4U] & 8U, 0U);
    ASSERT_EQ(nx_usb_endpoint_submit(&out, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_receive(&model, 1, payload, 1, false), NX_SUCCESS);
    ASSERT_EQ(nx_native_usb_complete(&model, 1, false), NX_SUCCESS);
    nx_usb_controller_irq(&model.controller);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}
