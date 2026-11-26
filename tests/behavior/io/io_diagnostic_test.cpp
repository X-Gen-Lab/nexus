/**
 * \file            io_diagnostic_test.cpp
 * \brief           Copied I/O facts explain retained work without recovery
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/diagnostic.h"
#include <gtest/gtest.h>

namespace {
TEST(IODiagnostic, MissingFactsRemainUnknownRatherThanHealthy) {
    nx_io_diagnostic_facts_t facts{};
    nx_io_diagnostic_t out{};
    ASSERT_EQ(nx_io_diagnose(&facts, &out), NX_SUCCESS);
    EXPECT_EQ(out.findings, 0U);
    EXPECT_EQ(out.unknown_facts, NX_IO_FACT_ALL);
}

TEST(IODiagnostic, DeadlineAndQuarantineNeverReleaseBorrowedStorage) {
    nx_request_t request{};
    nx_request_slot_t slot{};
    nx_request_observer_t observer{};
    nx_request_observation_t observation{};
    uint64_t epoch{};
    nx_request_initialize(&request);
    ASSERT_EQ(nx_request_prepare(&request, 10U), NX_SUCCESS);
    ASSERT_EQ(nx_request_slot_bind(&slot, &request, &epoch), NX_SUCCESS);
    ASSERT_EQ(nx_request_observer_bind(&observer, &slot, epoch), NX_SUCCESS);
    ASSERT_EQ(nx_request_admit(&request, NX_REQUEST_ACTIVE), NX_SUCCESS);
    ASSERT_EQ(nx_request_transition(&request, NX_REQUEST_QUARANTINED),
              NX_SUCCESS);
    ASSERT_EQ(nx_request_observer_sample(&observer, 10U, &observation),
              NX_SUCCESS);
    nx_io_diagnostic_facts_t facts{};
    facts.request = &observation;
    nx_io_diagnostic_t out{};
    ASSERT_EQ(nx_io_diagnose(&facts, &out), NX_SUCCESS);
    EXPECT_NE(out.findings & NX_IO_DIAG_QUARANTINED, 0U);
    EXPECT_NE(out.findings & NX_IO_DIAG_DEADLINE_REACHED, 0U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(slot.request, &request);
    EXPECT_EQ(out.unknown_facts & NX_IO_FACT_REQUEST, 0U);
}

TEST(IODiagnostic, SettledDoesNotClaimObserverJoinOrSuccessfulTransfer) {
    nx_request_observation_t request{};
    request.epoch = 1U;
    request.observed_at = 100U;
    request.deadline = 20U;
    request.state = NX_REQUEST_SETTLED;
    request.settled = true;
    request.result = NX_ERROR_CANCELLED;
    nx_io_diagnostic_facts_t facts{};
    facts.request = &request;
    facts.known = NX_IO_FACT_OBSERVERS;
    facts.observers_pending = true;
    nx_io_diagnostic_t out{};
    ASSERT_EQ(nx_io_diagnose(&facts, &out), NX_SUCCESS);
    EXPECT_NE(out.findings & NX_IO_DIAG_SETTLED, 0U);
    EXPECT_NE(out.findings & NX_IO_DIAG_OBSERVERS_PENDING, 0U);
    EXPECT_EQ(out.findings & NX_IO_DIAG_DEADLINE_REACHED, 0U);
}

TEST(IODiagnostic, NoStateChangeDoesNotInventAMissedService) {
    nx_request_observation_t request{};
    request.epoch = 2U;
    request.state = NX_REQUEST_ACTIVE;
    request.result = NX_ERROR_BUSY;
    request.deadline = NX_DEADLINE_NEVER;
    request.observed_at = 1000000U;
    nx_io_diagnostic_facts_t facts{};
    facts.request = &request;
    nx_io_diagnostic_t out{};
    ASSERT_EQ(nx_io_diagnose(&facts, &out), NX_SUCCESS);
    EXPECT_NE(out.findings & NX_IO_DIAG_ACTIVE, 0U);
    EXPECT_EQ(out.findings & NX_IO_DIAG_SERVICE_OVERDUE, 0U);
    EXPECT_EQ(out.findings & NX_IO_DIAG_DEADLINE_REACHED, 0U);
    EXPECT_NE(out.unknown_facts & NX_IO_FACT_SERVICE, 0U);
}

TEST(IODiagnostic, ServiceBudgetNeedsAnExplicitMonotonicObservation) {
    nx_io_diagnostic_facts_t facts{};
    facts.known = NX_IO_FACT_SERVICE;
    facts.last_service_at = 10U;
    facts.observed_at = 20U;
    facts.maximum_service_gap_us = 10U;
    nx_io_diagnostic_t out{};
    ASSERT_EQ(nx_io_diagnose(&facts, &out), NX_SUCCESS);
    EXPECT_EQ(out.findings & NX_IO_DIAG_SERVICE_OVERDUE, 0U);
    facts.observed_at = 21U;
    ASSERT_EQ(nx_io_diagnose(&facts, &out), NX_SUCCESS);
    EXPECT_NE(out.findings & NX_IO_DIAG_SERVICE_OVERDUE, 0U);
    out.findings = 777U;
    facts.observed_at = 9U;
    EXPECT_EQ(nx_io_diagnose(&facts, &out), NX_ERROR_STATE);
    EXPECT_EQ(out.findings, 777U);
}

TEST(IODiagnostic, ExplicitLifecycleAndOwnerFactsKeepTheirSeparateMeaning) {
    nx_io_diagnostic_facts_t facts{};
    facts.known = NX_IO_FACT_LIFECYCLE | NX_IO_FACT_OWNER;
    facts.stopping = true;
    facts.owner_queued = true;
    nx_io_diagnostic_t out{};
    ASSERT_EQ(nx_io_diagnose(&facts, &out), NX_SUCCESS);
    EXPECT_NE(out.findings & NX_IO_DIAG_NOT_STARTED, 0U);
    EXPECT_NE(out.findings & NX_IO_DIAG_STOPPING, 0U);
    EXPECT_NE(out.findings & NX_IO_DIAG_OWNER_CLOSED, 0U);
    EXPECT_NE(out.findings & NX_IO_DIAG_NO_FREE_SLOT, 0U);
    EXPECT_NE(out.findings & NX_IO_DIAG_OWNER_PENDING, 0U);
}

TEST(IODiagnostic, StreamAggregateDoesNotInventWhichUserHoldsTheBlock) {
    nx_stream_observation_t stream{};
    stream.capacity = 3U;
    stream.outstanding = 2U;
    stream.free_blocks = 1U;
    stream.losses = 1U;
    stream.stopping = true;
    nx_io_diagnostic_facts_t facts{};
    facts.stream = &stream;
    nx_io_diagnostic_t out{};
    ASSERT_EQ(nx_io_diagnose(&facts, &out), NX_SUCCESS);
    EXPECT_NE(out.findings & NX_IO_DIAG_STREAM_OUTSTANDING, 0U);
    EXPECT_NE(out.findings & NX_IO_DIAG_STREAM_CURSOR_BUSY, 0U);
    EXPECT_NE(out.findings & NX_IO_DIAG_STREAM_LOSS, 0U);
    EXPECT_NE(out.findings & NX_IO_DIAG_STOPPING, 0U);
    EXPECT_EQ(out.unknown_facts & NX_IO_FACT_STREAM, 0U);
}

TEST(IODiagnostic, InvalidSnapshotsAndOverlappingOutputLeaveFactsUntouched) {
    nx_io_diagnostic_facts_t facts{};
    nx_io_diagnostic_t out{777U, 888U};
    EXPECT_EQ(nx_io_diagnose(nullptr, &out), NX_ERROR_INVALID);
    EXPECT_EQ(nx_io_diagnose(&facts, nullptr), NX_ERROR_INVALID);
    facts.known = NX_IO_FACT_REQUEST;
    EXPECT_EQ(nx_io_diagnose(&facts, &out), NX_ERROR_INVALID);
    facts.known = 0U;
    EXPECT_EQ(
        nx_io_diagnose(&facts, reinterpret_cast<nx_io_diagnostic_t*>(&facts)),
        NX_ERROR_INVALID);
    nx_request_observation_t request{};
    request.epoch = 1U;
    request.state = NX_REQUEST_ACTIVE;
    request.settled = true;
    facts.request = &request;
    EXPECT_EQ(nx_io_diagnose(&facts, &out), NX_ERROR_INVALID);
    request.settled = false;
    EXPECT_EQ(
        nx_io_diagnose(&facts, reinterpret_cast<nx_io_diagnostic_t*>(&request)),
        NX_ERROR_INVALID);
    facts.request = nullptr;
    nx_stream_observation_t stream{};
    stream.capacity = 2U;
    stream.outstanding = 3U;
    facts.stream = &stream;
    EXPECT_EQ(nx_io_diagnose(&facts, &out), NX_ERROR_INVALID);
    EXPECT_EQ(out.findings, 777U);
    EXPECT_EQ(out.unknown_facts, 888U);
}

TEST(IODiagnostic, OneReasonNameNeverHidesOtherFindings) {
    EXPECT_STREQ(nx_io_diagnostic_name(NX_IO_DIAG_QUARANTINED),
                 "request-quarantined");
    EXPECT_STREQ(nx_io_diagnostic_name(NX_IO_DIAG_STREAM_CURSOR_BUSY),
                 "ordinary-stream-cursor-busy");
    EXPECT_EQ(nx_io_diagnostic_name(0U), nullptr);
    EXPECT_EQ(nx_io_diagnostic_name(NX_IO_DIAG_ACTIVE | NX_IO_DIAG_STOPPING),
              nullptr);
}

TEST(IODiagnostic, InfiniteSentinelIsNotAnObservedMonotonicTime) {
    nx_request_observation_t request{};
    request.epoch = 1U;
    request.state = NX_REQUEST_ACTIVE;
    request.deadline = 100U;
    request.observed_at = NX_DEADLINE_NEVER;
    nx_io_diagnostic_facts_t facts{};
    facts.request = &request;
    nx_io_diagnostic_t out{777U, 888U};
    EXPECT_EQ(nx_io_diagnose(&facts, &out), NX_ERROR_INVALID);
    EXPECT_EQ(out.findings, 777U);
    EXPECT_EQ(out.unknown_facts, 888U);
}
} /* namespace */
