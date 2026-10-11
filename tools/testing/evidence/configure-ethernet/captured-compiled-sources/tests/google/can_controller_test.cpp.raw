/**
 * \file            can_controller_test.cpp
 * \brief           Classic CAN mailbox, shared-domain and loss contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "can.h"
#include "nexus/io/native/model.h"
}
#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

class ClassicCan : public testing::Test {
  protected:
    nx_native_can_t model = {};
    nx_can_filter_t filter = {0x123U, 0x7FFU,          0U,    0U,
                              0U,     NX_CAN_STANDARD, false, true};
    nx_can_filter_domain_t domain = {};
    nx_can_event_t events[2] = {};
    const nx_can_port_t port = {&nx_classic_can_ops, &model.controller};
    nx_can_tx_request_t requests[4] = {};
    nx_can_frame_t frame = {
        0x123U, NX_CAN_STANDARD, NX_CAN_DATA, 8U, {1, 2, 3, 4, 5, 6, 7, 8}};

    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        domain.primary = reinterpret_cast<uintptr_t>(&model.hardware);
        domain.clock_enable = &model.hardware.clock_enable;
        domain.primary_clock_mask = 1U << 25U;
        domain.secondary_clock_mask = 1U << 26U;
        domain.filters = &filter;
        domain.filter_count = 1U;
        domain.split = 14U;
        auto& state = model.controller;
        state.domain = &domain;
        state.events = events;
        state.capacity = 2U;
        state.timing = {500000U, 6U, 11U, 2U, 2U};
        state.clock_hz = 42000000U;
        state.drain_timeout_us = 20U;
        state.poll_limit = 4U;
        state.irq_policy = {NX_IRQ_KERNEL_BASEPRI, 82U, 4U, 5U};
        for (unsigned i = 0; i < 4U; ++i) {
            state.sources[i] = {static_cast<int16_t>(19U + i), 5U, 3U};
            nx_request_initialize(&requests[i].base);
            ASSERT_EQ(nx_can_tx_prepare(&requests[i], &frame, 200U),
                      NX_SUCCESS);
        }
        ASSERT_EQ(nx_native_can_initialize(&model), NX_SUCCESS);
    }
    void TearDown() override {
        model.hardware.hold_abort = false;
        model.hardware.hold_fifo_release = false;
        model.hardware.hold_irq_enable = false;
        model.hardware.hold_irq_disable = false;
        model.hardware.hold_interrupt_clear = false;
        model.hardware.hold_completion_clear = false;
        EXPECT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    }
    void Receive(unsigned fifo = 0, uint8_t raw_dlc = 8U) {
        ASSERT_EQ(nx_native_can_receive(&model, fifo, &frame, raw_dlc, 7U),
                  NX_SUCCESS);
        nx_can_controller_irq(&model.controller);
    }
};

TEST_F(ClassicCan, RejectsNonClassicFramesWithoutMutatingPreparedStorage) {
    frame.dlc = 9U;
    EXPECT_EQ(nx_can_tx_prepare(&requests[0], &frame, 200U), NX_ERROR_INVALID);
    EXPECT_EQ(requests[0].frame.dlc, 8U);
    frame.dlc = 8U;
    frame.id = 0x800U;
    EXPECT_EQ(nx_can_tx_prepare(&requests[0], &frame, 200U), NX_ERROR_INVALID);
    frame.format = NX_CAN_EXTENDED;
    frame.id = 0x20000000U;
    EXPECT_EQ(nx_can_tx_prepare(&requests[0], &frame, 200U), NX_ERROR_INVALID);
}
TEST_F(ClassicCan, ThreeMailboxesAndFourthRejectionHaveExactBorrowBoundaries) {
    for (unsigned i = 0; i < 3U; ++i) {
        ASSERT_EQ(nx_can_port_submit(&port, &requests[i]), NX_SUCCESS);
    }
    EXPECT_EQ(nx_can_port_submit(&port, &requests[3]), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&requests[3].base), NX_REQUEST_READY);
    EXPECT_EQ(model.controller.snapshot.pending, 3U);
}
TEST_F(ClassicCan, MailboxEmptyAloneNeverSettlesTheWireRequest) {
    ASSERT_EQ(nx_can_port_submit(&port, &requests[0]), NX_SUCCESS);
    model.hardware.registers[2] |= 1U << 26U;
    ASSERT_EQ(nx_can_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_ACTIVE);
}
TEST_F(ClassicCan, TxokAndCompletionDetachBeforeSettlement) {
    ASSERT_EQ(nx_can_port_submit(&port, &requests[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_can_complete(&model, 0U, NX_SUCCESS), NX_SUCCESS);
    nx_can_controller_irq(&model.controller);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_SETTLED);
    EXPECT_EQ(model.controller.slots[0].request, nullptr);
    nx_result_t result = NX_ERROR_IO;
    size_t transferred = 0;
    ASSERT_EQ(nx_request_result(&requests[0].base, &result, &transferred),
              NX_SUCCESS);
    EXPECT_EQ(result, NX_SUCCESS);
    EXPECT_EQ(transferred, 8U);
}
TEST_F(ClassicCan, RemoteFrameUsesRequestedDlcAndZeroTransferredBytes) {
    frame.kind = NX_CAN_REMOTE;
    ASSERT_EQ(nx_can_tx_prepare(&requests[0], &frame, 200U), NX_SUCCESS);
    ASSERT_EQ(nx_can_port_submit(&port, &requests[0]), NX_SUCCESS);
    EXPECT_EQ(model.hardware.registers[0x188U / 4U], 0U);
    EXPECT_NE(model.hardware.registers[0x180U / 4U] & 2U, 0U);
    ASSERT_EQ(nx_native_can_complete(&model, 0U, NX_SUCCESS), NX_SUCCESS);
    nx_can_controller_irq(&model.controller);
    EXPECT_EQ(requests[0].base.transferred, 0U);
}
TEST_F(ClassicCan, CancellationWaitsForActualAbortAcknowledgement) {
    model.hardware.hold_abort = true;
    ASSERT_EQ(nx_can_port_submit(&port, &requests[0]), NX_SUCCESS);
    ASSERT_EQ(nx_can_port_cancel(&port, &requests[0]), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_DRAINING);
    ASSERT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
    ASSERT_EQ(nx_can_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(model.controller.slots[0].request, &requests[0]);
    EXPECT_EQ(nx_can_port_stop(&port), NX_ERROR_BUSY);
    ASSERT_EQ(nx_native_can_complete(&model, 0U, NX_ERROR_CANCELLED),
              NX_SUCCESS);
    ASSERT_EQ(nx_can_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_SETTLED);
    EXPECT_EQ(requests[0].base.result, NX_ERROR_CANCELLED);
}
TEST_F(ClassicCan, CompletionRacingCancellationPreservesWireSuccess) {
    ASSERT_EQ(nx_can_port_submit(&port, &requests[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_can_complete(&model, 0U, NX_SUCCESS), NX_SUCCESS);
    ASSERT_EQ(nx_can_port_cancel(&port, &requests[0]), NX_SUCCESS);
    EXPECT_EQ(requests[0].base.result, NX_SUCCESS);
}
TEST_F(ClassicCan, TimeoutUsesAbsoluteDeadlineAndRetainsAbortResponsibility) {
    model.hardware.hold_abort = true;
    ASSERT_EQ(nx_can_port_submit(&port, &requests[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(100U), NX_SUCCESS);
    ASSERT_EQ(nx_can_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_DRAINING);
    ASSERT_EQ(nx_native_can_complete(&model, 0U, NX_ERROR_CANCELLED),
              NX_SUCCESS);
    ASSERT_EQ(nx_can_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(requests[0].base.result, NX_ERROR_TIMEOUT);
}
TEST_F(ClassicCan, DeadlineExpiredRejectsWithNoMailboxEffect) {
    ASSERT_EQ(nx_native_clock_advance(100U), NX_SUCCESS);
    EXPECT_EQ(nx_can_port_submit(&port, &requests[0]), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_READY);
    EXPECT_EQ(model.hardware.registers[0x180U / 4U], 0U);
}
TEST_F(ClassicCan, ReceiveDlcFifteenCopiesOnlyEightBytesWithRawMetadata) {
    Receive(1U, 15U);
    nx_can_event_t output = {};
    size_t count = 0;
    ASSERT_EQ(nx_can_port_read_events(&port, &output, 1U, &count), NX_SUCCESS);
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(output.frame.dlc, 8U);
    EXPECT_EQ(output.raw_dlc, 15U);
    EXPECT_EQ(output.fifo, 1U);
    EXPECT_EQ(output.filter, 7U);
    EXPECT_NE(output.flags & NX_CAN_DLC_NORMALIZED, 0U);
    EXPECT_EQ(output.frame.data[7], 8U);
}
TEST_F(ClassicCan, FullRingPlacesLossAfterOlderFactsAndBeforeLaterFrames) {
    Receive();
    frame.id = 0x124U;
    Receive();
    frame.id = 0x125U;
    Receive();
    nx_can_event_t output[4] = {};
    size_t count = 0;
    ASSERT_EQ(nx_can_port_read_events(&port, output, 4U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 3U);
    EXPECT_EQ(output[0].frame.id, 0x123U);
    EXPECT_EQ(output[1].frame.id, 0x124U);
    EXPECT_EQ(output[2].kind, NX_CAN_EVENT_LOSS);
    EXPECT_EQ(output[2].lost, 1U);
    EXPECT_EQ(output[2].flags, NX_CAN_LOSS_SOFTWARE);
    frame.id = 0x126U;
    Receive();
    ASSERT_EQ(nx_can_port_read_events(&port, output, 1U, &count), NX_SUCCESS);
    EXPECT_EQ(output[0].frame.id, 0x126U);
}
TEST_F(ClassicCan, HardwareOverrunReportsLowerBoundWithoutInventedExactCount) {
    model.hardware.registers[0x0CU / 4U] |= 1U << 4U;
    nx_can_controller_irq(&model.controller);
    nx_can_event_t output = {};
    size_t count = 0;
    ASSERT_EQ(nx_can_port_read_events(&port, &output, 1U, &count), NX_SUCCESS);
    EXPECT_EQ(output.kind, NX_CAN_EVENT_LOSS);
    EXPECT_EQ(output.lost, 1U);
    EXPECT_NE(output.flags & NX_CAN_LOSS_HARDWARE_LOWER_BOUND, 0U);
}
TEST_F(ClassicCan, BusOffRemainsAuthoritativeWhenTheRingIsFull) {
    Receive();
    Receive();
    model.hardware.hold_abort = true;
    ASSERT_EQ(nx_can_port_submit(&port, &requests[0]), NX_SUCCESS);
    nx_native_can_error(&model, true, 255U, 128U, 3U);
    nx_can_controller_irq(&model.controller);
    nx_can_state_t state = {};
    ASSERT_EQ(nx_can_port_state(&port, &state), NX_SUCCESS);
    EXPECT_TRUE(state.bus_off);
    EXPECT_EQ(state.transmit_errors, 255U);
    EXPECT_EQ(nx_can_port_submit(&port, &requests[1]), NX_ERROR_STATE);
    EXPECT_EQ(nx_can_port_recover(&port), NX_ERROR_BUSY);
    ASSERT_EQ(nx_native_can_complete(&model, 0U, NX_ERROR_IO), NX_SUCCESS);
    ASSERT_EQ(nx_can_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_can_port_recover(&port), NX_SUCCESS);
}
TEST_F(ClassicCan, FourthVectorPriorityCannotBypassKernelWakeCeiling) {
    nx_irq_wake_t wake = {nullptr, [](void*) { return NX_SUCCESS; }, true};
    model.controller.sources[3].priority = 4U;
    EXPECT_EQ(nx_can_port_attach_wake(&port, &wake, 5U), NX_ERROR_PERMISSION);
    EXPECT_EQ(model.controller.wake, nullptr);
    model.controller.sources[3].priority = 5U;
    EXPECT_EQ(nx_can_port_attach_wake(&port, &wake, 5U), NX_SUCCESS);
    EXPECT_EQ(nx_can_port_attach_wake(&port, nullptr, 0U), NX_SUCCESS);
}
TEST_F(ClassicCan, CompletionClearFailureCannotReleaseCallerStorage) {
    model.hardware.hold_completion_clear = true;
    ASSERT_EQ(nx_can_port_submit(&port, &requests[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_can_complete(&model, 0U, NX_SUCCESS), NX_SUCCESS);
    ASSERT_EQ(nx_can_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(model.controller.slots[0].request, &requests[0]);
    model.hardware.hold_completion_clear = false;
    ASSERT_EQ(nx_can_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_SETTLED);
}
TEST_F(ClassicCan, StuckIrqDisableRetainsRingAndWakeUntilStopReadback) {
    nx_irq_wake_t wake = {nullptr, [](void*) { return NX_SUCCESS; }, false};
    ASSERT_EQ(nx_can_port_attach_wake(&port, &wake, 0U), NX_SUCCESS);
    model.hardware.hold_irq_disable = true;
    EXPECT_EQ(nx_can_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(model.controller.events, events);
    EXPECT_EQ(model.controller.wake, &wake);
    EXPECT_EQ(domain.active_mask, 1U);
    model.hardware.hold_irq_disable = false;
    EXPECT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(model.controller.wake, nullptr);
    EXPECT_EQ(domain.active_mask, 0U);
}
TEST_F(ClassicCan, IncomingMaskRejectsTaskControlsWithoutAdmission) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    EXPECT_EQ(nx_can_port_submit(&port, &requests[0]), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_can_port_service(&port), NX_ERROR_CONTEXT);
    nx_arch_irq_restore(saved);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_READY);
}
TEST_F(ClassicCan, FiltersUseExactIdentifierIdeRtrAndImmutableBankSplit) {
    EXPECT_EQ(model.hardware.registers[0x200U / 4U], 14U << 8U);
    EXPECT_EQ(model.hardware.registers[0x240U / 4U], 0x123U << 21U);
    EXPECT_EQ(model.hardware.registers[0x244U / 4U], (0x7FFU << 21U) | 6U);
    EXPECT_EQ(model.hardware.registers[0x21CU / 4U], 1U);
    EXPECT_EQ(nx_can_filter_domain_initialize(&domain), NX_ERROR_BUSY);
}
TEST_F(ClassicCan, WrongFilterOwnerAndNonintegralTimingRejectBeforeEffects) {
    ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    domain.configured = false;
    filter.bank = 14U;
    const uint32_t prior = model.hardware.registers[0x21CU / 4U];
    EXPECT_EQ(nx_can_filter_domain_initialize(&domain), NX_ERROR_INVALID);
    EXPECT_EQ(model.hardware.registers[0x21CU / 4U], prior);
    filter.bank = 0U;
    model.controller.timing.prescaler = 7U;
    EXPECT_EQ(nx_native_can_initialize(&model), NX_ERROR_INVALID);
    EXPECT_EQ(domain.active_mask, 0U);
}

TEST_F(ClassicCan, PrimaryClockIsActuallyAcquiredForTheFilterDomain) {
    EXPECT_NE(model.hardware.clock_enable & (1U << 25U), 0U);
}
TEST_F(ClassicCan, SecondControllerKeepsPrimaryClockAndPeerFiltersAlive) {
    nx_native_can_t peer = {};
    peer.controller = model.controller;
    peer.controller.snapshot = {};
    peer.controller.index = 1U;
    nx_can_event_t peer_events[2] = {};
    peer.controller.events = peer_events;
    for (unsigned i = 0; i < 4U; ++i) {
        peer.controller.sources[i].irq_number = static_cast<int16_t>(63U + i);
    }
    ASSERT_EQ(nx_native_can_initialize(&peer), NX_SUCCESS);
    EXPECT_EQ(domain.active_mask, 3U);
    EXPECT_NE(model.hardware.clock_enable & (1U << 26U), 0U);
    const uint32_t primary_filter = model.hardware.registers[0x240U / 4U];
    ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(domain.active_mask, 2U);
    EXPECT_NE(model.hardware.clock_enable & (1U << 25U), 0U);
    EXPECT_EQ(model.hardware.registers[0x240U / 4U], primary_filter);
    nx_can_port_t peer_port = {&nx_classic_can_ops, &peer.controller};
    ASSERT_EQ(nx_can_port_submit(&peer_port, &requests[0]), NX_SUCCESS);
    ASSERT_EQ(nx_native_can_complete(&peer, 0U, NX_SUCCESS), NX_SUCCESS);
    ASSERT_EQ(nx_can_port_service(&peer_port), NX_SUCCESS);
    EXPECT_EQ(requests[0].base.result, NX_SUCCESS);
    EXPECT_EQ(nx_can_port_stop(&peer_port), NX_SUCCESS);
}

TEST_F(ClassicCan, ColdUnownedTransmitCannotEscapeInitializationMode) {
    ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    model.hardware.registers[0x180U / 4U] = 1U;
    model.hardware.registers[2] &= ~(1U << 26U);
    EXPECT_EQ(nx_can_controller_initialize(&model.controller), NX_ERROR_IO);
    EXPECT_TRUE(model.controller.snapshot.closing);
    EXPECT_EQ(model.hardware.registers[0] & 1U, 1U);
    EXPECT_EQ(domain.active_mask, 1U);
    EXPECT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(model.hardware.registers[0x180U / 4U] & 1U, 0U);
}
TEST_F(ClassicCan, UnownedHardwareAbortFailureRetainsDomainAndWakeLoans) {
    ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    model.hardware.registers[0x180U / 4U] = 1U;
    model.hardware.registers[2] &= ~(1U << 26U);
    model.hardware.hold_abort = true;
    (void)nx_can_controller_initialize(&model.controller);
    EXPECT_EQ(nx_can_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_TRUE(model.controller.snapshot.started);
    EXPECT_EQ(nx_can_filter_domain_stop(&domain), NX_ERROR_BUSY);
    model.hardware.hold_abort = false;
    EXPECT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
}

TEST_F(ClassicCan, StoppedReaderNeverTouchesReleasedReceiveStorage) {
    Receive();
    ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    nx_can_event_t output = {};
    size_t count = 0U;
    EXPECT_EQ(nx_can_port_read_events(&port, &output, 1U, &count),
              NX_ERROR_STATE);
}
TEST_F(ClassicCan, LockedHardwareFifoOlderFramesPrecedeItsOverrunBoundary) {
    ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    nx_can_event_t storage[8] = {};
    model.controller.events = storage;
    model.controller.capacity = 8U;
    ASSERT_EQ(nx_can_controller_initialize(&model.controller), NX_SUCCESS);
    for (unsigned i = 0; i < 3U; ++i) {
        ASSERT_EQ(nx_native_can_receive(&model, 0U, &frame, 8U, 0U),
                  NX_SUCCESS);
    }
    EXPECT_EQ(nx_native_can_receive(&model, 0U, &frame, 8U, 0U),
              NX_ERROR_OVERFLOW);
    nx_can_controller_irq(&model.controller);
    nx_can_event_t output[4] = {};
    size_t count = 0U;
    ASSERT_EQ(nx_can_port_read_events(&port, output, 4U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 4U);
    for (unsigned i = 0; i < 3U; ++i) {
        EXPECT_EQ(output[i].kind, NX_CAN_EVENT_FRAME);
    }
    EXPECT_EQ(output[3].kind, NX_CAN_EVENT_LOSS);
    EXPECT_NE(output[3].flags & NX_CAN_LOSS_HARDWARE_LOWER_BOUND, 0U);
    ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
}

TEST_F(ClassicCan, FailedVectorEnableCannotReportSuccessfulInitialization) {
    ASSERT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    model.hardware.hold_irq_enable = true;
    EXPECT_EQ(nx_can_controller_initialize(&model.controller), NX_ERROR_IO);
    EXPECT_TRUE(model.controller.snapshot.closing);
    EXPECT_EQ(domain.active_mask, 1U);
}
TEST_F(ClassicCan, StopDrainsRetainedHardwareFifoBeforeReleasingTheRing) {
    ASSERT_EQ(nx_native_can_receive(&model, 0U, &frame, 8U, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(model.hardware.receive_count[0], 0U);
}
TEST_F(ClassicCan, StuckFifoReleaseKeepsControllerAndRingResponsibility) {
    ASSERT_EQ(nx_native_can_receive(&model, 1U, &frame, 8U, 0U), NX_SUCCESS);
    model.hardware.hold_fifo_release = true;
    EXPECT_EQ(nx_can_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_TRUE(model.controller.snapshot.started);
    EXPECT_EQ(domain.active_mask, 1U);
    model.hardware.hold_fifo_release = false;
    EXPECT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
}

TEST_F(ClassicCan, AdmissionRejectsMutableRequestAliasingTheFilterPlan) {
    const nx_can_filter_t* saved = domain.filters;
    domain.filters = reinterpret_cast<const nx_can_filter_t*>(&requests[0]);
    EXPECT_EQ(nx_can_port_submit(&port, &requests[0]), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_READY);
    EXPECT_EQ(model.hardware.registers[0x180U / 4U], 0U);
    domain.filters = saved;
}

TEST_F(ClassicCan, StopJoinsAnInFlightNativeWakePublisherBeforeRelease) {
    struct Publisher {
        std::mutex mutex;
        std::condition_variable changed;
        bool entered = false;
        bool release = false;
    } publisher;
    nx_irq_wake_t wake = {&publisher,
                          [](void* context) {
                              auto* current = static_cast<Publisher*>(context);
                              std::unique_lock<std::mutex> lock(current->mutex);
                              current->entered = true;
                              current->changed.notify_all();
                              bool released = current->changed.wait_for(
                                  lock, std::chrono::seconds(2),
                                  [&] { return current->release; });
                              return released ? NX_SUCCESS : NX_ERROR_TIMEOUT;
                          },
                          false};
    ASSERT_EQ(nx_can_port_attach_wake(&port, &wake, 0U), NX_SUCCESS);
    std::thread irq([&] { nx_can_controller_irq(&model.controller); });
    {
        std::unique_lock<std::mutex> lock(publisher.mutex);
        EXPECT_TRUE(publisher.changed.wait_for(
            lock, std::chrono::seconds(2), [&] { return publisher.entered; }));
    }
    EXPECT_EQ(nx_can_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(model.controller.wake, &wake);
    {
        std::lock_guard<std::mutex> lock(publisher.mutex);
        publisher.release = true;
        publisher.changed.notify_all();
    }
    irq.join();
    EXPECT_EQ(nx_can_port_stop(&port), NX_SUCCESS);
}
