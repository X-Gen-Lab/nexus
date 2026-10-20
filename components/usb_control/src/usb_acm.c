/**
 * \file            usb_acm.c
 * \brief           Explicit CDC ACM control intent over the EP0 packetizer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/usb_acm.h"
#include "nexus/arch/arch.h"
#include <string.h>

/** \brief Mutable class state has exactly the packetizer's task executor. */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Numeric checks precede typed access; valid storage remains external.
 */
static bool metadata(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t start = (uintptr_t)pointer;
    return pointer != NULL && start % alignment == 0U &&
           bytes <= UINTPTR_MAX - start;
}
/** \brief Compare validated nonwrapping half-open extents. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return an != 0U && bn != 0U && (uintptr_t)a < (uintptr_t)b + bn &&
           (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Caller writes cannot overwrite immutable public profile metadata. */
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
/** \brief Public entry storage is bound once and naturally aligned. */
static bool bound(const nx_usb_acm_t* a) {
    return metadata(a, sizeof(*a), _Alignof(nx_usb_acm_t)) &&
           a->control.profile != NULL;
}
/** \brief Value outputs never overwrite the context or stable ROM metadata. */
static bool output(const nx_usb_acm_t* a, void* bytes, size_t n,
                   size_t alignment) {
    return metadata(bytes, n, alignment) && !overlap(bytes, n, a, sizeof(*a)) &&
           !profile_alias(a->control.profile, bytes, n);
}
/** \brief Only the CDC wire-format range is shared; actual UART policy is not.
 */
static bool coding(const nx_usb_acm_line_coding_t* value) {
    uint8_t bits = value->data_bits;
    return value->rate != 0U && value->stop_bits <= 2U && value->parity <= 4U &&
           (bits == 5U || bits == 6U || bits == 7U || bits == 8U ||
            bits == 16U);
}
/** \brief No packed metadata casts: all seven payload bytes are byte-aligned.
 */
static void encode(uint8_t* bytes, const nx_usb_acm_line_coding_t* value) {
    bytes[0] = (uint8_t)value->rate;
    bytes[1] = (uint8_t)(value->rate >> 8U);
    bytes[2] = (uint8_t)(value->rate >> 16U);
    bytes[3] = (uint8_t)(value->rate >> 24U);
    bytes[4] = value->stop_bits;
    bytes[5] = value->parity;
    bytes[6] = value->data_bits;
}
/** \brief Decode only a complete immutable DATA_READY view. */
static nx_result_t pending_value(const nx_usb_acm_t* a,
                                 nx_usb_acm_pending_t* value) {
    if (!a->active || a->approved ||
        a->pending.change == NX_USB_ACM_GET_LINE_CODING) {
        return NX_ERROR_STATE;
    }
    *value = a->pending;
    if (value->change != NX_USB_ACM_SET_LINE_CODING) {
        return NX_SUCCESS;
    }
    nx_usb_control_out_view_t view;
    nx_result_t result = nx_usb_control_out_view(&a->control, &view);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (view.setup_epoch != a->pending.epoch || view.length != 7U) {
        return NX_ERROR_INVALID;
    }
    const uint8_t* b = view.bytes;
    nx_usb_acm_line_coding_t line = {(uint32_t)b[0] | (uint32_t)b[1] << 8U |
                                         (uint32_t)b[2] << 16U |
                                         (uint32_t)b[3] << 24U,
                                     b[4], b[5], b[6]};
    if (!coding(&line)) {
        return NX_ERROR_INVALID;
    }
    value->proposed.line_coding = line;
    return NX_SUCCESS;
}
/** \brief Clearing class admission is legal only before or after outer loan. */
static void finish(nx_usb_acm_t* a, nx_result_t result,
                   nx_result_t wire_result) {
    a->completed_epoch = a->pending.epoch;
    a->result = result;
    a->wire_result = wire_result;
    a->active = false;
    a->outer_started = false;
    a->approved = false;
}
/** \brief This is an observed software identity check, not a physical lock. */
static bool current(const nx_usb_device_state_t* state, uint64_t epoch) {
    return state->started && !state->closing && state->epoch == epoch &&
           state->setup.epoch == epoch;
}
/** \brief Strict class/interface fields: no clipping or silent bit masking. */
static nx_result_t classify(const nx_usb_setup_t* setup, uint8_t interface,
                            nx_usb_acm_change_t* change) {
    if (setup->index != interface) {
        return NX_ERROR_INVALID;
    }
    switch (setup->request) {
        case 0x20U:
            *change = NX_USB_ACM_SET_LINE_CODING;
            return setup->request_type == 0x21U && setup->value == 0U &&
                           setup->length == 7U
                       ? NX_SUCCESS
                       : NX_ERROR_INVALID;
        case 0x21U:
            *change = NX_USB_ACM_GET_LINE_CODING;
            return setup->request_type == 0xa1U && setup->value == 0U &&
                           setup->length == 7U
                       ? NX_SUCCESS
                       : NX_ERROR_INVALID;
        case 0x22U:
            *change = NX_USB_ACM_SET_CONTROL_LINE_STATE;
            return setup->request_type == 0x21U && setup->length == 0U &&
                           (setup->value & 0xfffcU) == 0U
                       ? NX_SUCCESS
                       : NX_ERROR_INVALID;
        case 0x23U:
            *change = NX_USB_ACM_SEND_BREAK;
            return setup->request_type == 0x21U && setup->length == 0U
                       ? NX_SUCCESS
                       : NX_ERROR_INVALID;
        default:
            return NX_ERROR_UNSUPPORTED;
    }
}
nx_result_t nx_usb_acm_initialize(nx_usb_acm_t* a,
                                  const nx_usb_device_profile_t* profile,
                                  uint8_t interface_number,
                                  const nx_usb_acm_state_t* initial) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!metadata(a, sizeof(*a), _Alignof(nx_usb_acm_t)) ||
        !metadata(initial, sizeof(*initial), _Alignof(nx_usb_acm_state_t)) ||
        nx_usb_profile_validate(profile) != NX_SUCCESS ||
        profile->setup_decision.ops == NULL ||
        profile->setup_decision.ops->reject == NULL ||
        overlap(a, sizeof(*a), initial, sizeof(*initial)) ||
        profile_alias(profile, a, sizeof(*a)) ||
        !coding(&initial->line_coding) ||
        (initial->control_lines & 0xfffcU) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (a->control.profile != NULL) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = nx_usb_control_initialize(&a->control, profile);
    if (result != NX_SUCCESS) {
        return result;
    }
    a->committed = *initial;
    a->interface_number = interface_number;
    a->result = NX_SUCCESS;
    a->wire_result = NX_ERROR_STATE;
    nx_request_initialize(&a->reply.base);
    return NX_SUCCESS;
}
nx_result_t nx_usb_acm_begin(nx_usb_acm_t* a, uint64_t epoch,
                             nx_time_us_t deadline) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!bound(a) || epoch == 0U) {
        return NX_ERROR_INVALID;
    }
    if (a->active) {
        return NX_ERROR_BUSY;
    }
    nx_usb_device_state_t state;
    nx_result_t result = nx_usb_device_state(a->control.profile->port, &state);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!current(&state, epoch) || !state.control_pending ||
        epoch <= a->last_epoch) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    nx_usb_acm_change_t change;
    result = classify(&state.setup, a->interface_number, &change);
    if (result != NX_SUCCESS) {
        nx_result_t rejected =
            nx_usb_setup_reject(&a->control.profile->setup_decision, epoch);
        if (rejected == NX_SUCCESS) {
            a->last_epoch = epoch;
        }
        return rejected == NX_SUCCESS ? result : rejected;
    }
    if (change == NX_USB_ACM_GET_LINE_CODING) {
        encode(a->wire, &a->committed.line_coding);
        result = nx_usb_control_in_prepare(&a->reply, a->wire, sizeof(a->wire),
                                           epoch, deadline);
    } else if (change == NX_USB_ACM_SET_LINE_CODING) {
        result = nx_usb_control_out_prepare(&a->reply, a->wire, sizeof(a->wire),
                                            epoch, deadline);
    }
    bool data = change == NX_USB_ACM_GET_LINE_CODING ||
                change == NX_USB_ACM_SET_LINE_CODING;
    if (result == NX_SUCCESS && data) {
        result = nx_usb_control_begin(&a->control, &a->reply);
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    a->pending = (nx_usb_acm_pending_t){a->committed, epoch, change};
    if (change == NX_USB_ACM_SET_CONTROL_LINE_STATE) {
        a->pending.proposed.control_lines = state.setup.value;
    } else if (change == NX_USB_ACM_SEND_BREAK) {
        a->pending.proposed.break_duration = state.setup.value;
    }
    a->deadline = deadline;
    a->last_epoch = epoch;
    a->active = true;
    a->outer_started = data;
    a->approved = false;
    return NX_SUCCESS;
}
nx_result_t nx_usb_acm_step(nx_usb_acm_t* a) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!bound(a)) {
        return NX_ERROR_INVALID;
    }
    if (!a->active) {
        return NX_SUCCESS;
    }
    nx_result_t result = nx_usb_control_step(&a->control);
    nx_usb_control_snapshot_t snapshot;
    nx_result_t copied = nx_usb_control_snapshot(&a->control, &snapshot);
    if (copied != NX_SUCCESS) {
        return copied;
    }
    bool same = current(&snapshot.device, a->pending.epoch);
    if (a->outer_started) {
        if (nx_request_state(&a->reply.base) == NX_REQUEST_SETTLED) {
            nx_result_t wire_result = a->reply.base.result;
            /* Outer SUCCESS is already an acquired, matching STATUS ACK.
             * A later SETUP/reset or failed state read cannot un-ACK it.
             * The packetizer aborts pre-ACK supersession and drains its loan.
             */
            if (wire_result == NX_SUCCESS && a->approved &&
                a->pending.change != NX_USB_ACM_GET_LINE_CODING) {
                a->committed = a->pending.proposed;
                a->committed_epoch = a->pending.epoch;
            }
            finish(a, wire_result, wire_result);
        }
    } else if (result != NX_SUCCESS || !same ||
               !snapshot.device.control_pending) {
        finish(a, result != NX_SUCCESS ? result : NX_ERROR_CANCELLED,
               NX_ERROR_STATE);
    } else if (nx_deadline_expired(a->deadline, nx_time_now_us())) {
        finish(a, NX_ERROR_TIMEOUT, NX_ERROR_STATE);
    }
    return result;
}
nx_result_t nx_usb_acm_pending(const nx_usb_acm_t* a,
                               nx_usb_acm_pending_t* pending) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!bound(a) ||
        !output(a, pending, sizeof(*pending), _Alignof(nx_usb_acm_pending_t))) {
        return NX_ERROR_INVALID;
    }
    nx_usb_acm_pending_t value;
    nx_result_t result = pending_value(a, &value);
    if (result == NX_SUCCESS) {
        *pending = value;
    }
    return result;
}
nx_result_t nx_usb_acm_approve(nx_usb_acm_t* a, uint64_t epoch, bool accepted) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!bound(a)) {
        return NX_ERROR_INVALID;
    }
    if (!a->active || a->pending.epoch != epoch || a->approved ||
        a->pending.change == NX_USB_ACM_GET_LINE_CODING) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(a->deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    if (a->pending.change == NX_USB_ACM_SET_LINE_CODING) {
        nx_usb_acm_pending_t value;
        if (accepted) {
            nx_result_t result = pending_value(a, &value);
            if (result != NX_SUCCESS) {
                return result;
            }
        }
        nx_result_t result =
            nx_usb_control_approve(&a->control, epoch, accepted);
        if (result == NX_SUCCESS && accepted) {
            a->pending = value;
            a->approved = true;
        }
        return result;
    }
    if (!accepted) {
        nx_result_t result =
            nx_usb_setup_reject(&a->control.profile->setup_decision, epoch);
        if (result == NX_SUCCESS) {
            finish(a, NX_ERROR_PERMISSION, NX_ERROR_STATE);
        }
        return result;
    }
    nx_result_t result =
        nx_usb_control_status_prepare(&a->reply, epoch, a->deadline);
    if (result == NX_SUCCESS) {
        result = nx_usb_control_begin(&a->control, &a->reply);
    }
    if (result == NX_SUCCESS) {
        a->outer_started = true;
        a->approved = true;
    }
    return result;
}
nx_result_t nx_usb_acm_cancel(nx_usb_acm_t* a, uint64_t epoch) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!bound(a)) {
        return NX_ERROR_INVALID;
    }
    if (!a->active || a->pending.epoch != epoch) {
        return NX_ERROR_STATE;
    }
    if (a->outer_started) {
        return nx_usb_control_cancel(&a->control, &a->reply, epoch);
    }
    finish(a, NX_ERROR_CANCELLED, NX_ERROR_STATE);
    return NX_SUCCESS;
}
nx_result_t nx_usb_acm_snapshot(const nx_usb_acm_t* a,
                                nx_usb_acm_snapshot_t* snapshot) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!bound(a) || !output(a, snapshot, sizeof(*snapshot),
                             _Alignof(nx_usb_acm_snapshot_t))) {
        return NX_ERROR_INVALID;
    }
    nx_usb_control_snapshot_t control;
    nx_result_t result = nx_usb_control_snapshot(&a->control, &control);
    if (result == NX_SUCCESS) {
        *snapshot = (nx_usb_acm_snapshot_t){
            a->committed,   a->committed_epoch, a->completed_epoch, a->result,
            a->wire_result, control.phase,      a->active};
    }
    return result;
}
