/**
 * \file            rtt.c
 * \brief           Whole-record adapter over the actual SEGGER RTT engine
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/rtt.h"
#include "nexus/arch/arch.h"
#include "nexus/arch/atomic.h"
#include <SEGGER_RTT.h>
#include <limits.h>

static uint32_t entering;
static uint32_t configured_channels;
static bool upstream_initialized;

/** \brief Exclude target calls without changing the incoming interrupt masks.
 */
static nx_result_t enter(void) {
    if (nx_arch_in_isr() || !nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t expected = 0U;
    return nx_atomic_u32_compare_exchange_acq_rel(&entering, &expected, 1U)
               ? NX_SUCCESS
               : NX_ERROR_BUSY;
}

/** \brief Publish exit after every upstream operation and statistic update. */
static void leave(void) {
    nx_atomic_u32_store_release(&entering, 0U);
}

/** \brief Reject aliases with rings whose bytes belong to the RTT protocol. */
static bool overlaps(const void* first, size_t bytes, const void* second,
                     size_t other_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return bytes > UINTPTR_MAX - a || other_bytes > UINTPTR_MAX - b ||
           (a < b + other_bytes && b < a + bytes);
}

/** \brief Upstream SKIP returns a boolean rather than a copied-byte count. */
static nx_result_t write_record(void* context, nx_log_level_t level,
                                const void* bytes, size_t length,
                                nx_time_us_t deadline) {
    (void)deadline;
    nx_rtt_t* rtt = context;
    if (rtt == NULL || (bytes == NULL && length != 0U) || length > UINT_MAX ||
        (unsigned)level > (unsigned)NX_LOG_ERROR) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = enter();
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!rtt->initialized || rtt->stopped) {
        result = NX_ERROR_STATE;
    } else if (length == 0U) {
        result = NX_SUCCESS;
    } else if (overlaps(bytes, length, rtt, sizeof(*rtt)) ||
               overlaps(bytes, length, rtt->up, rtt->up_bytes) ||
               overlaps(bytes, length, rtt->down, rtt->down_bytes)) {
        result = NX_ERROR_INVALID;
    } else if (SEGGER_RTT_WriteSkipNoLock(rtt->channel, bytes,
                                          (unsigned)length) == 1U) {
        if (rtt->accepted != UINT32_MAX) {
            ++rtt->accepted;
        }
        result = NX_SUCCESS;
    } else {
        if (rtt->dropped != UINT32_MAX) {
            ++rtt->dropped;
        }
        result = NX_ERROR_OVERFLOW;
    }
    leave();
    return result;
}

/** \brief Check consumption once without waiting for a missing debugger. */
static nx_result_t flush_record(void* context, nx_time_us_t deadline) {
    (void)deadline;
    nx_rtt_t* rtt = context;
    if (rtt == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = enter();
    if (result != NX_SUCCESS) {
        return result;
    }
    result =
        !rtt->initialized
            ? NX_ERROR_STATE
            : (SEGGER_RTT_GetBytesInBuffer(rtt->channel) == 0U ? NX_SUCCESS
                                                               : NX_ERROR_BUSY);
    leave();
    return result;
}

/** \brief Configure one image-lifetime channel without replacing another owner.
 */
nx_result_t nx_rtt_init(nx_rtt_t* rtt, unsigned channel, const char* name,
                        void* up, size_t up_bytes, void* down,
                        size_t down_bytes) {
    if (rtt == NULL || channel == 0U || channel >= 4U || name == NULL ||
        up == NULL || down == NULL || up_bytes < 2U || down_bytes < 2U ||
        up_bytes > UINT_MAX || down_bytes > UINT_MAX ||
        overlaps(up, up_bytes, down, down_bytes) ||
        overlaps(up, up_bytes, rtt, sizeof(*rtt)) ||
        overlaps(down, down_bytes, rtt, sizeof(*rtt))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = enter();
    if (result != NX_SUCCESS) {
        return result;
    }
    if (rtt->initialized) {
        leave();
        return NX_ERROR_STATE;
    }
    if ((configured_channels & (1U << channel)) != 0U) {
        leave();
        return NX_ERROR_BUSY;
    }
    if (!upstream_initialized) {
        SEGGER_RTT_Init();
        upstream_initialized = true;
    }
    int configured = SEGGER_RTT_ConfigUpBuffer(
        channel, name, up, (unsigned)up_bytes, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    configured |=
        SEGGER_RTT_ConfigDownBuffer(channel, name, down, (unsigned)down_bytes,
                                    SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    if (configured != 0) {
        leave();
        return NX_ERROR_IO;
    }
    *rtt = (nx_rtt_t){0};
    rtt->channel = channel;
    rtt->up = up;
    rtt->up_bytes = up_bytes;
    rtt->down = down;
    rtt->down_bytes = down_bytes;
    rtt->initialized = true;
    configured_channels |= 1U << channel;
    leave();
    return NX_SUCCESS;
}

/** \brief Reuse Log's sink interface without another forwarding port. */
nx_log_sink_port_t nx_rtt_log_sink(nx_rtt_t* rtt) {
    return rtt != NULL ? (nx_log_sink_port_t){rtt, write_record, flush_record}
                       : (nx_log_sink_port_t){0};
}

/** \brief Copy available downlink once under the same target exclusion. */
nx_result_t nx_rtt_read(nx_rtt_t* rtt, void* bytes, size_t capacity,
                        size_t* count) {
    if (rtt == NULL || bytes == NULL || capacity == 0U || capacity > UINT_MAX ||
        count == NULL || overlaps(bytes, capacity, count, sizeof(*count)) ||
        overlaps(rtt, sizeof(*rtt), count, sizeof(*count)) ||
        overlaps(rtt, sizeof(*rtt), bytes, capacity)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = enter();
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!rtt->initialized || rtt->stopped) {
        leave();
        return NX_ERROR_STATE;
    }
    if (overlaps(bytes, capacity, rtt->up, rtt->up_bytes) ||
        overlaps(bytes, capacity, rtt->down, rtt->down_bytes) ||
        overlaps(count, sizeof(*count), rtt->up, rtt->up_bytes) ||
        overlaps(count, sizeof(*count), rtt->down, rtt->down_bytes)) {
        leave();
        return NX_ERROR_INVALID;
    }
    unsigned received =
        SEGGER_RTT_ReadNoLock(rtt->channel, bytes, (unsigned)capacity);
    *count = received;
    rtt->received = received > UINT32_MAX - rtt->received
                        ? UINT32_MAX
                        : rtt->received + received;
    leave();
    return received != 0U ? NX_SUCCESS : NX_ERROR_EMPTY;
}

/** \brief Local closure does not assert remote debugger-memory quiescence. */
nx_result_t nx_rtt_stop(nx_rtt_t* rtt) {
    if (rtt == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = enter();
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!rtt->initialized) {
        leave();
        return NX_ERROR_STATE;
    }
    rtt->stopped = true;
    result = SEGGER_RTT_GetBytesInBuffer(rtt->channel) == 0U ? NX_SUCCESS
                                                             : NX_ERROR_BUSY;
    leave();
    return result;
}
