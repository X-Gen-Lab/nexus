/**
 * \file            amp_ipc.c
 * \brief           Explicit-cache epoch-bound shared-memory loan lifecycle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/amp_ipc_storage.h"

_Static_assert(sizeof(nx_amp_word_t) == NX_AMP_IPC_UNIT, "word isolation");
_Static_assert(sizeof(nx_amp_record_t) == NX_AMP_IPC_UNIT, "record isolation");
_Static_assert(sizeof(nx_amp_wire_slot_t) == 4U * NX_AMP_IPC_UNIT,
               "slot isolation");

typedef struct {
    const void* address;
    size_t bytes;
} span_t;

/** \brief Check integer ranges before overlap arithmetic or pointer access. */
static bool valid_span(span_t span) {
    return span.address != NULL && span.bytes != 0U &&
           span.bytes <= UINTPTR_MAX - (uintptr_t)span.address;
}

static bool overlap(span_t a, span_t b) {
    return (uintptr_t)a.address < (uintptr_t)b.address + b.bytes &&
           (uintptr_t)b.address < (uintptr_t)a.address + a.bytes;
}

static bool aligned(const void* address, size_t alignment) {
    return address != NULL && (uintptr_t)address % alignment == 0U;
}

/** \brief Six independently owned storage spans; bounded by exact capacity. */
static void storage(const nx_amp_ipc_t* ipc, const nx_amp_config_t* c,
                    span_t spans[6]) {
    spans[0] = (span_t){ipc, sizeof(*ipc)};
    spans[1] = (span_t){c->slots, (size_t)c->capacity * sizeof(*c->slots)};
    spans[2] = (span_t){c->wire, (size_t)c->capacity * sizeof(*c->wire)};
    spans[3] = (span_t){c->payload, c->payload_bytes};
    spans[4] = (span_t){c->local_boot, sizeof(*c->local_boot)};
    spans[5] = (span_t){c->peer_boot, sizeof(*c->peer_boot)};
}

static bool live(const nx_amp_ipc_t* ipc) {
    return aligned(ipc, _Alignof(nx_amp_ipc_t)) && ipc->initialized &&
           !ipc->joined;
}

static bool output_valid(const nx_amp_ipc_t* ipc, void* out, size_t bytes,
                         size_t alignment) {
    span_t output = {out, bytes};
    span_t spans[6];
    if (!valid_span(output) || !aligned(out, alignment)) {
        return false;
    }
    storage(ipc, &ipc->config, spans);
    for (size_t i = 0U; i < 6U; ++i) {
        if (overlap(output, spans[i])) {
            return false;
        }
    }
    return !overlap(output,
                    (span_t){ipc->config.ops, sizeof(*ipc->config.ops)});
}

static bool config_valid(const nx_amp_ipc_t* ipc, const nx_amp_config_t* c) {
    if (!aligned(c, _Alignof(nx_amp_config_t)) ||
        !aligned(c->ops, _Alignof(nx_amp_port_ops_t)) ||
        !aligned(c->slots, _Alignof(nx_amp_local_slot_t)) ||
        !aligned(c->wire, _Alignof(nx_amp_wire_slot_t)) ||
        !aligned(c->local_boot, _Alignof(nx_amp_word_t)) ||
        !aligned(c->peer_boot, _Alignof(nx_amp_word_t)) ||
        c->ops->claim == NULL || c->ops->unclaim == NULL ||
        c->ops->clean == NULL || c->ops->invalidate == NULL ||
        c->ops->acquire == NULL || c->ops->release == NULL ||
        c->ops->stopped == NULL || c->capacity == 0U ||
        c->capacity > UINT32_MAX / sizeof(*c->wire) ||
        c->capacity > UINT32_MAX / sizeof(*c->slots) || c->local_epoch == 0U ||
        c->peer_epoch == 0U || c->granule == 0U ||
        c->granule > NX_AMP_IPC_UNIT ||
        (c->granule & (c->granule - 1U)) != 0U ||
        c->payload_bytes % c->granule != 0U ||
        (c->role != NX_AMP_SENDER && c->role != NX_AMP_RECEIVER)) {
        return false;
    }
    span_t spans[6];
    storage(ipc, c, spans);
    for (size_t i = 0U; i < 6U; ++i) {
        if (!valid_span(spans[i])) {
            return false;
        }
        if (i >= 2U && (uintptr_t)spans[i].address % c->granule != 0U) {
            return false;
        }
        if (overlap(spans[i], (span_t){c->ops, sizeof(*c->ops)}) ||
            overlap(spans[i], (span_t){c, sizeof(*c)})) {
            return false;
        }
        for (size_t j = 0U; j < i; ++j) {
            if (overlap(spans[i], spans[j])) {
                return false;
            }
        }
    }
    for (uint32_t i = 0U; i < c->capacity; ++i) {
        const nx_amp_local_slot_t* slot = &c->slots[i];
        if (slot->sequence != 0U || slot->active || slot->returned ||
            slot->offset != 0U || slot->length != 0U) {
            return false;
        }
    }
    return true;
}

nx_result_t nx_amp_ipc_construct(nx_amp_ipc_t* ipc,
                                 const nx_amp_config_t* config) {
    if (!aligned(ipc, _Alignof(nx_amp_ipc_t))) {
        return NX_ERROR_INVALID;
    }
    if (ipc->initialized) {
        return NX_ERROR_STATE;
    }
    if (!config_valid(ipc, config)) {
        return NX_ERROR_INVALID;
    }
    const nx_amp_port_ops_t* ops = config->ops;
    if (ops->acquire(config->context, &config->local_boot->value) !=
            config->local_epoch ||
        ops->acquire(config->context, &config->peer_boot->value) !=
            config->peer_epoch) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = ops->claim(config->context, ipc, config->wire,
                                    config->role, config->local_epoch);
    if (result == NX_SUCCESS) {
        *ipc = (nx_amp_ipc_t){.config = *config, .initialized = true};
    }
    return result;
}

/** \brief Once observed, a boot mismatch cannot be undone by stale memory. */
static nx_result_t epochs(nx_amp_ipc_t* ipc) {
    const nx_amp_config_t* c = &ipc->config;
    if (ipc->faulted) {
        return NX_ERROR_STATE;
    }
    uint32_t own = c->ops->acquire(c->context, &c->local_boot->value);
    uint32_t peer = c->ops->acquire(c->context, &c->peer_boot->value);
    if (own != c->local_epoch || peer != c->peer_epoch) {
        ipc->closed = true;
        ipc->faulted = true;
        return NX_ERROR_STATE;
    }
    return NX_SUCCESS;
}

static bool payload_valid(const nx_amp_config_t* c, uint32_t offset,
                          uint32_t bytes) {
    return bytes != 0U && offset <= c->payload_bytes &&
           bytes <= c->payload_bytes - offset && offset % c->granule == 0U &&
           bytes % c->granule == 0U;
}

static bool payload_busy(const nx_amp_config_t* c, uint32_t offset,
                         uint32_t bytes) {
    for (uint32_t i = 0U; i < c->capacity; ++i) {
        const nx_amp_local_slot_t* slot = &c->slots[i];
        if (slot->active && offset < slot->offset + slot->length &&
            slot->offset < offset + bytes) {
            return true;
        }
    }
    return false;
}

static nx_amp_ticket_t ticket_for(nx_amp_ipc_t* ipc, uint32_t index) {
    return (nx_amp_ticket_t){ipc, ipc->config.local_epoch,
                             ipc->config.peer_epoch, index,
                             ipc->config.slots[index].sequence};
}

/** \brief Compare stable identity before touching a retained payload span. */
static nx_amp_local_slot_t* lookup(nx_amp_ticket_t ticket, nx_amp_role_t role) {
    nx_amp_ipc_t* ipc = ticket.owner;
    if (!live(ipc) || ipc->config.role != role ||
        ticket.local_epoch != ipc->config.local_epoch ||
        ticket.peer_epoch != ipc->config.peer_epoch ||
        ticket.slot >= ipc->config.capacity || ticket.sequence == 0U) {
        return NULL;
    }
    nx_amp_local_slot_t* slot = &ipc->config.slots[ticket.slot];
    return slot->active && slot->sequence == ticket.sequence ? slot : NULL;
}

static void retire_loan(nx_amp_local_slot_t* slot) {
    slot->active = false;
    slot->returned = false;
    slot->offset = 0U;
    slot->length = 0U;
}

nx_result_t nx_amp_ipc_send(nx_amp_ipc_t* ipc, uint32_t offset, uint32_t bytes,
                            nx_amp_ticket_t* ticket) {
    if (!live(ipc)) {
        return NX_ERROR_STATE;
    }
    const nx_amp_config_t* c = &ipc->config;
    if (c->role != NX_AMP_SENDER ||
        !output_valid(ipc, ticket, sizeof(*ticket),
                      _Alignof(nx_amp_ticket_t)) ||
        !payload_valid(c, offset, bytes)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = epochs(ipc);
    if (result != NX_SUCCESS || ipc->closed) {
        return result != NX_SUCCESS ? result : NX_ERROR_BUSY;
    }
    if (payload_busy(c, offset, bytes)) {
        return NX_ERROR_BUSY;
    }
    uint32_t index = 0U;
    while (index < c->capacity &&
           (c->slots[index].active || c->slots[index].sequence == UINT32_MAX)) {
        ++index;
    }
    if (index == c->capacity) {
        return NX_ERROR_EXHAUSTED;
    }
    result = c->ops->clean(c->context, (uint8_t*)c->payload + offset, bytes);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_amp_local_slot_t* slot = &c->slots[index];
    nx_amp_wire_slot_t* wire = &c->wire[index];
    ++slot->sequence;
    wire->offer = (nx_amp_record_t){.sender_epoch = c->local_epoch,
                                    .receiver_epoch = c->peer_epoch,
                                    .sequence = slot->sequence,
                                    .offset = offset,
                                    .length = bytes};
    result = c->ops->clean(c->context, &wire->offer, sizeof(wire->offer));
    if (result != NX_SUCCESS) {
        /* An attempted descriptor identity is burned, never published/reused.
         */
        return result;
    }
    slot->offset = offset;
    slot->length = bytes;
    slot->active = true;
    slot->returned = false;
    *ticket = ticket_for(ipc, index);
    /* Publish only after local retention and all cross-core cleans complete. */
    c->ops->release(c->context, &wire->published.value, slot->sequence);
    return NX_SUCCESS;
}

nx_result_t nx_amp_ipc_poll(nx_amp_ticket_t ticket) {
    nx_amp_local_slot_t* slot = lookup(ticket, NX_AMP_SENDER);
    if (slot == NULL) {
        return NX_ERROR_STATE;
    }
    nx_amp_ipc_t* ipc = ticket.owner;
    const nx_amp_config_t* c = &ipc->config;
    nx_result_t result = epochs(ipc);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_amp_wire_slot_t* wire = &c->wire[ticket.slot];
    if (c->ops->acquire(c->context, &wire->acknowledged.value) !=
        ticket.sequence) {
        return NX_ERROR_BUSY;
    }
    result =
        c->ops->invalidate(c->context, &wire->receipt, sizeof(wire->receipt));
    if (result != NX_SUCCESS) {
        return result;
    }
    if (epochs(ipc) != NX_SUCCESS) {
        return NX_ERROR_STATE;
    }
    const nx_amp_record_t* ack = &wire->receipt;
    if (ack->sender_epoch != c->local_epoch ||
        ack->receiver_epoch != c->peer_epoch ||
        ack->sequence != ticket.sequence) {
        return NX_ERROR_BUSY;
    }
    retire_loan(slot);
    return NX_SUCCESS;
}

nx_result_t nx_amp_ipc_receive(nx_amp_ipc_t* ipc, nx_amp_view_t* view) {
    if (!live(ipc)) {
        return NX_ERROR_STATE;
    }
    const nx_amp_config_t* c = &ipc->config;
    if (c->role != NX_AMP_RECEIVER ||
        !output_valid(ipc, view, sizeof(*view), _Alignof(nx_amp_view_t))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = epochs(ipc);
    if (result != NX_SUCCESS || ipc->closed) {
        return result != NX_SUCCESS ? result : NX_ERROR_BUSY;
    }
    for (uint32_t index = 0U; index < c->capacity; ++index) {
        nx_amp_local_slot_t* slot = &c->slots[index];
        if (slot->active) {
            continue;
        }
        nx_amp_wire_slot_t* wire = &c->wire[index];
        uint32_t sequence = c->ops->acquire(c->context, &wire->published.value);
        if (sequence <= slot->sequence) {
            continue;
        }
        result =
            c->ops->invalidate(c->context, &wire->offer, sizeof(wire->offer));
        if (result != NX_SUCCESS) {
            return result;
        }
        nx_amp_record_t record = wire->offer;
        if (record.sender_epoch != c->peer_epoch ||
            record.receiver_epoch != c->local_epoch) {
            /* Older boot records are inert, including a high old sequence. */
            continue;
        }
        if (record.sequence != sequence) {
            return NX_ERROR_STATE;
        }
        if (!payload_valid(c, record.offset, record.length)) {
            return NX_ERROR_INVALID;
        }
        if (payload_busy(c, record.offset, record.length)) {
            return NX_ERROR_BUSY;
        }
        void* data = (uint8_t*)c->payload + record.offset;
        result = c->ops->invalidate(c->context, data, record.length);
        if (result != NX_SUCCESS) {
            return result;
        }
        if (epochs(ipc) != NX_SUCCESS) {
            return NX_ERROR_STATE;
        }
        *slot = (nx_amp_local_slot_t){.sequence = sequence,
                                      .offset = record.offset,
                                      .length = record.length,
                                      .active = true};
        *view = (nx_amp_view_t){ticket_for(ipc, index), data, record.length};
        return NX_SUCCESS;
    }
    return NX_ERROR_EMPTY;
}

nx_result_t nx_amp_ipc_finish(nx_amp_ticket_t ticket) {
    nx_amp_local_slot_t* slot = lookup(ticket, NX_AMP_RECEIVER);
    if (slot == NULL) {
        return NX_ERROR_STATE;
    }
    /* The caller joins all readers before entering, including failed ACKs. */
    slot->returned = true;
    nx_amp_ipc_t* ipc = ticket.owner;
    const nx_amp_config_t* c = &ipc->config;
    nx_result_t result = epochs(ipc);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_amp_wire_slot_t* wire = &c->wire[ticket.slot];
    wire->receipt = (nx_amp_record_t){.sender_epoch = c->peer_epoch,
                                      .receiver_epoch = c->local_epoch,
                                      .sequence = ticket.sequence};
    result = c->ops->clean(c->context, &wire->receipt, sizeof(wire->receipt));
    if (result != NX_SUCCESS) {
        return result;
    }
    if (epochs(ipc) != NX_SUCCESS) {
        return NX_ERROR_STATE;
    }
    c->ops->release(c->context, &wire->acknowledged.value, ticket.sequence);
    retire_loan(slot);
    return NX_SUCCESS;
}

nx_result_t nx_amp_ipc_close(nx_amp_ipc_t* ipc, nx_amp_close_t* control) {
    if (!live(ipc)) {
        return NX_ERROR_STATE;
    }
    if (!output_valid(ipc, control, sizeof(*control),
                      _Alignof(nx_amp_close_t))) {
        return NX_ERROR_INVALID;
    }
    ipc->closed = true;
    *control =
        (nx_amp_close_t){ipc, ipc->config.local_epoch, ipc->config.peer_epoch};
    return NX_SUCCESS;
}

static bool closed(nx_amp_close_t control) {
    return live(control.owner) && control.owner->closed &&
           control.local_epoch == control.owner->config.local_epoch &&
           control.peer_epoch == control.owner->config.peer_epoch;
}

/** \brief Unique-role release is the endpoint's final observable action. */
static nx_result_t unbind(nx_amp_ipc_t* ipc) {
    ipc->joined = true;
    ipc->config.ops->unclaim(ipc->config.context, ipc);
    return NX_SUCCESS;
}

nx_result_t nx_amp_ipc_join(nx_amp_close_t control) {
    if (!closed(control)) {
        return NX_ERROR_STATE;
    }
    nx_amp_ipc_t* ipc = control.owner;
    nx_result_t result = epochs(ipc);
    if (result != NX_SUCCESS) {
        return result;
    }
    for (uint32_t index = 0U; index < ipc->config.capacity; ++index) {
        if (!ipc->config.slots[index].active) {
            continue;
        }
        if (ipc->config.role == NX_AMP_RECEIVER) {
            return NX_ERROR_BUSY;
        }
        result = nx_amp_ipc_poll(ticket_for(ipc, index));
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    return unbind(ipc);
}

nx_result_t nx_amp_ipc_reset_join(nx_amp_close_t control) {
    if (!closed(control)) {
        return NX_ERROR_STATE;
    }
    nx_amp_ipc_t* ipc = control.owner;
    const nx_amp_config_t* c = &ipc->config;
    for (uint32_t index = 0U; index < c->capacity; ++index) {
        if (c->role == NX_AMP_RECEIVER && c->slots[index].active &&
            !c->slots[index].returned) {
            return NX_ERROR_BUSY;
        }
    }
    nx_result_t result = c->ops->stopped(c->context, c->peer_epoch);
    if (result != NX_SUCCESS) {
        return result;
    }
    for (uint32_t index = 0U; index < c->capacity; ++index) {
        retire_loan(&c->slots[index]);
    }
    return unbind(ipc);
}
