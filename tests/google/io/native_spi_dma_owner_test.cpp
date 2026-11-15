/**
 * \file            native_spi_dma_owner_test.cpp
 * \brief           Production Native segmented backend through real BusOwner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/spi_dma_owner.h"
#include "nexus/io/native/model.h"
#include "nexus/io/native/spi_dma_model.h"
#include "spi_dma_provider.h"
#include "spi_dma_sequence_owner.h"
#include <gtest/gtest.h>

namespace {
void (*handoff)(void*);
void* handoff_context;
void (*after_active)(void*);
void* after_active_context;
class NativeSPIDMAOwner : public testing::Test {
  protected:
    nx_native_spi_dma_state_t hardware[2]{};
    nx_spi_dma_sequence_state_t sequence[2]{};
    nx_native_spi_dma_endpoint_state_t device{&hardware[0], false, 0U, 0U};
    nx_native_spi_dma_endpoint_state_t other_device{&hardware[1], false, 0U,
                                                    0U};
    nx_spi_dma_sequence_endpoint_t child{&sequence[0], &device};
    nx_spi_dma_sequence_endpoint_t other_child{&sequence[1], &other_device};
    const nx_spi_dma_transaction_port_t port{&nx_spi_dma_sequence_port_ops,
                                             &sequence[0]};
    const nx_spi_dma_transaction_port_t other_port{
        &nx_spi_dma_sequence_port_ops, &sequence[1]};
    const nx_spi_dma_transaction_endpoint_t endpoint{
        &nx_spi_dma_sequence_endpoint_ops, &child};
    const nx_spi_dma_transaction_endpoint_t other_endpoint{
        &nx_spi_dma_sequence_endpoint_ops, &other_child};
    nx_spi_dma_sequence_endpoint_t forged_child{&sequence[0], &other_device};
    const nx_spi_dma_transaction_endpoint_t forged_endpoint{
        &nx_spi_dma_sequence_endpoint_ops, &forged_child};
    const nx_spi_dma_owner_binding_t binding{
        &port, nx_native_spi_dma_owner_contains,
        nx_native_spi_dma_owner_attach_wake};
    nx_spi_dma_sequence_state_t reacquired{};
    nx_native_spi_dma_endpoint_state_t successor_device{&hardware[0], false, 0U,
                                                        0U};
    nx_spi_dma_sequence_endpoint_t successor_child{&reacquired,
                                                   &successor_device};
    const nx_spi_dma_transaction_port_t successor_port{
        &nx_spi_dma_sequence_port_ops, &reacquired};
    const nx_spi_dma_transaction_endpoint_t successor_endpoint{
        &nx_spi_dma_sequence_endpoint_ops, &successor_child};
    uint16_t successor_tx{0xBEEFU};
    uint16_t successor_rx{};
    const nx_spi_dma_segment_t successor_segment{&successor_tx, &successor_rx,
                                                 1U, 0U};
    nx_spi_dma_transaction_request_t successor_request{};
    nx_spi_dma_owner_executor_t executor{};
    nx_bus_owner_t owner{};
    nx_owner_slot_t slots[2]{};
    uint16_t tx[4]{0x1234U, 0x5678U, 0x9ABCU, 0xDEF0U};
    uint16_t rx[4]{};
    uint16_t dummy{0xA5A5U};
    uint16_t discard{};
    uint16_t other_tx{0xFACEU};
    uint16_t other_rx{};
    nx_spi_dma_segment_t segments[2]{{tx, rx, 1U, 0U},
                                     {tx + 1U, rx + 1U, 1U, 0U}};
    const nx_spi_dma_segment_t other_segment{&other_tx, &other_rx, 1U, 0U};
    nx_spi_dma_owner_operation_t operation{&endpoint, segments, 2U, 16U};
    nx_request_t outer{};
    nx_spi_dma_transaction_request_t other_request{};
    nx_owner_ticket_t ticket{};
    bool clocked_before_cs = false;
    bool partial_word = false;
    unsigned hints = 0U;
    const nx_irq_wake_t wake{&hints,
                             [](void* p) {
                                 ++*static_cast<unsigned*>(p);
                                 return NX_ERROR_IO;
                             },
                             false};
    unsigned publication_calls = 0U;
    uint32_t publication_maximum = 0U;
    bool nested_publication = false;
    nx_result_t publication_initialize = NX_ERROR_STATE;
    nx_result_t publication_bind = NX_ERROR_STATE;
    nx_result_t publication_stop = NX_ERROR_STATE;
    const nx_irq_wake_t publishing{
        this,
        [](void* context) {
            auto* self = static_cast<NativeSPIDMAOwner*>(context);
            if (self->hardware[0].wake_publishers > self->publication_maximum) {
                self->publication_maximum = self->hardware[0].wake_publishers;
            }
            (void)nx_bus_owner_service(&self->owner);
            if (self->nested_publication &&
                nx_request_state(&self->outer) == NX_REQUEST_ACTIVE) {
                self->nested_publication = false;
                nx_native_spi_dma_model_step(&self->port);
                nx_native_spi_dma_model_step(&self->port);
            }
            if (nx_request_state(&self->outer) == NX_REQUEST_SETTLED) {
                ++self->publication_calls;
                self->publication_initialize =
                    nx_native_spi_dma_initialize(&self->hardware[0]);
                self->publication_bind = nx_spi_dma_owner_attach_wake(
                    &self->binding, &self->wake, 0U);
                self->publication_stop =
                    nx_spi_dma_transaction_stop(&self->port);
            }
            return NX_ERROR_IO;
        },
        false};
    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        handoff = nullptr;
        handoff_context = nullptr;
        after_active = nullptr;
        after_active_context = nullptr;
        nx_request_initialize(&successor_request.base);
        ASSERT_EQ(nx_spi_dma_transaction_prepare(
                      &successor_request, &successor_segment, 1U, 16U, 1000U),
                  NX_SUCCESS);
        for (unsigned i = 0U; i < 2U; ++i) {
            ASSERT_EQ(nx_native_spi_dma_initialize(&hardware[i]), NX_SUCCESS);
            ASSERT_EQ(nx_spi_dma_sequence_initialize(
                          &sequence[i], &nx_native_spi_dma_sequence_backend,
                          &hardware[i]),
                      NX_SUCCESS);
        }
        ASSERT_EQ(nx_native_spi_dma_model_automatic(&port, false), NX_SUCCESS);
        ASSERT_EQ(nx_native_spi_dma_model_automatic(&other_port, false),
                  NX_SUCCESS);
        const auto execution =
            nx_spi_dma_owner_executor_port(&executor, &binding);
        const nx_owner_guard_port_t guard{
            nullptr,
            [](void*) -> uintptr_t { return nx_arch_irq_save().value; },
            [](void*, uintptr_t saved) {
                nx_arch_irq_restore({static_cast<uint32_t>(saved)});
            }};
        const nx_clock_t clock{[](void*) { return nx_time_now_us(); }, nullptr};
        ASSERT_EQ(nx_bus_owner_init(&owner, slots, 2U, guard, execution, clock,
                                    nullptr),
                  NX_SUCCESS);
        nx_request_initialize(&outer);
        ASSERT_EQ(nx_request_prepare(&outer, 1000U), NX_SUCCESS);
        nx_request_initialize(&other_request.base);
        ASSERT_EQ(nx_spi_dma_transaction_prepare(&other_request, &other_segment,
                                                 1U, 16U, 1000U),
                  NX_SUCCESS);
    }
    void TearDown() override {
        handoff = nullptr;
        handoff_context = nullptr;
        after_active = nullptr;
        after_active_context = nullptr;
        if (reacquired.backend != nullptr) {
            EXPECT_EQ(nx_native_spi_dma_model_hold(&successor_port, false),
                      NX_SUCCESS);
            if (reacquired.active != nullptr) {
                EXPECT_EQ(nx_spi_dma_transaction_cancel(&successor_port,
                                                        &successor_request),
                          NX_SUCCESS);
                nx_native_spi_dma_model_step(&successor_port);
                nx_spi_dma_transaction_service(&successor_port);
            }
            EXPECT_EQ(nx_spi_dma_transaction_stop(&successor_port), NX_SUCCESS);
        }
        nx_bus_owner_stop(&owner);
        if (!sequence[0].stopped) {
            EXPECT_EQ(nx_native_spi_dma_model_hold(&port, false), NX_SUCCESS);
        }
        if (!sequence[1].stopped) {
            EXPECT_EQ(nx_native_spi_dma_model_hold(&other_port, false),
                      NX_SUCCESS);
        }
        if (sequence[1].active != nullptr) {
            (void)nx_spi_dma_transaction_cancel(&other_port, &other_request);
        }
        for (unsigned i = 0U; i < 80U && (!nx_bus_owner_idle(&owner) ||
                                          sequence[1].active != nullptr);
             ++i) {
            nx_native_spi_dma_model_step(&port);
            nx_native_spi_dma_model_step(&other_port);
            (void)nx_bus_owner_service(&owner);
            nx_spi_dma_transaction_service(&other_port);
        }
        EXPECT_TRUE(nx_bus_owner_idle(&owner));
        EXPECT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
        EXPECT_EQ(nx_spi_dma_transaction_stop(&other_port), NX_SUCCESS);
        EXPECT_EQ(nx_native_clock_configure(false, 0U), NX_SUCCESS);
    }
    static void SourceAtAdmission(void* context) {
        auto* self = static_cast<NativeSPIDMAOwner*>(context);
        if (!nx_arch_irq_is_masked()) {
            const bool selected = self->device.cs;
            const size_t before = self->hardware[0].shifted;
            nx_native_spi_dma_model_step(&self->port);
            self->clocked_before_cs =
                !selected && self->hardware[0].shifted != before;
            self->partial_word =
                self->rx[0] != 0U && self->rx[0] != self->tx[0];
        }
    }
    static void Reacquire(void* context) {
        auto* self = static_cast<NativeSPIDMAOwner*>(context);
        ASSERT_EQ(nx_spi_dma_transaction_stop(&self->port), NX_SUCCESS);
        ASSERT_EQ(nx_native_spi_dma_initialize(&self->hardware[0]), NX_SUCCESS);
        ASSERT_EQ(nx_spi_dma_sequence_initialize(
                      &self->reacquired, &nx_native_spi_dma_sequence_backend,
                      &self->hardware[0]),
                  NX_SUCCESS);
        ASSERT_EQ(
            nx_native_spi_dma_model_automatic(&self->successor_port, false),
            NX_SUCCESS);
        ASSERT_EQ(nx_spi_dma_transaction_submit(&self->successor_endpoint,
                                                &self->successor_request),
                  NX_SUCCESS);
    }
    void Start() {
        ASSERT_EQ(nx_spi_dma_owner_submit(&owner, &outer, &operation, nullptr,
                                          &ticket),
                  NX_SUCCESS);
        ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
        ASSERT_EQ(nx_request_state(&outer), NX_REQUEST_ACTIVE);
    }
    void Phase(size_t words = 1U) {
        for (size_t i = 0U; i <= words; ++i) {
            nx_native_spi_dma_model_step(&port);
        }
        (void)nx_bus_owner_service(&owner);
    }
    void Finish() {
        for (unsigned i = 0U;
             i < 80U && nx_request_state(&outer) != NX_REQUEST_SETTLED; ++i) {
            nx_native_spi_dma_model_step(&port);
            (void)nx_bus_owner_service(&owner);
        }
        ASSERT_EQ(nx_request_state(&outer), NX_REQUEST_SETTLED);
        EXPECT_FALSE(executor.pending);
        EXPECT_EQ(executor.inner.segments, nullptr);
    }
};
TEST_F(NativeSPIDMAOwner, AdmissionCannotClockWireBeforeCsOrUsePartialWord) {
    after_active = SourceAtAdmission;
    after_active_context = this;
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_EQ(after_active, nullptr);
    EXPECT_FALSE(clocked_before_cs);
    EXPECT_FALSE(partial_word);
    ASSERT_NO_FATAL_FAILURE(Finish());
    EXPECT_EQ(outer.result, NX_SUCCESS);
    EXPECT_EQ(outer.transferred, 2U);
    EXPECT_EQ(rx[0], tx[0]);
    EXPECT_EQ(rx[1], tx[1]);
}
TEST_F(NativeSPIDMAOwner, OldSourceLookupCannotAdvanceReacquiredOwner) {
    handoff = Reacquire;
    handoff_context = this;
    nx_native_spi_dma_model_step(&port);
    EXPECT_EQ(handoff, nullptr);
    EXPECT_TRUE(sequence[0].stopped);
    EXPECT_TRUE(hardware[0].opened);
    EXPECT_TRUE(successor_device.cs);
    EXPECT_EQ(nx_request_state(&successor_request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(successor_rx, 0U);
    EXPECT_EQ(hardware[0].shifted, 0U);
}
TEST_F(NativeSPIDMAOwner, PhysicalForeignEndpointCannotClaimLogicalController) {
    operation.endpoint = &forged_endpoint;
    EXPECT_FALSE(nx_spi_dma_owner_contains(&binding, &forged_endpoint));
    EXPECT_EQ(
        nx_spi_dma_owner_submit(&owner, &outer, &operation, nullptr, &ticket),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_READY);
    EXPECT_EQ(sequence[0].active, nullptr);
    EXPECT_FALSE(device.cs);
    EXPECT_FALSE(other_device.cs);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}
TEST_F(NativeSPIDMAOwner, RealBackendMovesHalfwordsWithOneCsAndServiceGap) {
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_TRUE(device.cs);
    EXPECT_EQ(device.assertions, 1U);
    EXPECT_EQ(rx[0], 0U);
    ASSERT_NO_FATAL_FAILURE(Phase());
    EXPECT_EQ(rx[0], tx[0]);
    EXPECT_EQ(rx[1], 0U);
    EXPECT_TRUE(device.cs);
    EXPECT_EQ(device.releases, 0U);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_ACTIVE);
    ASSERT_NO_FATAL_FAILURE(Finish());
    EXPECT_EQ(rx[1], tx[1]);
    EXPECT_EQ(outer.result, NX_SUCCESS);
    EXPECT_EQ(outer.transferred, 2U);
    EXPECT_EQ(device.assertions, 1U);
    EXPECT_EQ(device.releases, 1U);
    EXPECT_FALSE(device.cs);
    EXPECT_EQ(hardware[0].active, nullptr);
}
TEST_F(NativeSPIDMAOwner, ByteWidthAndFixedCellsRetainExplicitWordCounts) {
    segments[0] = {&dummy, rx, 2U, NX_SPI_MEMORY_TX_FIXED};
    segments[1] = {tx + 2U, &discard, 2U, NX_SPI_MEMORY_RX_FIXED};
    operation.word_bits = 8U;
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Finish());
    const auto* received = reinterpret_cast<const uint8_t*>(rx);
    const auto* source = reinterpret_cast<const uint8_t*>(tx + 2U);
    EXPECT_EQ(received[0], 0xA5U);
    EXPECT_EQ(received[1], 0xA5U);
    EXPECT_EQ(discard, source[1]);
    EXPECT_EQ(outer.transferred, 4U);
    EXPECT_EQ(device.assertions, 1U);
    EXPECT_EQ(device.releases, 1U);
}
TEST_F(NativeSPIDMAOwner,
       IndependentActualControllersCannotConsumeEachOthersCs) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(nx_spi_dma_transaction_submit(&other_endpoint, &other_request),
              NX_SUCCESS);
    EXPECT_TRUE(other_device.cs);
    ASSERT_NO_FATAL_FAILURE(Finish());
    EXPECT_EQ(other_rx, 0U);
    EXPECT_TRUE(other_device.cs);
    EXPECT_EQ(nx_request_state(&other_request.base), NX_REQUEST_ACTIVE);
    nx_native_spi_dma_model_step(&other_port);
    nx_native_spi_dma_model_step(&other_port);
    nx_spi_dma_transaction_service(&other_port);
    EXPECT_EQ(nx_request_state(&other_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(other_rx, other_tx);
    EXPECT_FALSE(other_device.cs);
}
TEST_F(NativeSPIDMAOwner, SecondPhaseCancelQuarantineRetainsBothBorrowLayers) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Phase());
    ASSERT_EQ(sequence[0].phase, 1U);
    ASSERT_EQ(nx_native_spi_dma_model_hold(&port, true), NX_SUCCESS);
    ASSERT_EQ(nx_bus_owner_cancel(&owner, ticket), NX_SUCCESS);
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(sequence[0].active, &executor.inner);
    EXPECT_EQ(executor.inner.segments, segments);
    EXPECT_TRUE(device.cs);
    EXPECT_EQ(device.releases, 0U);
    EXPECT_EQ(nx_spi_dma_transaction_stop(&port), NX_ERROR_BUSY);
    ASSERT_EQ(nx_native_spi_dma_model_hold(&port, false), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Finish());
    EXPECT_EQ(outer.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(outer.transferred, 1U);
    EXPECT_EQ(device.releases, 1U);
}
TEST_F(NativeSPIDMAOwner, FailedHintsNeverReplacePeriodicAuthoritativeService) {
    ASSERT_EQ(nx_spi_dma_owner_attach_wake(&binding, &wake, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_native_spi_dma_model_automatic(&port, true), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Start());
    for (unsigned i = 0U;
         i < 16U && nx_request_state(&outer) != NX_REQUEST_SETTLED; ++i) {
        (void)nx_bus_owner_service(&owner);
    }
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_SETTLED);
    EXPECT_EQ(outer.result, NX_SUCCESS);
    EXPECT_EQ(outer.transferred, 2U);
    EXPECT_GE(hints, 2U);
    EXPECT_EQ(rx[0], tx[0]);
    EXPECT_EQ(rx[1], tx[1]);
    EXPECT_EQ(hardware[0].wake_publishers, 0U);
    EXPECT_EQ(nx_spi_dma_owner_attach_wake(&binding, nullptr, 0U), NX_SUCCESS);
}
TEST_F(NativeSPIDMAOwner, SettledRequestCannotReclaimActiveWakePublication) {
    ASSERT_EQ(nx_spi_dma_owner_attach_wake(&binding, &publishing, 0U),
              NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Finish());
    EXPECT_EQ(publication_calls, 1U);
    EXPECT_EQ(publication_initialize, NX_ERROR_BUSY);
    EXPECT_EQ(publication_bind, NX_ERROR_BUSY);
    EXPECT_EQ(publication_stop, NX_ERROR_BUSY);
    EXPECT_FALSE(sequence[0].stopped);
    EXPECT_EQ(hardware[0].wake, &publishing);
    EXPECT_EQ(hardware[0].wake_publishers, 0U);
    EXPECT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
}
TEST_F(NativeSPIDMAOwner, NestedWakePublicationsAllJoinBeforeStop) {
    nested_publication = true;
    ASSERT_EQ(nx_spi_dma_owner_attach_wake(&binding, &publishing, 0U),
              NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Finish());
    EXPECT_EQ(publication_calls, 2U);
    EXPECT_EQ(publication_maximum, 2U);
    EXPECT_EQ(publication_initialize, NX_ERROR_BUSY);
    EXPECT_EQ(publication_bind, NX_ERROR_BUSY);
    EXPECT_EQ(publication_stop, NX_ERROR_BUSY);
    EXPECT_FALSE(sequence[0].stopped);
    EXPECT_EQ(hardware[0].wake, &publishing);
    EXPECT_EQ(hardware[0].wake_publishers, 0U);
    EXPECT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
}
TEST_F(NativeSPIDMAOwner, SaturatedWakePublicationRetainsJoinResponsibility) {
    ASSERT_EQ(nx_spi_dma_owner_attach_wake(&binding, &wake, 0U), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Start());
    /* Synthetic existing publishers cannot wrap or be cleared by stop. */
    hardware[0].wake_publishers = UINT32_MAX;
    Finish();
    EXPECT_EQ(outer.result, NX_SUCCESS);
    EXPECT_EQ(outer.transferred, 2U);
    EXPECT_EQ(hints, 0U);
    EXPECT_EQ(hardware[0].wake_publishers, UINT32_MAX);
    EXPECT_EQ(nx_native_spi_dma_initialize(&hardware[0]), NX_ERROR_BUSY);
    EXPECT_EQ(nx_spi_dma_owner_attach_wake(&binding, nullptr, 0U),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_spi_dma_transaction_stop(&port), NX_ERROR_BUSY);
    EXPECT_FALSE(sequence[0].stopped);
    EXPECT_EQ(hardware[0].wake, &wake);
    EXPECT_EQ(hardware[0].wake_publishers, UINT32_MAX);
    /* Join the synthetic publishers only; production never clears a count. */
    hardware[0].wake_publishers = 0U;
    EXPECT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
}
TEST_F(NativeSPIDMAOwner, DeadlineDrainsRatherThanReplayingUncompletedSegment) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(nx_native_clock_advance(1000U), NX_SUCCESS);
    (void)nx_bus_owner_service(&owner);
    EXPECT_NE(nx_request_state(&outer), NX_REQUEST_SETTLED);
    EXPECT_TRUE(device.cs);
    ASSERT_NO_FATAL_FAILURE(Finish());
    EXPECT_EQ(outer.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(outer.transferred, 0U);
    EXPECT_EQ(device.assertions, 1U);
    EXPECT_EQ(device.releases, 1U);
}
TEST_F(NativeSPIDMAOwner, FaultAfterPrefixNeverStartsAnotherChainOrReplays) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Phase());
    ASSERT_EQ(nx_native_spi_dma_model_fault(&port, 0U), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Finish());
    EXPECT_EQ(outer.result, NX_ERROR_IO);
    EXPECT_EQ(outer.transferred, 1U);
    EXPECT_EQ(rx[0], tx[0]);
    EXPECT_EQ(rx[1], 0U);
    EXPECT_EQ(device.assertions, 1U);
    EXPECT_EQ(device.releases, 1U);
}
TEST_F(NativeSPIDMAOwner, SuccessfulOldStopCannotWithdrawReacquiredBackend) {
    ASSERT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
    ASSERT_EQ(nx_native_spi_dma_initialize(&hardware[0]), NX_SUCCESS);
    nx_spi_dma_sequence_state_t successor{};
    ASSERT_EQ(
        nx_spi_dma_sequence_initialize(
            &successor, &nx_native_spi_dma_sequence_backend, &hardware[0]),
        NX_SUCCESS);
    const nx_spi_dma_transaction_port_t next_port{&nx_spi_dma_sequence_port_ops,
                                                  &successor};
    EXPECT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
    EXPECT_TRUE(hardware[0].opened);
    EXPECT_EQ(nx_spi_dma_transaction_stop(&next_port), NX_SUCCESS);
}
} /* namespace */

/** \brief Enter the exact stale lookup window without changing production IRQs.
 */
extern "C" void nx_native_spi_dma_owner_before_step(void) {
    if (handoff != nullptr) {
        auto callback = handoff;
        handoff = nullptr;
        callback(handoff_context);
    }
}

/** \brief Respect CPU exclusion while observing real word/CS effects. */
extern "C" void nx_native_spi_dma_owner_after_active(void) {
    if (after_active != nullptr) {
        auto callback = after_active;
        after_active = nullptr;
        callback(after_active_context);
    }
}
