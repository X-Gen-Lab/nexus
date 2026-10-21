/**
 * \file            mapped_storage_test.cpp
 * \brief           Mapped-loan, NOR mutation and source-drain regressions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/native_mapped_storage.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>

namespace {
using testing::_;
using testing::InSequence;
using testing::Return;

class MappedStorage : public testing::Test {
  protected:
    std::array<uint8_t, 64> bytes{};
    std::array<uint8_t, 16> input{};
    nx_native_mapped_storage_t native{};
    nx_mapped_storage_owner_t owner{};
    nx_mapped_storage_slot_t slots[2]{};
    const nx_mapped_storage_port_t port{&nx_mapped_storage_owner_ops, &owner};
    nx_mapped_storage_request_t request{};
    nx_mapped_storage_ticket_t ticket{};
    nx_time_us_t now = 1;

    static nx_time_us_t clock(void* context) {
        return *static_cast<nx_time_us_t*>(context);
    }

    void SetUp() override {
        bytes.fill(0xff);
        input.fill(0xa5);
        nx_mapped_storage_backend_t backend{};
        ASSERT_EQ(nx_native_mapped_storage_init(
                      &native, bytes.data(), bytes.size(), 4, 16, 4, &backend),
                  NX_SUCCESS);
        ASSERT_EQ(nx_mapped_storage_owner_init(&owner, slots, 2, backend,
                                               {clock, &now}),
                  NX_SUCCESS);
        nx_request_initialize(&request.base);
    }

    void prepare(size_t offset = 0, size_t length = 16,
                 nx_time_us_t deadline = NX_DEADLINE_NEVER) {
        ASSERT_EQ(nx_mapped_storage_prepare(&request, NX_MAPPED_STORAGE_PROGRAM,
                                            offset, input.data(), length,
                                            deadline),
                  NX_SUCCESS);
    }

    void settle() {
        for (unsigned step = 0;
             step < 20 && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
             ++step) {
            nx_mapped_storage_service(&port);
        }
        ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    }

    void result(nx_result_t expected, size_t count) {
        nx_result_t actual = NX_ERROR_STATE;
        size_t completed = SIZE_MAX;
        ASSERT_EQ(nx_request_result(&request.base, &actual, &completed),
                  NX_SUCCESS);
        EXPECT_EQ(actual, expected);
        EXPECT_EQ(completed, count);
    }
};

TEST_F(MappedStorage, ReadLoanExcludesMutationUntilExactRelease) {
    nx_mapped_storage_loan_t loan{};
    ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 4, 8,
                                        &loan),
              NX_SUCCESS);
    EXPECT_EQ(loan.data, bytes.data() + 4);
    EXPECT_EQ(loan.length, 8U);
    prepare();
    EXPECT_EQ(nx_mapped_storage_submit(&port, &request, &ticket),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_FALSE(native.active);
    EXPECT_EQ(nx_mapped_storage_release(&port, &loan), NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_release(&port, &loan), NX_ERROR_STATE);
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0, 4,
                                        &loan),
              NX_ERROR_BUSY);
    nx_mapped_storage_service(&port);
    EXPECT_TRUE(std::equal(input.begin(), input.begin() + 4, bytes.begin()));
    EXPECT_EQ(bytes[4], 0xff);
    settle();
    result(NX_SUCCESS, 16);
    EXPECT_FALSE(native.active);
    EXPECT_EQ(native.input, nullptr);
    ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0,
                                        16, &loan),
              NX_SUCCESS);
    EXPECT_TRUE(std::equal(input.begin(), input.end(),
                           static_cast<const uint8_t*>(loan.data)));
    EXPECT_EQ(nx_mapped_storage_release(&port, &loan), NX_SUCCESS);
}

TEST_F(MappedStorage, NativeProgramsOnlyOneToZeroAndErasesWholeBlocks) {
    prepare();
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    settle();
    result(NX_SUCCESS, 16);
    input.fill(0xff);
    prepare();
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    settle();
    result(NX_ERROR_IO, 0);
    EXPECT_EQ(bytes[0], 0xa5);
    ASSERT_EQ(nx_mapped_storage_prepare(&request, NX_MAPPED_STORAGE_ERASE, 0,
                                        nullptr, 16, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    settle();
    result(NX_SUCCESS, 16);
    EXPECT_TRUE(std::all_of(bytes.begin(), bytes.end(),
                            [](uint8_t value) { return value == 0xff; }));
}

TEST_F(MappedStorage, AdmissionRejectsOverflowAlignmentAndAliasedInput) {
    EXPECT_EQ(nx_mapped_storage_prepare(&request, NX_MAPPED_STORAGE_PROGRAM,
                                        SIZE_MAX - 3, input.data(), 8,
                                        NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    prepare(0, 8);
    request.offset = SIZE_MAX - 3;
    EXPECT_EQ(nx_mapped_storage_submit(&port, &request, &ticket),
              NX_ERROR_INVALID);
    prepare(1, 4);
    EXPECT_EQ(nx_mapped_storage_submit(&port, &request, &ticket),
              NX_ERROR_INVALID);
    prepare(0, 3);
    EXPECT_EQ(nx_mapped_storage_submit(&port, &request, &ticket),
              NX_ERROR_INVALID);
    ASSERT_EQ(nx_mapped_storage_prepare(&request, NX_MAPPED_STORAGE_PROGRAM, 0,
                                        bytes.data() + 32, 4,
                                        NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_submit(&port, &request, &ticket),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_mapped_storage_prepare(&request, NX_MAPPED_STORAGE_PROGRAM, 0,
                                        &request, 4, NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_mapped_storage_prepare(&request, NX_MAPPED_STORAGE_ERASE, 0,
                                        input.data(), 16, NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_mapped_storage_prepare(&request, NX_MAPPED_STORAGE_PROGRAM, 0,
                                        nullptr, 0, NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_FALSE(native.active);
    EXPECT_EQ(bytes[0], 0xff);
}

TEST_F(MappedStorage, MapBoundsCapacityAndCapabilityRejectWithoutLoans) {
    nx_mapped_storage_loan_t first{}, second{}, rejected{};
    EXPECT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ,
                                        SIZE_MAX, 2, &rejected),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0, 0,
                                        &rejected),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_XIP, 0, 4,
                                        &rejected),
              NX_ERROR_UNSUPPORTED);
    EXPECT_FALSE(native.mapped);
    ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0, 4,
                                        &first),
              NX_SUCCESS);
    ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 4, 4,
                                        &second),
              NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 8, 4,
                                        &rejected),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_mapped_storage_release(&port, &first), NX_SUCCESS);
    EXPECT_TRUE(native.mapped);
    EXPECT_EQ(nx_mapped_storage_release(&port, &second), NX_SUCCESS);
    EXPECT_FALSE(native.mapped);
}

TEST_F(MappedStorage, EpochsNeverRepeatAndExhaustedLoanSlotsRetire) {
    slots[0].epoch = UINT64_MAX - 1;
    nx_mapped_storage_loan_t final{}, next{};
    ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0, 4,
                                        &final),
              NX_SUCCESS);
    EXPECT_EQ(final.epoch, UINT64_MAX);
    ASSERT_EQ(nx_mapped_storage_release(&port, &final), NX_SUCCESS);
    ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0, 4,
                                        &next),
              NX_SUCCESS);
    EXPECT_NE(next.slot, final.slot);
    EXPECT_EQ(nx_mapped_storage_release(&port, &final), NX_ERROR_STATE);
    EXPECT_EQ(nx_mapped_storage_release(&port, &next), NX_SUCCESS);
    owner.writer_epoch = UINT64_MAX;
    prepare();
    EXPECT_EQ(nx_mapped_storage_submit(&port, &request, &ticket),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}

TEST_F(MappedStorage, ForeignAndStaleTokensCannotControlAnotherIncarnation) {
    nx_mapped_storage_owner_t other{};
    nx_mapped_storage_slot_t other_slots[1]{};
    std::array<uint8_t, 32> other_bytes{};
    other_bytes.fill(0xff);
    nx_native_mapped_storage_t other_native{};
    nx_mapped_storage_backend_t backend{};
    ASSERT_EQ(nx_native_mapped_storage_init(&other_native, other_bytes.data(),
                                            other_bytes.size(), 4, 16, 4,
                                            &backend),
              NX_SUCCESS);
    ASSERT_EQ(nx_mapped_storage_owner_init(&other, other_slots, 1, backend,
                                           {clock, &now}),
              NX_SUCCESS);
    const nx_mapped_storage_port_t second{&nx_mapped_storage_owner_ops, &other};
    nx_mapped_storage_loan_t loan{};
    ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0, 4,
                                        &loan),
              NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_release(&second, &loan), NX_ERROR_STATE);
    ASSERT_EQ(nx_mapped_storage_release(&port, &loan), NX_SUCCESS);
    prepare();
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    auto stale = ticket;
    EXPECT_EQ(nx_mapped_storage_cancel(&second, ticket), NX_ERROR_STATE);
    settle();
    prepare();
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    EXPECT_NE(stale.epoch, ticket.epoch);
    EXPECT_EQ(nx_mapped_storage_cancel(&port, stale), NX_ERROR_STATE);
    settle();
    EXPECT_EQ(other_bytes[0], 0xff);
}

TEST_F(MappedStorage, CancelRetainsPartialMutationUntilActualDrain) {
    prepare();
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    nx_mapped_storage_service(&port);
    native.hold_drain = true;
    ASSERT_EQ(nx_mapped_storage_cancel(&port, ticket), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    nx_mapped_storage_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_NE(native.input, nullptr);
    EXPECT_EQ(bytes[4], 0xff);
    native.drain_error = true;
    nx_mapped_storage_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_mapped_storage_stop(&port), NX_ERROR_BUSY);
    native.hold_drain = false;
    native.drain_error = false;
    settle();
    result(NX_ERROR_IO, 4);
    EXPECT_EQ(bytes[4], 0xff);
    EXPECT_EQ(native.input, nullptr);
    EXPECT_EQ(nx_mapped_storage_stop(&port), NX_SUCCESS);
}

TEST_F(MappedStorage, CancellationSettlesCancelledAfterSourceWithdrawal) {
    prepare();
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    ASSERT_EQ(nx_mapped_storage_cancel(&port, ticket), NX_SUCCESS);
    settle();
    result(NX_ERROR_CANCELLED, 0);
    EXPECT_EQ(bytes[0], 0xff);
}

TEST_F(MappedStorage, NativeAbortPreventsAnyFurtherBackendMutation) {
    prepare();
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    nx_mapped_storage_service(&port);
    ASSERT_EQ(nx_mapped_storage_cancel(&port, ticket), NX_SUCCESS);
    size_t completed = 0;
    EXPECT_EQ(owner.backend.poll(owner.backend.context, &completed),
              NX_ERROR_CANCELLED);
    EXPECT_EQ(completed, 4U);
    EXPECT_EQ(bytes[4], 0xff);
    EXPECT_NE(native.input, nullptr);
    settle();
    result(NX_ERROR_CANCELLED, 4);
}

TEST_F(MappedStorage, PrivateBackendStateCannotBecomePayloadOrLoanOutput) {
    ASSERT_EQ(nx_mapped_storage_prepare(&request, NX_MAPPED_STORAGE_PROGRAM, 0,
                                        &native, 4, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_submit(&port, &request, &ticket),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_FALSE(native.active);
    EXPECT_EQ(nx_mapped_storage_acquire(
                  &port, NX_MAPPED_STORAGE_CPU_READ, 0, 4,
                  reinterpret_cast<nx_mapped_storage_loan_t*>(&native)),
              NX_ERROR_INVALID);
}

TEST_F(MappedStorage, DeadlineEqualityCancelsBeforeTheNextMutation) {
    now = 9;
    prepare(0, 16, 10);
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    now = 10;
    settle();
    result(NX_ERROR_TIMEOUT, 0);
    EXPECT_EQ(bytes[0], 0xff);
    prepare(0, 4, 10);
    EXPECT_EQ(nx_mapped_storage_submit(&port, &request, &ticket),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}

TEST_F(MappedStorage, TerminalSuccessSurvivesLateCancelAndDeadline) {
    prepare(0, 4, 2);
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    nx_mapped_storage_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    now = 2;
    ASSERT_EQ(nx_mapped_storage_cancel(&port, ticket), NX_SUCCESS);
    settle();
    result(NX_SUCCESS, 4);
}

TEST_F(MappedStorage, StopClosesAdmissionWithoutRevokingExistingReadLoans) {
    nx_mapped_storage_loan_t loan{}, rejected{};
    ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0, 4,
                                        &loan),
              NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0, 4,
                                        &rejected),
              NX_ERROR_STATE);
    EXPECT_EQ(*static_cast<const uint8_t*>(loan.data), 0xff);
    EXPECT_EQ(nx_mapped_storage_release(&port, &loan), NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_stop(&port), NX_SUCCESS);
}

TEST_F(MappedStorage, FaultReportsPartialBytesAndNeverReplaysMutation) {
    native.fault_after = 6;
    prepare();
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    settle();
    result(NX_ERROR_IO, 6);
    EXPECT_EQ(bytes[5], 0xa5);
    EXPECT_EQ(bytes[6], 0xff);
    native.fault_after = SIZE_MAX;
    nx_mapped_storage_service(&port);
    EXPECT_EQ(bytes[6], 0xff);
}

class Backend {
  public:
    std::array<uint8_t, sizeof(nx_mapped_storage_loan_t)> scratch{};
    MOCK_METHOD(nx_result_t, map, ());
    MOCK_METHOD(nx_result_t, unmap, ());
    MOCK_METHOD(nx_result_t, start,
                (nx_mapped_storage_operation_t, size_t, const void*, size_t));
    MOCK_METHOD(nx_result_t, poll, (size_t*));
    MOCK_METHOD(nx_result_t, cancel, ());
    MOCK_METHOD(nx_result_t, drain, ());

    nx_mapped_storage_backend_t bind(const uint8_t* bytes, size_t size) {
        return {this,
                sizeof(*this),
                {bytes, size, 4, 16, 7},
                [](void* p) { return static_cast<Backend*>(p)->map(); },
                [](void* p) { return static_cast<Backend*>(p)->unmap(); },
                [](void* p, nx_mapped_storage_operation_t op, size_t offset,
                   const void* data, size_t length) {
                    return static_cast<Backend*>(p)->start(op, offset, data,
                                                           length);
                },
                [](void* p, size_t* completed) {
                    return static_cast<Backend*>(p)->poll(completed);
                },
                [](void* p) { return static_cast<Backend*>(p)->cancel(); },
                [](void* p) { return static_cast<Backend*>(p)->drain(); }};
    }
};

TEST_F(MappedStorage, FailedLastUnmapRetainsExactLoanAndExcludesWriters) {
    testing::StrictMock<Backend> backend;
    ASSERT_EQ(nx_mapped_storage_owner_init(&owner, slots, 2,
                                           backend.bind(bytes.data(), 64),
                                           {clock, &now}),
              NX_SUCCESS);
    {
        InSequence order;
        EXPECT_CALL(backend, map()).WillOnce(Return(NX_SUCCESS));
        EXPECT_CALL(backend, unmap()).WillOnce(Return(NX_ERROR_IO));
        EXPECT_CALL(backend, unmap()).WillOnce(Return(NX_SUCCESS));
    }
    nx_mapped_storage_loan_t xip{}, dma{};
    ASSERT_EQ(
        nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_XIP, 0, 4, &xip),
        NX_SUCCESS);
    ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_DMA_READ, 8, 4,
                                        &dma),
              NX_SUCCESS);
    ASSERT_EQ(nx_mapped_storage_release(&port, &xip), NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_release(&port, &dma), NX_ERROR_IO);
    prepare();
    EXPECT_EQ(nx_mapped_storage_submit(&port, &request, &ticket),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_mapped_storage_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_mapped_storage_release(&port, &dma), NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_stop(&port), NX_SUCCESS);
}

TEST_F(MappedStorage,
       ReleaseRejectsTokensInsideApertureAndPrivateBackendState) {
    for (bool in_aperture : {true, false}) {
        testing::StrictMock<Backend> backend;
        ASSERT_EQ(nx_mapped_storage_owner_init(&owner, slots, 2,
                                               backend.bind(bytes.data(), 64),
                                               {clock, &now}),
                  NX_SUCCESS);
        EXPECT_CALL(backend, map()).WillOnce(Return(NX_SUCCESS));
        EXPECT_CALL(backend, unmap()).Times(0);
        nx_mapped_storage_loan_t original{};
        ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ,
                                            0, bytes.size(), &original),
                  NX_SUCCESS);
        void* alias = in_aperture ? static_cast<void*>(bytes.data())
                                  : static_cast<void*>(backend.scratch.data());
        std::memcpy(alias, &original, sizeof(original));
        EXPECT_EQ(
            nx_mapped_storage_release(
                &port, static_cast<const nx_mapped_storage_loan_t*>(alias)),
            NX_ERROR_INVALID);
        testing::Mock::VerifyAndClearExpectations(&backend);
        EXPECT_CALL(backend, unmap()).WillOnce(Return(NX_SUCCESS));
        EXPECT_EQ(nx_mapped_storage_release(&port, &original), NX_SUCCESS);
    }
}

TEST_F(MappedStorage, FailedStartIsAcceptedAndQuarantinedUntilBackendDetaches) {
    testing::StrictMock<Backend> backend;
    ASSERT_EQ(nx_mapped_storage_owner_init(&owner, slots, 2,
                                           backend.bind(bytes.data(), 64),
                                           {clock, &now}),
              NX_SUCCESS);
    {
        InSequence order;
        EXPECT_CALL(backend,
                    start(NX_MAPPED_STORAGE_PROGRAM, 0, input.data(), 16))
            .WillOnce(Return(NX_ERROR_IO));
        EXPECT_CALL(backend, cancel()).WillOnce(Return(NX_SUCCESS));
        EXPECT_CALL(backend, drain()).WillOnce(Return(NX_ERROR_IO));
        EXPECT_CALL(backend, drain()).WillOnce(Return(NX_SUCCESS));
    }
    prepare();
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    nx_mapped_storage_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    nx_mapped_storage_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    result(NX_ERROR_IO, 0);
}

TEST_F(MappedStorage, InvalidProgressCancelsAndCannotPublishAnOverflowCount) {
    testing::StrictMock<Backend> backend;
    ASSERT_EQ(nx_mapped_storage_owner_init(&owner, slots, 2,
                                           backend.bind(bytes.data(), 64),
                                           {clock, &now}),
              NX_SUCCESS);
    EXPECT_CALL(backend, start(_, _, _, _)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(backend, poll(_)).WillOnce([](size_t* count) {
        *count = SIZE_MAX;
        return NX_SUCCESS;
    });
    EXPECT_CALL(backend, cancel()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(backend, drain()).WillOnce(Return(NX_SUCCESS));
    prepare();
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    settle();
    result(NX_ERROR_IO, 0);
}

TEST_F(MappedStorage, ConstructorAndTypedDispatchRejectInvalidStorage) {
    auto backend = owner.backend;
    EXPECT_EQ(
        nx_mapped_storage_owner_init(&owner, slots, 0, backend, {clock, &now}),
        NX_ERROR_INVALID);
    backend.geometry.size = SIZE_MAX;
    EXPECT_EQ(
        nx_mapped_storage_owner_init(&owner, slots, 2, backend, {clock, &now}),
        NX_ERROR_INVALID);
    backend = owner.backend;
    backend.geometry.erase_unit = 6;
    EXPECT_EQ(
        nx_mapped_storage_owner_init(&owner, slots, 2, backend, {clock, &now}),
        NX_ERROR_INVALID);
    backend = owner.backend;
    backend.geometry.mapped_base = reinterpret_cast<const uint8_t*>(&owner);
    EXPECT_EQ(
        nx_mapped_storage_owner_init(&owner, slots, 2, backend, {clock, &now}),
        NX_ERROR_INVALID);
    nx_mapped_storage_ops_t absent{};
    const nx_mapped_storage_port_t unsupported{&absent, &owner};
    EXPECT_EQ(nx_mapped_storage_stop(&unsupported), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_mapped_storage_stop(nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_mapped_storage_geometry(nullptr), nullptr);
    EXPECT_EQ(nx_mapped_storage_geometry(&port)->size, bytes.size());
}
} /* namespace */
