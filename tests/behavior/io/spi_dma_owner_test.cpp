/**
 * \file            spi_dma_owner_test.cpp
 * \brief           Typed segmented SPI executor uses real bounded owner slots
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/spi_dma_owner.h"
#include "nexus/io/native/model.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using testing::_;
using testing::Invoke;
using testing::Return;

class Provider {
  public:
    MOCK_METHOD(nx_result_t, Submit, (nx_spi_dma_transaction_request_t*));
    MOCK_METHOD(nx_result_t, Cancel, (nx_spi_dma_transaction_request_t*));
    MOCK_METHOD(void, Service, ());
    MOCK_METHOD(nx_result_t, Attach, (const nx_irq_wake_t*, uint8_t));
};
struct Child {
    Provider* controller;
};
const nx_spi_dma_transaction_port_ops_t port_ops{
    [](void* p, nx_spi_dma_transaction_request_t* inner) {
        return static_cast<Provider*>(p)->Cancel(inner);
    },
    [](void* p) { static_cast<Provider*>(p)->Service(); },
    [](void*) { return NX_SUCCESS; }};
const nx_spi_dma_transaction_endpoint_ops_t endpoint_ops{
    [](void* p, nx_spi_dma_transaction_request_t* inner) {
        return static_cast<Child*>(p)->controller->Submit(inner);
    }};
bool contains(const nx_spi_dma_transaction_endpoint_t* endpoint,
              const nx_spi_dma_transaction_port_t* port) {
    return endpoint != nullptr && endpoint->ops == &endpoint_ops &&
           endpoint->context != nullptr && port != nullptr &&
           port->ops == &port_ops &&
           static_cast<const Child*>(endpoint->context)->controller ==
               port->context;
}
nx_result_t attach(const nx_spi_dma_transaction_port_t* port,
                   const nx_irq_wake_t* wake, uint8_t ceiling) {
    return static_cast<Provider*>(port->context)->Attach(wake, ceiling);
}

class SPIDMAOwner : public testing::Test {
  protected:
    testing::StrictMock<Provider> provider;
    const nx_spi_dma_transaction_port_t port{&port_ops, &provider};
    Child child{&provider};
    const nx_spi_dma_transaction_endpoint_t endpoint{&endpoint_ops, &child};
    const nx_spi_dma_owner_binding_t binding{&port, contains, attach};
    nx_spi_dma_owner_executor_t executor{};
    nx_bus_owner_t owner{};
    nx_owner_slot_t slots[2]{};
    unsigned guarded = 0U;
    uint16_t tx[2]{0x1234U, 0x5678U};
    uint16_t rx[2]{};
    nx_spi_dma_segment_t segments[2]{{tx, rx, 1U, 0U},
                                     {tx + 1U, rx + 1U, 1U, 0U}};
    nx_spi_dma_owner_operation_t operation{&endpoint, segments, 2U, 16U};
    nx_request_t outer{};
    nx_owner_ticket_t ticket{};

    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        const auto execution =
            nx_spi_dma_owner_executor_port(&executor, &binding);
        const nx_owner_guard_port_t guard{this,
                                          [](void* p) -> uintptr_t {
                                              auto* self =
                                                  static_cast<SPIDMAOwner*>(p);
                                              EXPECT_EQ(self->guarded, 0U);
                                              ++self->guarded;
                                              return 0U;
                                          },
                                          [](void* p, uintptr_t) {
                                              auto* self =
                                                  static_cast<SPIDMAOwner*>(p);
                                              EXPECT_EQ(self->guarded, 1U);
                                              --self->guarded;
                                          }};
        const nx_clock_t clock{[](void*) { return nx_time_now_us(); }, nullptr};
        ASSERT_EQ(nx_bus_owner_init(&owner, slots, 2U, guard, execution, clock,
                                    nullptr),
                  NX_SUCCESS);
        nx_request_initialize(&outer);
        ASSERT_EQ(nx_request_prepare(&outer, 1000U), NX_SUCCESS);
    }
    void TearDown() override {
        nx_bus_owner_stop(&owner);
        if (executor.pending) {
            testing::Mock::VerifyAndClearExpectations(&provider);
            EXPECT_CALL(provider, Service()).WillRepeatedly(Invoke([&] {
                if (nx_request_state(&executor.inner.base) !=
                    NX_REQUEST_SETTLED) {
                    nx_request_settle(&executor.inner.base, NX_ERROR_CANCELLED,
                                      0U);
                }
            }));
            EXPECT_CALL(provider, Cancel(_)).WillRepeatedly(Return(NX_SUCCESS));
        }
        for (unsigned i = 0U; i < 8U && !nx_bus_owner_idle(&owner); ++i) {
            (void)nx_bus_owner_service(&owner);
        }
        EXPECT_TRUE(nx_bus_owner_idle(&owner));
        EXPECT_EQ(nx_native_clock_configure(false, 0U), NX_SUCCESS);
    }
    void Enqueue(const nx_wait_port_t* completion = nullptr) {
        ASSERT_EQ(nx_spi_dma_owner_submit(&owner, &outer, &operation,
                                          completion, &ticket),
                  NX_SUCCESS);
        EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_QUEUED);
    }
    void Start() {
        EXPECT_CALL(provider, Submit(&executor.inner))
            .WillOnce(Invoke([&](nx_spi_dma_transaction_request_t* inner) {
                EXPECT_EQ(guarded, 0U);
                EXPECT_EQ(inner->segments, segments);
                EXPECT_EQ(inner->count, 2U);
                EXPECT_EQ(inner->word_bits, 16U);
                EXPECT_EQ(inner->base.deadline, 1000U);
                return nx_request_admit(&inner->base, NX_REQUEST_ACTIVE);
            }));
        ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
        EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_ACTIVE);
    }
    void Complete(nx_result_t result, size_t words) {
        EXPECT_CALL(provider, Service()).WillOnce(Invoke([&] {
            EXPECT_EQ(guarded, 0U);
            nx_request_settle(&executor.inner.base, result, words);
        }));
        ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
        EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_SETTLED);
        EXPECT_EQ(outer.result, result);
        EXPECT_EQ(outer.transferred, words);
        EXPECT_EQ(executor.inner.segments, nullptr);
        EXPECT_EQ(executor.inner.count, 0U);
        EXPECT_FALSE(executor.pending);
    }
};

TEST_F(SPIDMAOwner, RejectsForeignBusBeforeOuterAdmissionOrProviderCall) {
    testing::StrictMock<Provider> foreign;
    Child other{&foreign};
    const nx_spi_dma_transaction_endpoint_t wrong{&endpoint_ops, &other};
    operation.endpoint = &wrong;
    EXPECT_EQ(
        nx_spi_dma_owner_submit(&owner, &outer, &operation, nullptr, &ticket),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_READY);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
    EXPECT_FALSE(executor.pending);
}
TEST_F(SPIDMAOwner, RealSlotsRetainEntireChainAndReportWords) {
    ASSERT_NO_FATAL_FAILURE(Enqueue());
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_CALL(provider, Service()).WillOnce(Invoke([&] {
        EXPECT_EQ(guarded, 0U);
        EXPECT_EQ(executor.inner.segments, segments);
    }));
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_ACTIVE);
    EXPECT_EQ(executor.inner.segments, segments);
    ASSERT_NO_FATAL_FAILURE(Complete(NX_SUCCESS, 2U));
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}
TEST_F(SPIDMAOwner, DistinctChipSelectsOnSameControllerRemainValid) {
    Child second{&provider};
    const nx_spi_dma_transaction_endpoint_t face{&endpoint_ops, &second};
    operation.endpoint = &face;
    ASSERT_NO_FATAL_FAILURE(Enqueue());
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Complete(NX_SUCCESS, 2U));
}
TEST_F(SPIDMAOwner, ProviderRejectionSettlesOuterAndDropsPrivateReferences) {
    ASSERT_NO_FATAL_FAILURE(Enqueue());
    EXPECT_CALL(provider, Submit(&executor.inner))
        .WillOnce(Return(NX_ERROR_PERMISSION));
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_SETTLED);
    EXPECT_EQ(outer.result, NX_ERROR_PERMISSION);
    EXPECT_EQ(executor.inner.segments, nullptr);
    EXPECT_EQ(executor.inner.count, 0U);
    EXPECT_FALSE(executor.pending);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}
TEST_F(SPIDMAOwner,
       AlreadySettledAdmissionStillPublishesOnlyThroughOwnerService) {
    ASSERT_NO_FATAL_FAILURE(Enqueue());
    EXPECT_CALL(provider, Submit(&executor.inner))
        .WillOnce(Invoke([](nx_spi_dma_transaction_request_t* inner) {
            nx_result_t admitted =
                nx_request_admit(&inner->base, NX_REQUEST_ACTIVE);
            nx_request_settle(&inner->base, NX_SUCCESS, 2U);
            return admitted;
        }));
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_ACTIVE);
    EXPECT_CALL(provider, Service());
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_SETTLED);
    EXPECT_EQ(outer.transferred, 2U);
    EXPECT_FALSE(executor.pending);
}
TEST_F(SPIDMAOwner, QueueDeadlineExpiresWithoutAnyProviderBorrow) {
    ASSERT_EQ(nx_request_prepare(&outer, 100U), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Enqueue());
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_SETTLED);
    EXPECT_EQ(outer.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(outer.transferred, 0U);
    EXPECT_FALSE(executor.pending);
}
TEST_F(SPIDMAOwner, QueuedCancellationAndEpochReuseCannotAffectNewIncarnation) {
    ASSERT_NO_FATAL_FAILURE(Enqueue());
    const auto stale = ticket;
    ASSERT_EQ(nx_bus_owner_cancel(&owner, ticket), NX_SUCCESS);
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&outer), NX_REQUEST_SETTLED);
    ASSERT_EQ(outer.result, NX_ERROR_CANCELLED);
    ASSERT_EQ(nx_request_prepare(&outer, 1000U), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Enqueue());
    EXPECT_EQ(ticket.slot, stale.slot);
    EXPECT_NE(ticket.epoch, stale.epoch);
    EXPECT_EQ(nx_bus_owner_cancel(&owner, stale), NX_ERROR_STATE);
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Complete(NX_SUCCESS, 2U));
}
TEST_F(SPIDMAOwner, ActiveCancellationKeepsQuarantinedChainBorrowUntilDrain) {
    ASSERT_NO_FATAL_FAILURE(Enqueue());
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(nx_bus_owner_cancel(&owner, ticket), NX_SUCCESS);
    EXPECT_CALL(provider, Service()).Times(2);
    EXPECT_CALL(provider, Cancel(&executor.inner)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_DRAINING);
    EXPECT_CALL(provider, Service()).WillOnce(Invoke([&] {
        (void)nx_request_transition(&executor.inner.base,
                                    NX_REQUEST_QUARANTINED);
    }));
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(executor.inner.segments, segments);
    EXPECT_TRUE(executor.pending);
    EXPECT_FALSE(nx_bus_owner_idle(&owner));
    ASSERT_NO_FATAL_FAILURE(Complete(NX_ERROR_CANCELLED, 1U));
}
TEST_F(SPIDMAOwner, LateCancelCannotOverrideAlreadyLatchedProviderSuccess) {
    ASSERT_NO_FATAL_FAILURE(Enqueue());
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(nx_bus_owner_cancel(&owner, ticket), NX_SUCCESS);
    EXPECT_CALL(provider, Service())
        .WillOnce(Invoke(
            [&] { nx_request_settle(&executor.inner.base, NX_SUCCESS, 2U); }))
        .WillOnce(Invoke([] {}));
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    EXPECT_EQ(outer.result, NX_SUCCESS);
    EXPECT_EQ(outer.transferred, 2U);
    EXPECT_FALSE(executor.pending);
}
TEST_F(SPIDMAOwner, CompletionPublisherMustExitBeforeOwnerCanBeIdle) {
    struct Observer {
        nx_bus_owner_t* owner;
        unsigned calls;
    } observer{&owner, 0U};
    const nx_wait_port_t completion{
        &observer, nullptr, nullptr, [](void* p) {
            auto* observed = static_cast<Observer*>(p);
            EXPECT_FALSE(nx_bus_owner_idle(observed->owner));
            ++observed->calls;
            return NX_SUCCESS;
        }};
    ASSERT_NO_FATAL_FAILURE(Enqueue(&completion));
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Complete(NX_SUCCESS, 2U));
    EXPECT_EQ(observer.calls, 1U);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}
TEST_F(SPIDMAOwner, RxCannotAuthorizeOwnerOrExecutorMetadataAsPayload) {
    for (void* control :
         {static_cast<void*>(&owner), static_cast<void*>(&slots[0]),
          static_cast<void*>(&executor), static_cast<void*>(&operation),
          static_cast<void*>(&outer)}) {
        segments[0].rx = control;
        EXPECT_EQ(nx_spi_dma_owner_submit(&owner, &outer, &operation, nullptr,
                                          &ticket),
                  NX_ERROR_INVALID);
        EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_READY);
        EXPECT_TRUE(nx_bus_owner_idle(&owner));
    }
    segments[0].rx = rx;
}
TEST_F(SPIDMAOwner, WakeBindingForwardsActualPolicyFailureWithoutSubmission) {
    const nx_irq_wake_t wake{&owner, [](void*) { return NX_SUCCESS; }, true};
    EXPECT_CALL(provider, Attach(&wake, 5U))
        .WillOnce(Return(NX_ERROR_PERMISSION));
    EXPECT_EQ(nx_spi_dma_owner_attach_wake(&binding, &wake, 5U),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_READY);
    EXPECT_CALL(provider, Attach(nullptr, 0U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_spi_dma_owner_attach_wake(&binding, nullptr, 0U), NX_SUCCESS);
}
TEST_F(SPIDMAOwner, IncompleteBindingProducesNoExecutorOrWakeEffects) {
    auto malformed = binding;
    malformed.contains = nullptr;
    nx_spi_dma_owner_executor_t fresh{};
    EXPECT_EQ(nx_spi_dma_owner_executor_port(&fresh, &malformed).start,
              nullptr);
    malformed = binding;
    malformed.attach_wake = nullptr;
    EXPECT_EQ(nx_spi_dma_owner_attach_wake(&malformed, nullptr, 0U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_spi_dma_owner_executor_port(nullptr, &binding).start, nullptr);
}
} /* namespace */
