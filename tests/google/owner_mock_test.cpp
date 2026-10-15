/**
 * \file            owner_mock_test.cpp
 * \brief           Real owner lifecycle checked through strict executor mocks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/bus_owner.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

/** \brief Inject hardware interactions without replacing owner logic. */
class Executor {
  public:
    MOCK_METHOD(nx_result_t, start, (void*, nx_time_us_t));
    MOCK_METHOD(nx_result_t, service, (nx_result_t*, size_t*));
    MOCK_METHOD(nx_result_t, cancel, ());
};

/** \brief Provide exact caller-owned slots and explicit dependency ports. */
class OwnerTest : public ::testing::Test {
  protected:
    StrictMock<Executor> executor;
    nx_bus_owner_t owner{};
    nx_owner_slot_t slots[1]{};
    nx_request_t request{};
    nx_owner_ticket_t ticket{};
    nx_time_us_t now = 0;
    uintptr_t guard_depth = 0;
    int operation = 1;

    /** \brief Model a nonrecursive short metadata guard. */
    static uintptr_t enter(void* context) {
        auto* fixture = static_cast<OwnerTest*>(context);
        EXPECT_EQ(fixture->guard_depth, 0U);
        fixture->guard_depth = 1;
        return 0;
    }

    /** \brief Restore the exact incoming guard state. */
    static void leave(void* context, uintptr_t state) {
        auto* fixture = static_cast<OwnerTest*>(context);
        EXPECT_EQ(fixture->guard_depth, 1U);
        fixture->guard_depth = state;
    }

    /** \brief Check the hardware start runs outside metadata exclusion. */
    static nx_result_t start(void* context, void* item, nx_time_us_t deadline) {
        auto* fixture = static_cast<OwnerTest*>(context);
        EXPECT_EQ(fixture->guard_depth, 0U);
        return fixture->executor.start(item, deadline);
    }

    /** \brief Completion observes hardware drain outside metadata exclusion. */
    static nx_result_t service(void* context, nx_result_t* result,
                               size_t* transferred) {
        auto* fixture = static_cast<OwnerTest*>(context);
        EXPECT_EQ(fixture->guard_depth, 0U);
        return fixture->executor.service(result, transferred);
    }

    /** \brief Cancellation initiates drain without reclaiming the borrow. */
    static nx_result_t cancel(void* context) {
        auto* fixture = static_cast<OwnerTest*>(context);
        EXPECT_EQ(fixture->guard_depth, 0U);
        return fixture->executor.cancel();
    }

    /** \brief Supply the request's monotonic time domain. */
    static nx_time_us_t read(void* context) {
        return static_cast<OwnerTest*>(context)->now;
    }

    /** \brief Initialize only fresh static storage, never a live borrow. */
    void SetUp() override {
        const nx_owner_guard_port_t guard = {this, enter, leave};
        const nx_owner_executor_port_t port = {this, start, service, cancel,
                                               false};
        const nx_clock_t clock = {read, this};
        ASSERT_EQ(
            nx_bus_owner_init(&owner, slots, 1, guard, port, clock, nullptr),
            NX_SUCCESS);
        nx_request_initialize(&request);
        ASSERT_EQ(nx_request_prepare(&request, 1000), NX_SUCCESS);
    }

    /** \brief Establish one real admission through the public owner API. */
    void submit() {
        ASSERT_EQ(
            nx_bus_owner_submit(&owner, &request, &operation, nullptr, &ticket),
            NX_SUCCESS);
    }
};

TEST_F(OwnerTest, CancellationRetainsStorageUntilHardwareDrain) {
    submit();
    EXPECT_CALL(executor, start(&operation, 1000)).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    ASSERT_EQ(nx_bus_owner_cancel(&owner, ticket), NX_SUCCESS);
    EXPECT_CALL(executor, cancel()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(executor, service(_, _)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_DRAINING);
    EXPECT_EQ(slots[0].identity.request, &request);
    EXPECT_EQ(slots[0].operation, &operation);
    EXPECT_FALSE(nx_bus_owner_idle(&owner));
    EXPECT_CALL(executor, service(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(NX_ERROR_CANCELLED),
                        SetArgPointee<1>(3), Return(NX_SUCCESS)));
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
    EXPECT_EQ(slots[0].identity.request, nullptr);
    EXPECT_EQ(slots[0].operation, nullptr);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
    nx_result_t result = NX_SUCCESS;
    size_t transferred = 0;
    ASSERT_EQ(nx_request_result(&request, &result, &transferred), NX_SUCCESS);
    EXPECT_EQ(result, NX_ERROR_CANCELLED);
    EXPECT_EQ(transferred, 3U);
}

TEST_F(OwnerTest, FullQueueRejectsWithoutBorrowOrExecutorEffects) {
    submit();
    nx_request_t rejected{};
    nx_request_initialize(&rejected);
    ASSERT_EQ(nx_request_prepare(&rejected, 1000), NX_SUCCESS);
    nx_owner_ticket_t rejected_ticket{};
    int other_operation = 2;
    EXPECT_EQ(nx_bus_owner_submit(&owner, &rejected, &other_operation, nullptr,
                                  &rejected_ticket),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(nx_request_state(&rejected), NX_REQUEST_READY);
    EXPECT_EQ(slots[0].identity.request, &request);
}

TEST_F(OwnerTest, ExpiredQueueSettlesWithoutTouchingHardware) {
    submit();
    now = 1000;
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    nx_result_t result = NX_SUCCESS;
    size_t transferred = 1;
    ASSERT_EQ(nx_request_result(&request, &result, &transferred), NX_SUCCESS);
    EXPECT_EQ(result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(transferred, 0U);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}

TEST_F(OwnerTest, FailedStartSettlesAndDetachesRejectedHardwareLoan) {
    submit();
    EXPECT_CALL(executor, start(&operation, 1000))
        .WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
    EXPECT_EQ(slots[0].identity.request, nullptr);
    EXPECT_EQ(slots[0].operation, nullptr);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}

TEST_F(OwnerTest, StaleCancellationCannotTargetReusedSlot) {
    submit();
    const nx_owner_ticket_t stale = ticket;
    ASSERT_EQ(nx_bus_owner_cancel(&owner, ticket), NX_SUCCESS);
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    ASSERT_EQ(nx_request_prepare(&request, 1000), NX_SUCCESS);
    submit();
    EXPECT_NE(ticket.epoch, stale.epoch);
    EXPECT_EQ(nx_bus_owner_cancel(&owner, stale), NX_ERROR_STATE);
    EXPECT_FALSE(slots[0].cancel_requested);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_QUEUED);
}
} /* namespace */
