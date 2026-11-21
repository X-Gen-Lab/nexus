/**
 * \file            common_owner_integration_test.cpp
 * \brief           Explicit periodic pumping preserves existing owner budgets
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/owner_observer.h"
#include "nexus/components/periodic.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
class TimedOwnerIntegration : public testing::Test {
  protected:
    MOCK_METHOD(nx_result_t, Start, (void*, nx_time_us_t));
    MOCK_METHOD(nx_result_t, Service, (nx_result_t*, size_t*));
    MOCK_METHOD(nx_result_t, Cancel, ());
    nx_time_us_t now = 0;
    nx_bus_owner_t owner = {};
    nx_owner_slot_t slot = {};
    nx_owner_observer_t observer = {};
    nx_periodic_t periodic = {};
    static nx_time_us_t clock(void* context) {
        return static_cast<TimedOwnerIntegration*>(context)->now;
    }
    static uintptr_t enter(void*) {
        return 0;
    }
    static void leave(void*, uintptr_t) {
    }
    static nx_result_t start(void* context, void* operation,
                             nx_time_us_t deadline) {
        return static_cast<TimedOwnerIntegration*>(context)->Start(operation,
                                                                   deadline);
    }
    static nx_result_t service(void* context, nx_result_t* result,
                               size_t* transferred) {
        return static_cast<TimedOwnerIntegration*>(context)->Service(
            result, transferred);
    }
    static nx_result_t cancel(void* context) {
        return static_cast<TimedOwnerIntegration*>(context)->Cancel();
    }
    void SetUp() override {
        ASSERT_EQ(nx_bus_owner_init(&owner, &slot, 1, {nullptr, enter, leave},
                                    {this, start, service, cancel, false},
                                    {clock, this}, nullptr),
                  NX_SUCCESS);
        ASSERT_EQ(nx_owner_observer_bind(&observer, &owner), NX_SUCCESS);
        ASSERT_EQ(nx_periodic_reset(&periodic, now, 20, NX_PERIODIC_SKIP),
                  NX_SUCCESS);
    }
};

TEST_F(TimedOwnerIntegration, QueueResidenceConsumesOriginalTransactionBudget) {
    nx_request_t request = {};
    nx_request_initialize(&request);
    nx_time_us_t deadline = 0;
    ASSERT_EQ(nx_deadline_finite_after(now, 10, &deadline), NX_SUCCESS);
    ASSERT_EQ(nx_request_prepare(&request, deadline), NX_SUCCESS);
    nx_owner_ticket_t ticket = {};
    int operation = 17;
    ASSERT_EQ(
        nx_bus_owner_submit(&owner, &request, &operation, nullptr, &ticket),
        NX_SUCCESS);
    EXPECT_CALL(*this, Start(testing::_, testing::_)).Times(0);
    EXPECT_CALL(*this, Service(testing::_, testing::_)).Times(0);
    EXPECT_CALL(*this, Cancel()).Times(0);
    now = 20;
    nx_periodic_release_t release = {};
    ASSERT_EQ(nx_periodic_poll(&periodic, now, 1, &release), NX_SUCCESS);
    ASSERT_EQ(release.releases, 1U);
    nx_owner_observation_t observation = {};
    ASSERT_EQ(nx_owner_observer_service(&observer, now, &observation),
              NX_SUCCESS);
    nx_result_t result = NX_SUCCESS;
    size_t transferred = 99;
    ASSERT_EQ(nx_request_result(&request, &result, &transferred), NX_SUCCESS);
    EXPECT_EQ(result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(transferred, 0U);
    EXPECT_TRUE(observation.idle);
    EXPECT_EQ(observation.calls, 1U);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
    nx_owner_observer_detach(&observer);
    nx_bus_owner_stop(&owner);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}
} /* namespace */
