/**
 * \file            core_lifecycle_test.cpp
 * \brief           Reuse, clock publication and finite-domain boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-04
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/request.h"
#include <gtest/gtest.h>

namespace {
/** \brief Reach a state through the public single-owner contract. */
void reach(nx_request_t* request, nx_request_state_t state) {
    nx_request_initialize(request);
    ASSERT_EQ(nx_request_prepare(request, 100U), NX_SUCCESS);
    if (state == NX_REQUEST_READY) {
        return;
    }
    ASSERT_EQ(nx_request_admit(request, NX_REQUEST_QUEUED), NX_SUCCESS);
    if (state == NX_REQUEST_QUEUED) {
        return;
    }
    ASSERT_EQ(nx_request_transition(request, NX_REQUEST_ACTIVE), NX_SUCCESS);
    if (state == NX_REQUEST_ACTIVE) {
        return;
    }
    ASSERT_EQ(nx_request_transition(request, NX_REQUEST_DRAINING), NX_SUCCESS);
    if (state == NX_REQUEST_QUARANTINED) {
        ASSERT_EQ(nx_request_transition(request, NX_REQUEST_QUARANTINED),
                  NX_SUCCESS);
    } else if (state == NX_REQUEST_SETTLED) {
        nx_request_settle(request, NX_ERROR_CANCELLED, 7U);
    }
}

TEST(CoreLifecycle, TransitionMatrixNeverRewindsOrReadmitsARequest) {
    constexpr nx_request_state_t states[] = {
        NX_REQUEST_READY,    NX_REQUEST_QUEUED,  NX_REQUEST_ACTIVE,
        NX_REQUEST_DRAINING, NX_REQUEST_SETTLED, NX_REQUEST_QUARANTINED};
    constexpr bool allowed[6][6] = {{false, false, false, false, false, false},
                                    {false, false, true, true, false, false},
                                    {false, false, false, true, false, true},
                                    {false, false, false, false, false, true},
                                    {false, false, false, false, false, false},
                                    {false, false, false, false, false, false}};
    for (size_t from = 0U; from < 6U; ++from) {
        for (size_t to = 0U; to < 6U; ++to) {
            SCOPED_TRACE(testing::Message() << from << " -> " << to);
            nx_request_t request{};
            reach(&request, states[from]);
            const nx_request_t before = request;
            EXPECT_EQ(nx_request_transition(&request, states[to]),
                      allowed[from][to] ? NX_SUCCESS : NX_ERROR_STATE);
            EXPECT_EQ(nx_request_state(&request),
                      allowed[from][to] ? states[to] : states[from]);
            EXPECT_EQ(request.result, before.result);
            EXPECT_EQ(request.transferred, before.transferred);
            EXPECT_EQ(request.deadline, before.deadline);
        }
    }
}

TEST(CoreLifecycle, LastUsableEpochDoesNotWrapOrRewriteRejectedOutputs) {
    nx_request_t request{};
    nx_request_initialize(&request);
    nx_request_slot_t slot{nullptr, UINT64_MAX - 1U};
    uint64_t epoch = 777U;
    ASSERT_EQ(nx_request_slot_bind(&slot, &request, &epoch), NX_SUCCESS);
    EXPECT_EQ(epoch, UINT64_MAX);
    EXPECT_EQ(nx_request_slot_lookup(&slot, UINT64_MAX - 1U), nullptr);
    ASSERT_EQ(nx_request_slot_release(&slot, epoch), NX_SUCCESS);
    epoch = 777U;
    EXPECT_EQ(nx_request_slot_bind(&slot, &request, &epoch),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(epoch, 777U);
    EXPECT_EQ(slot.epoch, UINT64_MAX);
    EXPECT_EQ(slot.request, nullptr);
}

TEST(CoreLifecycle, PrepareResetsOnlyAnUnborrowedConsumedIncarnation) {
    for (nx_request_state_t state :
         {NX_REQUEST_READY, NX_REQUEST_QUEUED, NX_REQUEST_ACTIVE,
          NX_REQUEST_DRAINING, NX_REQUEST_SETTLED, NX_REQUEST_QUARANTINED}) {
        SCOPED_TRACE(static_cast<int>(state));
        nx_request_t request{};
        reach(&request, state);
        const nx_request_t before = request;
        const bool unborrowed =
            state == NX_REQUEST_READY || state == NX_REQUEST_SETTLED;
        EXPECT_EQ(nx_request_prepare(&request, 200U),
                  unborrowed ? NX_SUCCESS : NX_ERROR_STATE);
        EXPECT_EQ(nx_request_state(&request),
                  unborrowed ? NX_REQUEST_READY : state);
        EXPECT_EQ(request.deadline, unborrowed ? 200U : before.deadline);
        EXPECT_EQ(request.result, unborrowed ? NX_SUCCESS : before.result);
        EXPECT_EQ(request.transferred, unborrowed ? 0U : before.transferred);
    }
}

TEST(CoreClock, AliasedObservationCannotReplaceTheTickAccumulator) {
    nx_clock32_t clock{};
    ASSERT_EQ(nx_clock32_initialize(&clock, 10U, 3U), NX_SUCCESS);
    const nx_clock32_t before = clock;
    EXPECT_EQ(nx_clock32_observe(&clock, 13U, &clock.ticks), NX_ERROR_INVALID);
    EXPECT_EQ(clock.ticks, before.ticks);
    EXPECT_EQ(clock.previous, before.previous);
    EXPECT_EQ(clock.frequency_hz, before.frequency_hz);
    EXPECT_EQ(clock.initialized, before.initialized);
}

TEST(CoreClock, FailedConversionDoesNotCommitTheCounterOrOutput) {
    nx_clock32_t clock{};
    ASSERT_EQ(nx_clock32_initialize(&clock, 10U, 1U), NX_SUCCESS);
    clock.ticks = UINT64_MAX / UINT64_C(1000000);
    const nx_clock32_t before = clock;
    nx_time_us_t output = 777U;
    EXPECT_EQ(nx_clock32_observe(&clock, 11U, &output), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(output, 777U);
    EXPECT_EQ(clock.ticks, before.ticks);
    EXPECT_EQ(clock.previous, before.previous);
    EXPECT_EQ(clock.frequency_hz, before.frequency_hz);
}

TEST(CoreClock, AdjacentOutputDoesNotAliasTheExtensionState) {
    struct Storage {
        nx_clock32_t clock;
        nx_time_us_t output;
    } storage{};
    ASSERT_EQ(nx_clock32_initialize(&storage.clock, 10U, 3U), NX_SUCCESS);
    ASSERT_EQ(nx_clock32_observe(&storage.clock, 13U, &storage.output),
              NX_SUCCESS);
    EXPECT_EQ(storage.output, 1000000U);
    EXPECT_EQ(storage.clock.ticks, 3U);
    EXPECT_EQ(storage.clock.previous, 13U);
}

TEST(CoreClock, OneFullWrapCannotBeReconstructedFromEqualSnapshots) {
    nx_clock32_t clock{};
    ASSERT_EQ(nx_clock32_initialize(&clock, 10U, 1U), NX_SUCCESS);
    nx_time_us_t output = 777U;
    ASSERT_EQ(nx_clock32_observe(&clock, 10U, &output), NX_SUCCESS);
    EXPECT_EQ(output, 0U);
    /* Equal snapshots cannot distinguish no progress from 2^32 ticks. The
     * owner's service interval is strictly shorter than a complete wrap. */
    ASSERT_EQ(nx_clock32_observe(&clock, 9U, &output), NX_SUCCESS);
    EXPECT_EQ(output, UINT64_C(4294967295000000));
}

TEST(CoreClock, FiniteDeadlineNeverTurnsABoundedWaitIntoInfiniteWait) {
    struct Boundary {
        nx_time_us_t now;
        uint64_t duration;
        nx_result_t status;
        nx_time_us_t deadline;
    };
    constexpr Boundary cases[] = {
        {0U, 0U, NX_SUCCESS, 0U},
        {10U, 5U, NX_SUCCESS, 15U},
        {UINT64_MAX - 1U, 0U, NX_SUCCESS, UINT64_MAX - 1U},
        {UINT64_MAX - 2U, 1U, NX_SUCCESS, UINT64_MAX - 1U},
        {UINT64_MAX - 1U, 1U, NX_ERROR_EXHAUSTED, 777U},
        {0U, UINT64_MAX, NX_ERROR_EXHAUSTED, 777U},
        {1U, UINT64_MAX, NX_ERROR_EXHAUSTED, 777U},
        {NX_DEADLINE_NEVER, 0U, NX_ERROR_INVALID, 777U}};
    for (const Boundary& boundary : cases) {
        SCOPED_TRACE(testing::Message()
                     << boundary.now << " + " << boundary.duration);
        nx_time_us_t output = 777U;
        EXPECT_EQ(
            nx_deadline_finite_after(boundary.now, boundary.duration, &output),
            boundary.status);
        EXPECT_EQ(output, boundary.deadline);
    }
    EXPECT_EQ(nx_deadline_finite_after(0U, 0U, nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_deadline_after(UINT64_MAX - 1U, 1U), NX_DEADLINE_NEVER);
}
} /* namespace */
