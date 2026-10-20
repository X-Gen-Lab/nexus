/**
 * \file            usb_control.c
 * \brief           Bounded EP0 packetization without class or product policy
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/usb_control.h"
#include "nexus/arch/arch.h"
#include <string.h>

/** \brief All mutable control is task-owned; IRQs only complete inner packets.
 */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Check numeric extent/alignment before typed reads, not memory
 * mapping. */
static bool metadata(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t start = (uintptr_t)pointer;
    return pointer != NULL && start % alignment == 0U &&
           bytes <= UINTPTR_MAX - start;
}

/** \brief Reject wrap and metadata aliases before retaining any new loan. */
static bool span(const void* p, size_t n) {
    return n == 0U || (p != NULL && n <= UINTPTR_MAX - (uintptr_t)p);
}
/** \brief Half-open interval comparison after overflow validation. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return an != 0U && bn != 0U && (uintptr_t)a < (uintptr_t)b + bn &&
           (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Public immutable metadata cannot be overwritten by output payloads.
 */
static bool profile_alias(const nx_usb_device_profile_t* p, const void* bytes,
                          size_t n) {
    if (overlap(bytes, n, p, sizeof(*p)) ||
        overlap(bytes, n, p->endpoints,
                p->endpoint_count * sizeof(*p->endpoints)) ||
        overlap(bytes, n, p->port, sizeof(*p->port)) ||
        overlap(bytes, n, p->port->ops, sizeof(*p->port->ops)) ||
        overlap(bytes, n, p->setup_decision.ops,
                sizeof(*p->setup_decision.ops))) {
        return true;
    }
    for (size_t i = 0U; i < p->endpoint_count; ++i) {
        const nx_usb_endpoint_port_t* port = p->endpoints[i].port;
        if (overlap(bytes, n, port, sizeof(*port)) ||
            overlap(bytes, n, port->ops, sizeof(*port->ops))) {
            return true;
        }
    }
    return false;
}
/** \brief User output may not corrupt the adapter or its still borrowed reply.
 */
static bool output(const nx_usb_control_t* c, void* bytes, size_t n) {
    if (!span(bytes, n) || overlap(bytes, n, c, sizeof(*c)) ||
        profile_alias(c->profile, bytes, n)) {
        return false;
    }
    const nx_usb_control_request_t* r = c->active;
    return r == NULL || (!overlap(bytes, n, r, sizeof(*r)) &&
                         !overlap(bytes, n, r->in, r->length) &&
                         !overlap(bytes, n, r->out, r->length));
}
/** \brief The profile is stable for the complete lifetime of this adapter. */
static const nx_usb_endpoint_port_t* endpoint(const nx_usb_control_t* c,
                                              unsigned lane) {
    return nx_usb_profile_endpoint(c->profile, 0U, lane != 0U)->port;
}
/** \brief Release only after all provider references have been acquire joined.
 */
static void settle(nx_usb_control_t* c, nx_result_t result) {
    nx_usb_control_request_t* r = c->active;
    size_t count = c->confirmed_bytes;
    c->completed_epoch = c->setup.epoch;
    c->active = NULL;
    c->cancel_pending = false;
    c->stall_pending = false;
    c->phase = NX_USB_CONTROL_IDLE;
    nx_request_settle(&r->base, result, count);
}
/** \brief First terminal cause wins until an actual STATUS wire ACK wins. */
static void abort_reply(nx_usb_control_t* c, nx_result_t reason) {
    if (c->active == NULL) {
        return;
    }
    if (!c->cancel_pending) {
        c->terminal = reason;
    }
    c->cancel_pending = true;
    c->stall_pending = false;
    if (c->phase != NX_USB_CONTROL_NEEDS_DEVICE_STOP) {
        c->phase = NX_USB_CONTROL_DRAINING;
    }
    if (nx_request_state(&c->active->base) == NX_REQUEST_ACTIVE) {
        (void)nx_request_transition(&c->active->base, NX_REQUEST_DRAINING);
    }
}
/** \brief Fold completion before newer SETUP/reset; never recycle a live
 * packet. */
static void fold(nx_usb_control_t* c) {
    for (unsigned lane = 0U; lane < 2U; ++lane) {
        if (!c->borrowed[lane]) {
            continue;
        }
        nx_usb_request_t* p = &c->packets[lane];
        nx_request_state_t state = nx_request_state(&p->base);
        if (state == NX_REQUEST_QUARANTINED) {
            abort_reply(c, NX_ERROR_IO);
            if (lane == 0U) {
                c->phase = NX_USB_CONTROL_NEEDS_DEVICE_STOP;
            }
            if (nx_request_state(&c->active->base) != NX_REQUEST_QUARANTINED) {
                (void)nx_request_transition(&c->active->base,
                                            NX_REQUEST_QUARANTINED);
            }
            continue;
        }
        if (state != NX_REQUEST_SETTLED) {
            continue;
        }
        nx_result_t result = p->base.result;
        size_t transferred = p->base.transferred;
        c->borrowed[lane] = false;
        c->cancel_sent[lane] = false;
        if (result != NX_SUCCESS) {
            abort_reply(c, result);
            continue;
        }
        if (transferred > p->length ||
            (lane == 1U && transferred != p->length)) {
            abort_reply(c, NX_ERROR_IO);
            continue;
        }
        if (p->stage == NX_USB_STATUS) {
            /* STATUS ACK is terminal even when a later SETUP is now visible.
             * It does not promise address/configuration policy committed. */
            settle(c, NX_SUCCESS);
            continue;
        }
        c->confirmed_bytes += transferred;
        if (c->cancel_pending) {
            continue;
        }
        if (lane == 1U) {
            if (p->length == 0U) {
                c->phase = NX_USB_CONTROL_STATUS_OUT;
            } else if (c->confirmed_bytes == c->goal) {
                bool zlp = c->goal < c->setup.length &&
                           c->goal % NX_USB_FS_MAX_PACKET == 0U;
                c->phase = zlp ? NX_USB_CONTROL_DATA_IN_ZLP
                               : NX_USB_CONTROL_STATUS_OUT;
            }
        } else if (transferred < p->length || c->confirmed_bytes == c->goal) {
            c->phase = NX_USB_CONTROL_DATA_READY;
        }
    }
}
/** \brief Preparation creates no adapter/provider references or payload access.
 */
static nx_result_t prepare(nx_usb_control_request_t* r, const uint8_t* in,
                           uint8_t* out, size_t n, uint64_t epoch,
                           nx_time_us_t deadline,
                           nx_usb_control_reply_kind_t kind) {
    const void* bytes = in == NULL ? (const void*)out : (const void*)in;
    if (!metadata(r, sizeof(*r), _Alignof(nx_usb_control_request_t)) ||
        epoch == 0U || !span(bytes, n) || overlap(bytes, n, r, sizeof(*r))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_request_prepare(&r->base, deadline);
    if (result == NX_SUCCESS) {
        r->in = in;
        r->out = out;
        r->length = n;
        r->setup_epoch = epoch;
        r->kind = kind;
    }
    return result;
}
nx_result_t nx_usb_control_in_prepare(nx_usb_control_request_t* r,
                                      const uint8_t* bytes, size_t n,
                                      uint64_t epoch, nx_time_us_t deadline) {
    return prepare(r, bytes, NULL, n, epoch, deadline, NX_USB_CONTROL_IN_REPLY);
}
nx_result_t nx_usb_control_out_prepare(nx_usb_control_request_t* r,
                                       uint8_t* bytes, size_t n, uint64_t epoch,
                                       nx_time_us_t deadline) {
    return prepare(r, NULL, bytes, n, epoch, deadline,
                   NX_USB_CONTROL_OUT_REPLY);
}
nx_result_t nx_usb_control_status_prepare(nx_usb_control_request_t* r,
                                          uint64_t epoch,
                                          nx_time_us_t deadline) {
    return prepare(r, NULL, NULL, 0U, epoch, deadline,
                   NX_USB_CONTROL_STATUS_REPLY);
}
nx_result_t nx_usb_control_initialize(nx_usb_control_t* c,
                                      const nx_usb_device_profile_t* p) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!metadata(c, sizeof(*c), _Alignof(nx_usb_control_t)) ||
        nx_usb_profile_validate(p) != NX_SUCCESS ||
        profile_alias(p, c, sizeof(*c))) {
        return NX_ERROR_INVALID;
    }
    if (c->profile != NULL) {
        return NX_ERROR_STATE;
    }
    memset(c, 0, sizeof(*c));
    c->profile = p;
    c->phase = NX_USB_CONTROL_IDLE;
    nx_request_initialize(&c->packets[0].base);
    nx_request_initialize(&c->packets[1].base);
    return NX_SUCCESS;
}
nx_result_t nx_usb_control_begin(nx_usb_control_t* c,
                                 nx_usb_control_request_t* r) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!metadata(c, sizeof(*c), _Alignof(nx_usb_control_t)) ||
        c->profile == NULL ||
        !metadata(r, sizeof(*r), _Alignof(nx_usb_control_request_t))) {
        return NX_ERROR_INVALID;
    }
    if (c->active != NULL || c->borrowed[0] || c->borrowed[1]) {
        return NX_ERROR_BUSY;
    }
    if (nx_request_state(&r->base) != NX_REQUEST_READY) {
        return NX_ERROR_STATE;
    }
    const void* bytes =
        r->in == NULL ? (const void*)r->out : (const void*)r->in;
    if (!span(bytes, r->length) || overlap(r, sizeof(*r), c, sizeof(*c)) ||
        overlap(bytes, r->length, c, sizeof(*c)) ||
        overlap(bytes, r->length, r, sizeof(*r)) ||
        profile_alias(c->profile, r, sizeof(*r)) ||
        profile_alias(c->profile, bytes, r->length)) {
        return NX_ERROR_INVALID;
    }
    nx_usb_device_state_t state;
    nx_result_t result = nx_usb_device_state(c->profile->port, &state);
    if (result != NX_SUCCESS) {
        return result;
    }
    bool input = (state.setup.request_type & 128U) != 0U;
    if (!state.started || state.closing || !state.control_pending ||
        state.epoch == 0U || state.setup.epoch != state.epoch ||
        r->setup_epoch != state.epoch || c->completed_epoch == state.epoch) {
        return NX_ERROR_STATE;
    }
    if ((state.setup.length == 0U &&
         (r->kind != NX_USB_CONTROL_STATUS_REPLY || r->length != 0U ||
          r->in != NULL || r->out != NULL)) ||
        (state.setup.length != 0U && input &&
         (r->kind != NX_USB_CONTROL_IN_REPLY || r->out != NULL)) ||
        (state.setup.length != 0U && !input &&
         (r->kind != NX_USB_CONTROL_OUT_REPLY || r->in != NULL ||
          r->length < state.setup.length))) {
        return NX_ERROR_INVALID;
    }
    if (nx_deadline_expired(r->base.deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    result = nx_request_admit(&r->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        return result;
    }
    c->active = r;
    c->latest = state;
    c->setup = state.setup;
    c->goal = input && r->length < state.setup.length ? r->length
                                                      : state.setup.length;
    c->confirmed_bytes = 0U;
    c->terminal = NX_SUCCESS;
    c->cancel_pending = false;
    c->stall_pending = false;
    c->phase = state.setup.length == 0U ? NX_USB_CONTROL_STATUS_IN
               : input                  ? NX_USB_CONTROL_DATA_IN
                                        : NX_USB_CONTROL_DATA_OUT;
    return NX_SUCCESS;
}
/** \brief One accepted cancel attempt; rejection never acts as source drain. */
static nx_result_t drain(nx_usb_control_t* c) {
    for (unsigned lane = 0U; lane < 2U; ++lane) {
        if (c->borrowed[lane] && !c->cancel_sent[lane]) {
            nx_result_t result =
                nx_usb_endpoint_cancel(endpoint(c, lane), &c->packets[lane]);
            if (result == NX_SUCCESS) {
                c->cancel_sent[lane] = true;
            }
            return result;
        }
    }
    if (!c->borrowed[0] && !c->borrowed[1]) {
        settle(c, c->terminal);
    }
    return NX_SUCCESS;
}
/** \brief Emit only the current packet; FIFO admission is not its completion.
 */
static nx_result_t launch(nx_usb_control_t* c) {
    bool input = c->phase == NX_USB_CONTROL_DATA_IN ||
                 c->phase == NX_USB_CONTROL_DATA_IN_ZLP ||
                 c->phase == NX_USB_CONTROL_STATUS_IN;
    bool status = c->phase == NX_USB_CONTROL_STATUS_IN ||
                  c->phase == NX_USB_CONTROL_STATUS_OUT;
    unsigned lane = input ? 1U : 0U;
    nx_usb_request_t* p = &c->packets[lane];
    size_t length = 0U;
    if (!status && c->phase != NX_USB_CONTROL_DATA_IN_ZLP) {
        length = c->goal - c->confirmed_bytes;
        if (length > NX_USB_FS_MAX_PACKET) {
            length = NX_USB_FS_MAX_PACKET;
        }
    }
    nx_usb_stage_t stage = status ? NX_USB_STATUS : NX_USB_DATA;
    const uint8_t* in =
        !input || length == 0U ? NULL : c->active->in + c->confirmed_bytes;
    uint8_t* out =
        input || length == 0U ? NULL : c->active->out + c->confirmed_bytes;
    nx_result_t result =
        input ? nx_usb_in_prepare(p, in, length, stage, c->setup.epoch,
                                  c->active->base.deadline)
              : nx_usb_out_prepare(p, out, length, stage, c->setup.epoch,
                                   c->active->base.deadline);
    if (result == NX_SUCCESS) {
        result = nx_usb_endpoint_submit(endpoint(c, lane), p);
    }
    if (result == NX_SUCCESS) {
        c->borrowed[lane] = true;
        c->cancel_sent[lane] = false;
    } else if (result != NX_ERROR_BUSY) {
        abort_reply(c, result);
        settle(c, c->terminal);
    }
    return result;
}
nx_result_t nx_usb_control_step(nx_usb_control_t* c) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!metadata(c, sizeof(*c), _Alignof(nx_usb_control_t)) ||
        c->profile == NULL) {
        return NX_ERROR_INVALID;
    }
    fold(c);
    nx_result_t progress = nx_usb_device_service(c->profile->port);
    nx_usb_device_state_t state;
    nx_result_t result = nx_usb_device_state(c->profile->port, &state);
    if (result == NX_SUCCESS) {
        c->latest = state;
    }
    /* Service may have just completed a packet. Its acquired status ACK must
     * be folded before invalidation, timeout or cancellation is considered. */
    fold(c);
    if (c->active != NULL) {
        if (progress != NX_SUCCESS || result != NX_SUCCESS ||
            !c->latest.started || c->latest.closing ||
            !c->latest.control_pending) {
            abort_reply(c, progress != NX_SUCCESS ? progress : NX_ERROR_STATE);
        } else if (c->latest.epoch != c->setup.epoch ||
                   c->latest.setup.epoch != c->setup.epoch) {
            abort_reply(c, NX_ERROR_CANCELLED);
        } else if (nx_deadline_expired(c->active->base.deadline,
                                       nx_time_now_us())) {
            abort_reply(c, NX_ERROR_TIMEOUT);
        }
        if (c->cancel_pending) {
            return drain(c);
        }
        if (c->stall_pending) {
            result = nx_usb_setup_reject(&c->profile->setup_decision,
                                         c->setup.epoch);
            if (result == NX_SUCCESS) {
                c->stall_pending = false;
                settle(c, NX_ERROR_PERMISSION);
                c->phase = NX_USB_CONTROL_STALLED;
            }
            return result;
        }
        if (!c->borrowed[0] && !c->borrowed[1] &&
            c->phase != NX_USB_CONTROL_DATA_READY) {
            return launch(c);
        }
    } else if (result == NX_SUCCESS) {
        c->phase = !state.started || state.closing ? NX_USB_CONTROL_CLOSED
                   : state.control_pending && state.epoch != c->completed_epoch
                       ? NX_USB_CONTROL_WAIT_REPLY
                       : NX_USB_CONTROL_IDLE;
    }
    return progress;
}
nx_result_t nx_usb_control_cancel(nx_usb_control_t* c,
                                  nx_usb_control_request_t* r, uint64_t epoch) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!metadata(c, sizeof(*c), _Alignof(nx_usb_control_t)) ||
        c->profile == NULL ||
        !metadata(r, sizeof(*r), _Alignof(nx_usb_control_request_t))) {
        return NX_ERROR_INVALID;
    }
    if (c->active != r || c->setup.epoch != epoch) {
        return NX_ERROR_STATE;
    }
    fold(c);
    if (c->active == NULL) {
        return NX_SUCCESS;
    }
    abort_reply(c, NX_ERROR_CANCELLED);
    return NX_SUCCESS;
}
nx_result_t nx_usb_control_out_view(const nx_usb_control_t* c,
                                    nx_usb_control_out_view_t* view) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!metadata(c, sizeof(*c), _Alignof(nx_usb_control_t)) ||
        c->profile == NULL ||
        !metadata(view, sizeof(*view), _Alignof(nx_usb_control_out_view_t)) ||
        !output(c, view, sizeof(*view))) {
        return NX_ERROR_INVALID;
    }
    if (c->active == NULL || c->phase != NX_USB_CONTROL_DATA_READY ||
        c->borrowed[0] || c->borrowed[1]) {
        return NX_ERROR_STATE;
    }
    *view = (nx_usb_control_out_view_t){c->active->out, c->confirmed_bytes,
                                        c->setup.epoch};
    return NX_SUCCESS;
}
nx_result_t nx_usb_control_approve(nx_usb_control_t* c, uint64_t epoch,
                                   bool accepted) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!metadata(c, sizeof(*c), _Alignof(nx_usb_control_t)) ||
        c->profile == NULL) {
        return NX_ERROR_INVALID;
    }
    if (c->active == NULL || c->phase != NX_USB_CONTROL_DATA_READY ||
        c->setup.epoch != epoch) {
        return NX_ERROR_STATE;
    }
    nx_usb_device_state_t state;
    nx_result_t result = nx_usb_device_state(c->profile->port, &state);
    if (result != NX_SUCCESS) {
        return result;
    }
    c->latest = state;
    if (!state.started || state.closing || !state.control_pending ||
        state.epoch != epoch || state.setup.epoch != epoch) {
        abort_reply(c, NX_ERROR_CANCELLED);
        return NX_ERROR_STATE;
    }
    c->stall_pending = !accepted;
    c->phase = NX_USB_CONTROL_STATUS_IN;
    return NX_SUCCESS;
}
nx_result_t nx_usb_control_snapshot(const nx_usb_control_t* c,
                                    nx_usb_control_snapshot_t* value) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!metadata(c, sizeof(*c), _Alignof(nx_usb_control_t)) ||
        c->profile == NULL ||
        !metadata(value, sizeof(*value), _Alignof(nx_usb_control_snapshot_t)) ||
        !output(c, value, sizeof(*value))) {
        return NX_ERROR_INVALID;
    }
    *value = (nx_usb_control_snapshot_t){c->latest, c->confirmed_bytes,
                                         c->phase, c->active != NULL};
    return NX_SUCCESS;
}
