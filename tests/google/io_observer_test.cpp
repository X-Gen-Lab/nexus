/**
 * \file            io_observer_test.cpp
 * \brief           Real optional observation and overflow budget boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/owner_observer.h"
#include "nexus/core/request_observer.h"
#include "nexus/io/stream_budget.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>

namespace {
class RequestObservation : public testing::Test {
  protected:
    nx_request_t request{};
    nx_request_slot_t slot{};
    nx_request_observer_t observer{};
    nx_request_observation_t output{};
    uint64_t epoch{};
    void SetUp() override {
        nx_request_initialize(&request);
        ASSERT_EQ(nx_request_prepare(&request, 1000U), NX_SUCCESS);
        ASSERT_EQ(nx_request_slot_bind(&slot, &request, &epoch), NX_SUCCESS);
    }
};
TEST_F(RequestObservation, InvalidAndStaleBindingRejectWithoutReference) {
    EXPECT_EQ(nx_request_observer_bind(nullptr, &slot, epoch),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_observer_bind(&observer, nullptr, epoch),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_observer_bind(&observer, &slot, 0U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_observer_bind(&observer, &slot, epoch + 1U),
              NX_ERROR_STATE);
    EXPECT_EQ(observer.slot, nullptr);
}
TEST_F(RequestObservation, BorrowedFactsDoNotExposePrivateResults) {
    ASSERT_EQ(nx_request_observer_bind(&observer, &slot, epoch), NX_SUCCESS);
    ASSERT_EQ(nx_request_admit(&request, NX_REQUEST_ACTIVE), NX_SUCCESS);
    request.result = NX_ERROR_IO;
    request.transferred = 99U;
    ASSERT_EQ(nx_request_observer_sample(&observer, 10U, &output), NX_SUCCESS);
    EXPECT_EQ(output.state, NX_REQUEST_ACTIVE);
    EXPECT_EQ(output.result, NX_ERROR_BUSY);
    EXPECT_EQ(output.transferred, 0U);
    EXPECT_FALSE(output.settled);
    ASSERT_EQ(nx_request_transition(&request, NX_REQUEST_QUARANTINED),
              NX_SUCCESS);
    ASSERT_EQ(nx_request_observer_sample(&observer, 30U, &output), NX_SUCCESS);
    EXPECT_EQ(output.state, NX_REQUEST_QUARANTINED);
    EXPECT_EQ(observer.max_gap, 20U);
    EXPECT_EQ(observer.transitions, 1U);
    nx_request_settle(&request, NX_ERROR_CANCELLED, 3U);
    ASSERT_EQ(nx_request_observer_sample(&observer, 40U, &output), NX_SUCCESS);
    EXPECT_TRUE(output.settled);
    EXPECT_EQ(output.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(output.transferred, 3U);
}
TEST_F(RequestObservation, OldEpochCannotReadAReusedRequest) {
    ASSERT_EQ(nx_request_observer_bind(&observer, &slot, epoch), NX_SUCCESS);
    ASSERT_EQ(nx_request_slot_release(&slot, epoch), NX_SUCCESS);
    uint64_t next{};
    ASSERT_EQ(nx_request_slot_bind(&slot, &request, &next), NX_SUCCESS);
    output.epoch = 777U;
    EXPECT_EQ(nx_request_observer_sample(&observer, 10U, &output),
              NX_ERROR_STATE);
    EXPECT_EQ(output.epoch, 777U);
    nx_request_observer_detach(&observer);
    ASSERT_EQ(nx_request_observer_bind(&observer, &slot, next), NX_SUCCESS);
    ASSERT_EQ(nx_request_observer_sample(&observer, 20U, &output), NX_SUCCESS);
    EXPECT_EQ(output.epoch, next);
}
TEST_F(RequestObservation, ReversedClockPreservesOutputAndSaturatesCounters) {
    ASSERT_EQ(nx_request_observer_bind(&observer, &slot, epoch), NX_SUCCESS);
    ASSERT_EQ(nx_request_observer_sample(&observer, 100U, &output), NX_SUCCESS);
    observer.samples = UINT32_MAX;
    observer.clock_errors = UINT32_MAX;
    output.observed_at = 777U;
    EXPECT_EQ(nx_request_observer_sample(&observer, 99U, &output), NX_ERROR_IO);
    EXPECT_EQ(output.observed_at, 777U);
    EXPECT_EQ(observer.clock_errors, UINT32_MAX);
    ASSERT_EQ(nx_request_observer_sample(&observer, 150U, &output), NX_SUCCESS);
    EXPECT_EQ(observer.samples, UINT32_MAX);
    EXPECT_EQ(observer.max_gap, 50U);
}
TEST_F(RequestObservation, OutputOverlapCannotRewriteBorrowedControl) {
    ASSERT_EQ(nx_request_observer_bind(&observer, &slot, epoch), NX_SUCCESS);
    auto* overlap = reinterpret_cast<nx_request_observation_t*>(&request);
    EXPECT_EQ(nx_request_observer_sample(&observer, 1U, overlap),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_READY);
    nx_request_observer_detach(&observer);
    EXPECT_EQ(observer.slot, nullptr);
    EXPECT_EQ(slot.request, &request);
}
TEST(StreamObservation, TotalFreeDoesNotClaimTheFifoCursorIsAvailable) {
    uint8_t data[2][4]{};
    nx_stream_slot_t slots[2]{};
    for (size_t index = 0U; index < 2U; ++index) {
        slots[index].data = data[index];
        slots[index].capacity = sizeof(data[index]);
    }
    nx_stream_t stream{};
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 2U), NX_SUCCESS);
    nx_stream_block_t loans[2]{};
    for (size_t index = 0U; index < 2U; ++index) {
        nx_stream_fill_t fill{};
        ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
        ASSERT_EQ(nx_stream_publish(&stream, &fill, 1U, 0U), NX_SUCCESS);
        ASSERT_EQ(nx_stream_acquire(&stream, &loans[index]), NX_SUCCESS);
    }
    ASSERT_EQ(nx_stream_release(&stream, &loans[1]), NX_SUCCESS);
    nx_stream_observation_t observation{};
    ASSERT_EQ(nx_stream_observe(&stream, &observation), NX_SUCCESS);
    EXPECT_EQ(observation.free_blocks, 1U);
    EXPECT_FALSE(observation.next_free);
    EXPECT_EQ(observation.outstanding, 1U);
    EXPECT_EQ(nx_stream_stop(&stream), NX_ERROR_BUSY);
    ASSERT_EQ(nx_stream_observe(&stream, &observation), NX_SUCCESS);
    EXPECT_TRUE(observation.stopping);
    ASSERT_EQ(nx_stream_release(&stream, &loans[0]), NX_SUCCESS);
    EXPECT_EQ(nx_stream_stop(&stream), NX_SUCCESS);
}
TEST(StreamObservation, InvalidMetadataAndAliasedOutputRemainUnchanged) {
    nx_stream_t stream{};
    nx_stream_observation_t output{};
    output.losses = 77U;
    EXPECT_EQ(nx_stream_observe(nullptr, &output), NX_ERROR_INVALID);
    EXPECT_EQ(nx_stream_observe(&stream, &output), NX_ERROR_INVALID);
    EXPECT_EQ(output.losses, 77U);
}
TEST(StreamBudget, CalculatesDeclaredPipelineCapacityAndStrictLatency) {
    nx_stream_budget_t budget{1000U, 100U, 2U, 500000U, 99999U, 2U, 1U, 8U};
    nx_stream_budget_result_t output{};
    ASSERT_EQ(nx_stream_budget_calculate(&budget, &output), NX_SUCCESS);
    EXPECT_EQ(output.retained_blocks, 5U);
    EXPECT_EQ(output.required_blocks, 8U);
    EXPECT_EQ(output.required_payload_bytes, 1600U);
    EXPECT_EQ(output.block_fill_us, 100000U);
    EXPECT_TRUE(output.capacity_sufficient);
    EXPECT_TRUE(output.target_latency_sufficient);
    budget.target_delay_us = 100000U;
    budget.configured_blocks = 7U;
    ASSERT_EQ(nx_stream_budget_calculate(&budget, &output), NX_SUCCESS);
    EXPECT_FALSE(output.target_latency_sufficient);
    EXPECT_FALSE(output.capacity_sufficient);
}
TEST(StreamBudget, RoundsFractionalRetentionUpWithoutAdditionOverflow) {
    nx_stream_budget_t budget{1000U, 100U, 1U, 1U, 0U, 2U, 1U, 4U};
    nx_stream_budget_result_t output{};
    ASSERT_EQ(nx_stream_budget_calculate(&budget, &output), NX_SUCCESS);
    EXPECT_EQ(output.retained_blocks, 1U);
    EXPECT_EQ(output.required_blocks, 4U);
    budget.release_delay_us = 0U;
    ASSERT_EQ(nx_stream_budget_calculate(&budget, &output), NX_SUCCESS);
    EXPECT_EQ(output.required_blocks, 3U);
}
TEST(StreamBudget, ArithmeticOverflowNeverCreatesAClampedPassingBudget) {
    nx_stream_budget_t budget{UINT64_MAX, 1U, 1U, 2U, 0U, 2U, 1U, 4U};
    nx_stream_budget_result_t output{};
    output.required_blocks = 777U;
    EXPECT_EQ(nx_stream_budget_calculate(&budget, &output), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(output.required_blocks, 777U);
    budget = {1U, 1U, SIZE_MAX, 1000000U, 0U, 2U, 1U, 4U};
    EXPECT_EQ(nx_stream_budget_calculate(&budget, &output), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(output.required_blocks, 777U);
    budget.units_per_second = 0U;
    EXPECT_EQ(nx_stream_budget_calculate(&budget, &output), NX_ERROR_INVALID);
}
TEST(StreamBudget, LargestBlockCannotOverflowDenominatorOrPayloadSilently) {
    nx_stream_budget_t budget{1U, SIZE_MAX, 1U, 0U, 0U, 1U, 1U, 1U};
    nx_stream_budget_result_t output{};
    output.required_blocks = 777U;
    EXPECT_EQ(nx_stream_budget_calculate(&budget, &output), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(output.required_blocks, 777U);
    /* This valid denominator boundary also fits the supported 32-bit ABI. */
    budget = {1U, UINT32_MAX, 1U, 0U, 0U, 1U, 0U, 1U};
    ASSERT_EQ(nx_stream_budget_calculate(&budget, &output), NX_SUCCESS);
    EXPECT_EQ(output.block_fill_us, UINT64_C(4294967295000000));
    EXPECT_EQ(output.required_payload_bytes, UINT32_MAX);
}
class OwnerHardware {
  public:
    MOCK_METHOD(nx_result_t, Start, (void*, nx_time_us_t));
    MOCK_METHOD(nx_result_t, Service, (nx_result_t*, size_t*));
    MOCK_METHOD(nx_result_t, Cancel, ());
};
class OwnerObservation : public testing::Test {
  protected:
    testing::StrictMock<OwnerHardware> hardware;
    nx_bus_owner_t owner{};
    nx_owner_slot_t slots[1]{};
    nx_owner_observer_t observer{};
    nx_owner_observation_t output{};
    nx_request_t request{};
    nx_owner_ticket_t ticket{};
    nx_time_us_t now{};
    uintptr_t depth{};
    int operation{};
    static uintptr_t enter(void* context) {
        auto* self = static_cast<OwnerObservation*>(context);
        EXPECT_EQ(self->depth, 0U);
        self->depth = 1U;
        return 0U;
    }
    static void leave(void* context, uintptr_t saved) {
        auto* self = static_cast<OwnerObservation*>(context);
        EXPECT_EQ(self->depth, 1U);
        self->depth = saved;
    }
    static nx_time_us_t clock(void* context) {
        return static_cast<OwnerObservation*>(context)->now;
    }
    static nx_result_t start(void* context, void* operation,
                             nx_time_us_t deadline) {
        auto* self = static_cast<OwnerObservation*>(context);
        EXPECT_EQ(self->depth, 0U);
        return self->hardware.Start(operation, deadline);
    }
    static nx_result_t service(void* context, nx_result_t* result,
                               size_t* count) {
        auto* self = static_cast<OwnerObservation*>(context);
        EXPECT_EQ(self->depth, 0U);
        return self->hardware.Service(result, count);
    }
    static nx_result_t cancel(void* context) {
        auto* self = static_cast<OwnerObservation*>(context);
        EXPECT_EQ(self->depth, 0U);
        return self->hardware.Cancel();
    }
    void SetUp() override {
        ASSERT_EQ(nx_bus_owner_init(&owner, slots, 1U, {this, enter, leave},
                                    {this, start, service, cancel, true},
                                    {clock, this}, nullptr),
                  NX_SUCCESS);
        nx_request_initialize(&request);
        ASSERT_EQ(nx_request_prepare(&request, 1000U), NX_SUCCESS);
    }
};
TEST_F(OwnerObservation, BindingDoesNotServiceAndRejectsIncompleteOwner) {
    EXPECT_EQ(nx_owner_observer_bind(nullptr, &owner), NX_ERROR_INVALID);
    nx_bus_owner_t incomplete{};
    EXPECT_EQ(nx_owner_observer_bind(&observer, &incomplete), NX_ERROR_INVALID);
    ASSERT_EQ(nx_owner_observer_bind(&observer, &owner), NX_SUCCESS);
    EXPECT_EQ(observer.owner, &owner);
    EXPECT_EQ(nx_owner_observer_bind(&observer, &owner), NX_ERROR_STATE);
}
TEST_F(OwnerObservation,
       RealErrorAndClockDiagnosticRemainIndependentDuringDrain) {
    ASSERT_EQ(nx_owner_observer_bind(&observer, &owner), NX_SUCCESS);
    ASSERT_EQ(
        nx_bus_owner_submit(&owner, &request, &operation, nullptr, &ticket),
        NX_SUCCESS);
    EXPECT_CALL(hardware, Start(&operation, 1000U))
        .WillOnce(testing::Return(NX_SUCCESS));
    ASSERT_EQ(nx_owner_observer_service(&observer, 100U, &output), NX_SUCCESS);
    EXPECT_TRUE(output.has_active);
    EXPECT_TRUE(output.accepting);
    EXPECT_FALSE(output.queued);
    EXPECT_FALSE(output.slot_available);
    EXPECT_EQ(output.active.epoch, ticket.epoch);
    EXPECT_EQ(output.active_state, NX_REQUEST_ACTIVE);
    EXPECT_CALL(hardware, Service(testing::_, testing::_))
        .WillOnce(testing::Return(NX_ERROR_IO));
    EXPECT_EQ(nx_owner_observer_service(&observer, 110U, &output), NX_ERROR_IO);
    EXPECT_EQ(output.active_state, NX_REQUEST_QUARANTINED);
    EXPECT_EQ(output.errors, 1U);
    EXPECT_EQ(slots[0].identity.request, &request);
    EXPECT_CALL(hardware, Service(testing::_, testing::_))
        .WillOnce(testing::DoAll(testing::SetArgPointee<0>(NX_ERROR_CANCELLED),
                                 testing::SetArgPointee<1>(2U),
                                 testing::Return(NX_SUCCESS)));
    EXPECT_EQ(nx_owner_observer_service(&observer, 109U, &output), NX_SUCCESS);
    EXPECT_EQ(output.service_result, NX_SUCCESS);
    EXPECT_EQ(output.diagnostic_result, NX_ERROR_IO);
    EXPECT_EQ(output.clock_errors, 1U);
    EXPECT_EQ(output.calls, 3U);
    EXPECT_TRUE(output.idle);
    EXPECT_TRUE(output.slot_available);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
    EXPECT_EQ(slots[0].identity.request, nullptr);
    nx_owner_observer_detach(&observer);
    EXPECT_EQ(observer.owner, nullptr);
}
TEST_F(OwnerObservation, BusyAndSaturatingCountersNeverInventQuarantine) {
    ASSERT_EQ(nx_owner_observer_bind(&observer, &owner), NX_SUCCESS);
    ASSERT_EQ(
        nx_bus_owner_submit(&owner, &request, &operation, nullptr, &ticket),
        NX_SUCCESS);
    observer.observation.calls = UINT32_MAX;
    EXPECT_CALL(hardware, Start(&operation, 1000U))
        .WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_owner_observer_service(&observer, 1U, &output), NX_ERROR_BUSY);
    EXPECT_EQ(output.calls, UINT32_MAX);
    EXPECT_EQ(output.busy, 1U);
    EXPECT_EQ(output.errors, 0U);
    EXPECT_FALSE(output.has_active);
    EXPECT_FALSE(output.idle);
    EXPECT_TRUE(output.accepting);
    EXPECT_TRUE(output.queued);
    EXPECT_FALSE(output.slot_available);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_QUEUED);
}
TEST_F(OwnerObservation, ClosedIdleOwnerReportsAdmissionSeparatelyFromLoans) {
    ASSERT_EQ(nx_owner_observer_bind(&observer, &owner), NX_SUCCESS);
    nx_bus_owner_stop(&owner);
    ASSERT_EQ(nx_owner_observer_service(&observer, 10U, &output), NX_SUCCESS);
    EXPECT_FALSE(output.accepting);
    EXPECT_FALSE(output.queued);
    EXPECT_TRUE(output.slot_available);
    EXPECT_TRUE(output.idle);
}
TEST_F(OwnerObservation, OutputAliasRejectsBeforeHardwareService) {
    ASSERT_EQ(nx_owner_observer_bind(&observer, &owner), NX_SUCCESS);
    auto* overlap = reinterpret_cast<nx_owner_observation_t*>(&observer);
    EXPECT_EQ(nx_owner_observer_service(&observer, 1U, overlap),
              NX_ERROR_INVALID);
    EXPECT_EQ(observer.observation.calls, 0U);
}
} /* namespace */
