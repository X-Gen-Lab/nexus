/** Explicit bounded UART adapter. SPDX-License-Identifier: MIT */
#include "log/log_uart.h"
#include "osal/osal.h"
#include <limits.h>
#include <string.h>
static log_status_t translated(nx_status_t status) {
    if (status == NX_OK) return LOG_OK;
    if (status == NX_ERR_BUSY) return LOG_ERROR_BUSY;
    if (status == NX_ERR_TIMEOUT) return LOG_ERROR_TIMEOUT;
    if (status == NX_ERR_CONTEXT) return LOG_ERROR_ISR;
    return LOG_ERROR_BACKEND;
}
static bool claim(log_uart_backend_t* s) {
    osal_enter_critical();
    bool ok = !s->servicing;
    if (ok) s->servicing = true;
    osal_exit_critical();
    return ok;
}
static void release(log_uart_backend_t* s) {
    osal_enter_critical(); s->servicing = false; osal_exit_critical();
}
static log_status_t sink_init(void* ctx) {
    return ((log_uart_backend_t*)ctx)->opened ? LOG_OK : LOG_ERROR_NOT_INIT;
}
static log_status_t sink_write(void* ctx, const char* msg, size_t len) {
    log_uart_backend_t* s = ctx;
    if (!s || !msg || !len) return LOG_ERROR_INVALID_PARAM;
    if (osal_is_isr()) return LOG_ERROR_ISR;
    osal_enter_critical();
    log_status_t result = LOG_OK;
    if (!s->opened) result = LOG_ERROR_NOT_INIT;
    else if (s->closing || s->unknown_lease) result = LOG_ERROR_BUSY;
    else if (len > LOG_UART_MESSAGE_SIZE || s->count == LOG_UART_QUEUE_DEPTH) {
        ++s->stats.dropped; result = LOG_ERROR_FULL;
    } else {
        size_t tail = (s->head + s->count) % LOG_UART_QUEUE_DEPTH;
        memcpy(s->messages[tail], msg, len);
        s->lengths[tail] = len;
        ++s->count; ++s->stats.accepted;
    }
    osal_exit_critical();
    return result;
}
static log_status_t sink_flush(void* ctx) { return log_backend_uart_flush(ctx); }
static log_status_t sink_deinit(void* ctx) {
    /* Registration relinquishes the device only after its queued data drains.
     * Ownership check is performed by the public shutdown wrapper, not here
     * because the core deliberately still owns registration during callback. */
    log_uart_backend_t* s = ctx;
    log_status_t result = log_backend_uart_flush(s);
    if (result != LOG_OK) return result;
    if (!claim(s)) return LOG_ERROR_BUSY;
    nx_status_t status = nx_device_close(s->device);
    if (status == NX_OK) {
        osal_enter_critical(); s->opened = false; s->closing = false;
        memset(&s->device, 0, sizeof(s->device)); osal_exit_critical();
    }
    osal_enter_critical(); s->stats.last_status = status; osal_exit_critical();
    release(s);
    return translated(status);
}
log_status_t log_backend_uart_init(log_uart_backend_t* s, const char* name,
                                   uint32_t tx_budget, uint32_t flush_budget) {
    if (!s || !name || !tx_budget || !flush_budget || tx_budget > INT32_MAX || flush_budget > INT32_MAX)
        return LOG_ERROR_INVALID_PARAM;
    if (osal_is_isr()) return LOG_ERROR_ISR;
    if (s->opened || s->servicing) return LOG_ERROR_ALREADY_INIT;
    memset(s, 0, sizeof(*s));
    nx_status_t status = nx_device_open(name, NX_DEVICE_CLASS_UART, (uintptr_t)s, &s->device);
    if (status != NX_OK) return translated(status);
    s->opened = true;
    s->tx_timeout_ms = tx_budget; s->flush_timeout_ms = flush_budget;
    s->backend = (log_backend_t){.name = "uart", .ctx = s, .init = sink_init,
        .write = sink_write, .flush = sink_flush, .deinit = sink_deinit,
        .enabled = true, .min_level = LOG_LEVEL_TRACE};
    nx_device_caps_t caps;
    status = nx_device_query(s->device, &caps);
    if (status == NX_OK && !(caps.flags & NX_DEVICE_CAP_UART_OPERATIONS)) status = NX_ERR_NOT_SUPPORTED;
    if (status != NX_OK) {
        nx_status_t closed = nx_device_close(s->device);
        if (closed == NX_OK) s->opened = false;
        else s->closing = true; /* Caller retains cleanup entry even on init failure. */
        s->stats.last_status = status;
        return translated(status);
    }
    return LOG_OK;
}
log_status_t log_backend_uart_service(log_uart_backend_t* s) {
    if (!s) return LOG_ERROR_INVALID_PARAM;
    if (osal_is_isr()) return LOG_ERROR_ISR;
    if (!claim(s)) return LOG_ERROR_BUSY;
    if (!s->opened) { release(s); return LOG_ERROR_NOT_INIT; }
    if (s->unknown_lease) { release(s); return LOG_ERROR_BACKEND; }
    nx_status_t status = NX_OK;
    osal_enter_critical(); bool queued = s->count != 0; osal_exit_critical();
    if (!s->active && queued) {
        status = nx_device_uart_submit(s->device, s->messages[s->head], s->lengths[s->head],
                                       s->tx_timeout_ms, &s->ticket);
        if (status == NX_OK) { osal_enter_critical(); s->active = true; osal_exit_critical(); }
        else if (status == NX_ERR_INVALID_STATE) {
            /* A zero-ticket provider violation may have borrowed this slot. */
            osal_enter_critical(); s->unknown_lease = true; osal_exit_critical();
        } else if (status != NX_ERR_BUSY) {
            osal_enter_critical(); ++s->stats.failed;
            s->head = (s->head + 1u) % LOG_UART_QUEUE_DEPTH; --s->count; osal_exit_critical();
        }
    }
    if (status == NX_OK && s->active) {
        nx_uart_result_t result;
        status = nx_device_uart_poll(s->device, s->ticket, &result);
        if (status == NX_OK && result.settled && result.wire_idle) {
            status = result.status;
            osal_enter_critical();
            if (status == NX_OK) ++s->stats.completed; else ++s->stats.failed;
            s->active = false; s->ticket.sequence = 0;
            s->head = (s->head + 1u) % LOG_UART_QUEUE_DEPTH; --s->count;
            osal_exit_critical();
        }
    }
    osal_enter_critical(); s->stats.last_status = status; osal_exit_critical();
    release(s);
    return translated(status);
}
log_status_t log_backend_uart_get_stats(log_uart_backend_t* s, log_uart_stats_t* out) {
    if (!s || !out) return LOG_ERROR_INVALID_PARAM;
    if (osal_is_isr()) return LOG_ERROR_ISR;
    osal_enter_critical(); *out = s->stats; out->pending = s->count;
    out->buffer_leased = s->active || s->unknown_lease; osal_exit_critical();
    return LOG_OK;
}
log_status_t log_backend_uart_flush(log_uart_backend_t* s) {
    if (!s) return LOG_ERROR_INVALID_PARAM;
    if (osal_is_isr()) return LOG_ERROR_ISR;
    if (!s->opened) return LOG_ERROR_NOT_INIT;
    uint32_t start;
    if (osal_get_time_ms(&start) != OSAL_OK) return LOG_ERROR_BACKEND;
    for (;;) {
        log_uart_stats_t stats = {0};
        log_status_t stats_status = log_backend_uart_get_stats(s, &stats);
        if (stats_status != LOG_OK) return stats_status;
        if (!stats.pending) return stats.failed ? LOG_ERROR_BACKEND : LOG_OK;
        log_status_t result = log_backend_uart_service(s);
        if (result != LOG_OK && result != LOG_ERROR_BUSY) return result;
        stats_status = log_backend_uart_get_stats(s, &stats);
        if (stats_status != LOG_OK) return stats_status;
        if (!stats.pending) return stats.failed ? LOG_ERROR_BACKEND : LOG_OK;
        uint32_t now;
        if (osal_get_time_ms(&now) != OSAL_OK) return LOG_ERROR_BACKEND;
        if ((uint32_t)(now - start) >= s->flush_timeout_ms) return LOG_ERROR_TIMEOUT;
        if (osal_task_delay(1) != OSAL_OK) return LOG_ERROR_BUSY;
    }
}
log_status_t log_backend_uart_shutdown(log_uart_backend_t* s, bool discard) {
    if (!s) return LOG_ERROR_INVALID_PARAM;
    log_status_t ownership = log_backend_destroy_check(&s->backend);
    if (ownership != LOG_OK) return ownership;
    if (!s->opened) return LOG_OK;
    osal_enter_critical(); s->closing = true; osal_exit_critical();
    if (!discard) return sink_deinit(s);
    if (!claim(s)) return LOG_ERROR_BUSY;
    nx_status_t status = NX_OK;
    if (s->unknown_lease) {
        status = nx_device_uart_recover(s->device);
        if (status == NX_OK) { osal_enter_critical(); s->opened = false; osal_exit_critical(); }
    } else {
        if (s->active) {
            status = nx_device_uart_cancel(s->device, s->ticket);
            if (status == NX_OK) {
                osal_enter_critical(); s->active = false; s->ticket.sequence = 0; osal_exit_critical();
            }
        }
        if (status == NX_OK) status = nx_device_close(s->device);
        if (status == NX_OK) { osal_enter_critical(); s->opened = false; osal_exit_critical(); }
    }
    osal_enter_critical();
    s->stats.last_status = status;
    if (status == NX_OK) {
        s->stats.dropped += s->count; s->count = 0;
        s->active = s->unknown_lease = s->closing = false;
        memset(&s->device, 0, sizeof(s->device)); s->ticket.sequence = 0;
    }
    osal_exit_critical(); release(s);
    return translated(status);
}
