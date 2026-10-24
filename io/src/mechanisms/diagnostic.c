/**
 * \file            diagnostic.c
 * \brief           Bounded classification of explicit copied I/O facts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/diagnostic.h"

/** \brief Checked byte spans avoid ordering unrelated C pointers. */
static bool overlaps(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return first_bytes > UINTPTR_MAX - a || second_bytes > UINTPTR_MAX - b ||
           (a < b + second_bytes && b < a + first_bytes);
}

/** \brief Validate copied state before interpreting any ownership facts. */
static bool valid_request(const nx_request_observation_t* request) {
    return request->epoch != 0U && request->observed_at != NX_DEADLINE_NEVER &&
           (unsigned)request->state <= (unsigned)NX_REQUEST_QUARANTINED &&
           request->settled == (request->state == NX_REQUEST_SETTLED);
}

/**
 * \brief Classify only explicit copied facts without changing their owners.
 *
 * Validate each supplied group before publishing the local result. A rejected
 * group leaves the caller's output untouched, including when output aliases an
 * input snapshot. Missing groups remain unknown rather than becoming healthy
 * or stalled by default. Findings are independent observations, not an inferred
 * cause: a deadline never settles a borrow, and unchanged state does not prove
 * that service was omitted.
 */
nx_result_t nx_io_diagnose(const nx_io_diagnostic_facts_t* facts,
                           nx_io_diagnostic_t* out) {
    const uint32_t explicit_groups = NX_IO_FACT_LIFECYCLE | NX_IO_FACT_OWNER |
                                     NX_IO_FACT_SERVICE | NX_IO_FACT_OBSERVERS;
    if (facts == NULL || out == NULL ||
        (uintptr_t)facts % _Alignof(nx_io_diagnostic_facts_t) != 0U ||
        (uintptr_t)out % _Alignof(nx_io_diagnostic_t) != 0U ||
        overlaps(facts, sizeof(*facts), out, sizeof(*out)) ||
        (facts->known & ~explicit_groups) != 0U) {
        return NX_ERROR_INVALID;
    }
    /* Keep publication atomic with respect to validation, not concurrency. The
     * caller supplies stable copied snapshots and synchronizes its own output.
     */
    nx_io_diagnostic_t result = {0U, NX_IO_FACT_ALL & ~facts->known};
    if (facts->request != NULL) {
        const nx_request_observation_t* request = facts->request;
        if ((uintptr_t)request % _Alignof(nx_request_observation_t) != 0U ||
            overlaps(request, sizeof(*request), out, sizeof(*out)) ||
            !valid_request(request)) {
            return NX_ERROR_INVALID;
        }
        result.unknown_facts &= ~NX_IO_FACT_REQUEST;
        switch (request->state) {
            case NX_REQUEST_QUEUED:
                result.findings |= NX_IO_DIAG_QUEUED;
                break;
            case NX_REQUEST_ACTIVE:
                result.findings |= NX_IO_DIAG_ACTIVE;
                break;
            case NX_REQUEST_DRAINING:
                result.findings |= NX_IO_DIAG_DRAINING;
                break;
            case NX_REQUEST_QUARANTINED:
                result.findings |= NX_IO_DIAG_QUARANTINED;
                break;
            case NX_REQUEST_SETTLED:
                result.findings |= NX_IO_DIAG_SETTLED;
                break;
            default:
                break;
        }
        if (request->state != NX_REQUEST_READY && !request->settled &&
            nx_deadline_expired(request->deadline, request->observed_at)) {
            result.findings |= NX_IO_DIAG_DEADLINE_REACHED;
        }
    }
    if (facts->stream != NULL) {
        const nx_stream_observation_t* stream = facts->stream;
        if ((uintptr_t)stream % _Alignof(nx_stream_observation_t) != 0U ||
            overlaps(stream, sizeof(*stream), out, sizeof(*out)) ||
            stream->capacity == 0U || stream->outstanding > stream->capacity ||
            stream->free_blocks != stream->capacity - stream->outstanding ||
            (stream->next_free && stream->free_blocks == 0U)) {
            return NX_ERROR_INVALID;
        }
        result.unknown_facts &= ~NX_IO_FACT_STREAM;
        if (stream->outstanding != 0U) {
            result.findings |= NX_IO_DIAG_STREAM_OUTSTANDING;
        }
        if (!stream->next_free) {
            result.findings |= NX_IO_DIAG_STREAM_CURSOR_BUSY;
        }
        /* Loss is pending metadata in this snapshot, not a lifetime total. */
        if (stream->losses != 0U) {
            result.findings |= NX_IO_DIAG_STREAM_LOSS;
        }
        if (stream->stopping) {
            result.findings |= NX_IO_DIAG_STOPPING;
        }
    }
    if ((facts->known & NX_IO_FACT_LIFECYCLE) != 0U) {
        if (!facts->started) {
            result.findings |= NX_IO_DIAG_NOT_STARTED;
        }
        if (facts->stopping) {
            result.findings |= NX_IO_DIAG_STOPPING;
        }
    }
    if ((facts->known & NX_IO_FACT_OWNER) != 0U) {
        if (!facts->owner_accepting) {
            result.findings |= NX_IO_DIAG_OWNER_CLOSED;
        }
        if (!facts->owner_slot_available) {
            result.findings |= NX_IO_DIAG_NO_FREE_SLOT;
        }
        if (facts->owner_queued) {
            result.findings |= NX_IO_DIAG_OWNER_PENDING;
        }
    }
    if ((facts->known & NX_IO_FACT_OBSERVERS) != 0U &&
        facts->observers_pending) {
        result.findings |= NX_IO_DIAG_OBSERVERS_PENDING;
    }
    if ((facts->known & NX_IO_FACT_SERVICE) != 0U) {
        /* Only explicit same-domain service timestamps establish a missed
         * budget. The never sentinel is a deadline, not an observed clock. */
        if (facts->observed_at == NX_DEADLINE_NEVER ||
            facts->last_service_at == NX_DEADLINE_NEVER ||
            facts->maximum_service_gap_us == 0U ||
            facts->maximum_service_gap_us == NX_DEADLINE_NEVER) {
            return NX_ERROR_INVALID;
        }
        if (facts->observed_at < facts->last_service_at) {
            return NX_ERROR_STATE;
        }
        if (facts->observed_at - facts->last_service_at >
            facts->maximum_service_gap_us) {
            result.findings |= NX_IO_DIAG_SERVICE_OVERDUE;
        }
    }
    *out = result;
    return NX_SUCCESS;
}

/** \brief Separate optional reason strings from the allocation-free query. */
const char* nx_io_diagnostic_name(uint32_t finding) {
    switch (finding) {
        case NX_IO_DIAG_QUEUED:
            return "request-queued";
        case NX_IO_DIAG_ACTIVE:
            return "request-active";
        case NX_IO_DIAG_DRAINING:
            return "request-draining";
        case NX_IO_DIAG_QUARANTINED:
            return "request-quarantined";
        case NX_IO_DIAG_SETTLED:
            return "request-settled";
        case NX_IO_DIAG_DEADLINE_REACHED:
            return "request-deadline-reached";
        case NX_IO_DIAG_NOT_STARTED:
            return "device-not-started";
        case NX_IO_DIAG_STOPPING:
            return "stopping";
        case NX_IO_DIAG_OWNER_CLOSED:
            return "owner-admission-closed";
        case NX_IO_DIAG_NO_FREE_SLOT:
            return "owner-no-free-slot";
        case NX_IO_DIAG_OWNER_PENDING:
            return "owner-pending-work";
        case NX_IO_DIAG_SERVICE_OVERDUE:
            return "service-budget-exceeded";
        case NX_IO_DIAG_OBSERVERS_PENDING:
            return "observers-not-joined";
        case NX_IO_DIAG_STREAM_OUTSTANDING:
            return "stream-blocks-outstanding";
        case NX_IO_DIAG_STREAM_CURSOR_BUSY:
            return "ordinary-stream-cursor-busy";
        case NX_IO_DIAG_STREAM_LOSS:
            return "stream-loss-observed";
        default:
            return NULL;
    }
}
