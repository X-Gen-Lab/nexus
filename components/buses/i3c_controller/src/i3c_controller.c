/**
 * \file            i3c_controller.c
 * \brief           Bounded I3C target, request and copied-event ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/i3c_controller_storage.h"
#include <string.h>

/** \brief Prove a complete aligned object span before dereferencing it. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && bytes != 0 && address % alignment == 0 &&
           bytes <= UINTPTR_MAX - address;
}

/** \brief Compare only already representable spans. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}

/** \brief Check the stable controller identity before all instance access. */
static nx_result_t live(const nx_i3c_controller_t* c) {
    if (!span(c, sizeof(*c), _Alignof(nx_i3c_controller_t))) {
        return NX_ERROR_INVALID;
    }
    return c->initialized && !c->joined ? NX_SUCCESS : NX_ERROR_STATE;
}

/** \brief Protect immutable policy, hardware table and owned working storage.
 */
static bool separate(const nx_i3c_controller_t* c, const void* p, size_t n) {
    return !overlap(p, n, c, sizeof(*c)) &&
           !overlap(p, n, c->config.ops, sizeof(*c->config.ops)) &&
           !overlap(p, n, c->config.definitions,
                    c->config.target_count * sizeof(*c->config.definitions)) &&
           !overlap(p, n, c->config.targets,
                    c->config.target_count * sizeof(*c->config.targets)) &&
           !overlap(p, n, c->config.events,
                    c->config.event_capacity * sizeof(*c->config.events));
}

/** \brief Outputs may never overwrite an admitted loan or its metadata. */
static bool output(const nx_i3c_controller_t* c, const void* p, size_t n,
                   size_t alignment) {
    return span(p, n, alignment) && separate(c, p, n) &&
           (c->active == NULL ||
            (!overlap(p, n, c->active, sizeof(*c->active)) &&
             (c->active->bytes == 0 ||
              !overlap(p, n, c->active->data, c->active->bytes))));
}

/** \brief Use a conservative pure-I3C set excluding broadcast neighbours. */
static bool address_valid(uint8_t address) {
    return address >= 0x08 && address <= 0x77 && address != 0x3e &&
           address != 0x5e && address != 0x6e && address != 0x76;
}

/** \brief Reject unsupported CCC framing before any borrowed payload access. */
static nx_result_t shape(nx_i3c_kind_t kind, nx_i3c_endpoint_t endpoint,
                         uint8_t ccc, uint8_t* data, size_t bytes, bool read,
                         bool broadcast) {
    if (kind == NX_I3C_DISCOVER) {
        return endpoint.controller == NULL && endpoint.index == 0 &&
                       endpoint.epoch == 0 && ccc == 0 && data == NULL &&
                       bytes == 0 && !read && !broadcast
                   ? NX_SUCCESS
                   : NX_ERROR_INVALID;
    }
    if (kind == NX_I3C_PRIVATE) {
        return bytes > 0 && bytes <= UINT16_MAX && !broadcast && ccc == 0
                   ? NX_SUCCESS
                   : NX_ERROR_INVALID;
    }
    if (kind != NX_I3C_CCC) {
        return NX_ERROR_INVALID;
    }
    size_t expected = 0;
    bool direction = true;
    bool all = false;
    switch (ccc) {
        case 0x00:
        case 0x01:
            all = true;
            direction = false;
            expected = 1;
            break;
        case 0x80:
        case 0x81:
            direction = false;
            expected = 1;
            break;
        case 0x8b:
        case 0x90:
            expected = 2;
            break;
        case 0x8c:
            expected = bytes == 3 ? 3 : 2;
            break;
        case 0x8d:
            expected = 6;
            break;
        case 0x8e:
        case 0x8f:
            expected = 1;
            break;
        default:
            return NX_ERROR_UNSUPPORTED;
    }
    return bytes == expected && read == direction && broadcast == all
               ? NX_SUCCESS
               : NX_ERROR_INVALID;
}

/** \brief Return only an exact confirmed endpoint incarnation. */
static bool endpoint_valid(const nx_i3c_controller_t* c,
                           nx_i3c_endpoint_t endpoint) {
    return endpoint.controller == c &&
           endpoint.index < c->config.target_count && endpoint.epoch != 0 &&
           c->config.targets[endpoint.index].assigned &&
           !c->config.targets[endpoint.index].tentative &&
           c->config.targets[endpoint.index].epoch == endpoint.epoch;
}

nx_result_t
nx_i3c_controller_construct(nx_i3c_controller_t* c,
                            const nx_i3c_controller_config_t* config) {
    if (!span(c, sizeof(*c), _Alignof(nx_i3c_controller_t)) ||
        !span(config, sizeof(*config), _Alignof(nx_i3c_controller_config_t))) {
        return NX_ERROR_INVALID;
    }
    if (c->initialized) {
        return NX_ERROR_STATE;
    }
    if (config->target_count == 0 || config->event_capacity == 0 ||
        config->target_count > SIZE_MAX / sizeof(*config->definitions) ||
        config->target_count > SIZE_MAX / sizeof(*config->targets) ||
        config->event_capacity > SIZE_MAX / sizeof(*config->events) ||
        config->batch_bytes == 0 || config->batch_bytes > 8 ||
        config->clock.read == NULL ||
        !span(config->ops, sizeof(*config->ops), _Alignof(nx_i3c_port_ops_t)) ||
        !span(config->definitions,
              config->target_count * sizeof(*config->definitions),
              _Alignof(nx_i3c_target_definition_t)) ||
        !span(config->targets, config->target_count * sizeof(*config->targets),
              _Alignof(nx_i3c_target_state_t)) ||
        !span(config->events, config->event_capacity * sizeof(*config->events),
              _Alignof(nx_i3c_event_t))) {
        return NX_ERROR_INVALID;
    }
    const void* ranges[] = {c,
                            config,
                            config->ops,
                            config->definitions,
                            config->targets,
                            config->events};
    const size_t sizes[] = {sizeof(*c),
                            sizeof(*config),
                            sizeof(*config->ops),
                            config->target_count * sizeof(*config->definitions),
                            config->target_count * sizeof(*config->targets),
                            config->event_capacity * sizeof(*config->events)};
    for (size_t i = 0; i < 6; ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (overlap(ranges[i], sizes[i], ranges[j], sizes[j])) {
                return NX_ERROR_INVALID;
            }
        }
    }
    const nx_i3c_port_ops_t* ops = config->ops;
    if (ops->claim == NULL || ops->release == NULL || ops->ready == NULL ||
        ops->begin == NULL || ops->pump == NULL || ops->assign == NULL ||
        ops->withdraw == NULL || ops->quiescent == NULL || ops->reset == NULL) {
        return NX_ERROR_INVALID;
    }
    for (size_t i = 0; i < config->target_count; ++i) {
        const nx_i3c_target_definition_t* d = &config->definitions[i];
        const nx_i3c_target_state_t* t = &config->targets[i];
        if (d->pid > UINT64_C(0xffffffffffff) || !address_valid(d->address) ||
            t->epoch != 0 || t->address != 0 || t->bcr != 0 || t->dcr != 0 ||
            t->assigned || t->tentative) {
            return NX_ERROR_INVALID;
        }
        for (size_t j = 0; j < i; ++j) {
            if (config->definitions[j].pid == d->pid ||
                config->definitions[j].address == d->address) {
                return NX_ERROR_INVALID;
            }
        }
    }
    nx_result_t result = ops->claim(config->context, c);
    if (result != NX_SUCCESS) {
        return result;
    }
    *c = (nx_i3c_controller_t){
        .config = *config, .pending_target = SIZE_MAX, .initialized = true};
    return NX_SUCCESS;
}

nx_result_t nx_i3c_request_prepare(nx_i3c_request_t* r, nx_i3c_kind_t kind,
                                   nx_i3c_endpoint_t endpoint, uint8_t ccc,
                                   uint8_t* data, size_t bytes, bool read,
                                   bool broadcast, nx_time_us_t deadline) {
    if (!span(r, sizeof(*r), _Alignof(nx_i3c_request_t)) ||
        (bytes != 0 &&
         (!span(data, bytes, 1) || overlap(data, bytes, r, sizeof(*r))))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result =
        shape(kind, endpoint, ccc, data, bytes, read, broadcast);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_request_prepare(&r->base, deadline);
    if (result == NX_SUCCESS) {
        r->kind = kind;
        r->endpoint = endpoint;
        r->ccc = ccc;
        r->data = data;
        r->bytes = bytes;
        r->read = read;
        r->broadcast = broadcast;
    }
    return result;
}

nx_result_t nx_i3c_submit(nx_i3c_controller_t* c, nx_i3c_request_t* r,
                          nx_i3c_ticket_t* ticket) {
    nx_result_t result = live(c);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (c->closed || c->faulted) {
        return NX_ERROR_STATE;
    }
    if (c->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (!span(r, sizeof(*r), _Alignof(nx_i3c_request_t)) ||
        !separate(c, r, sizeof(*r))) {
        return NX_ERROR_INVALID;
    }
    result = shape(r->kind, r->endpoint, r->ccc, r->data, r->bytes, r->read,
                   r->broadcast);
    if (result != NX_SUCCESS) {
        return result;
    }
    if ((r->bytes != 0 &&
         (!span(r->data, r->bytes, 1) || !separate(c, r->data, r->bytes) ||
          overlap(r, sizeof(*r), r->data, r->bytes))) ||
        !output(c, ticket, sizeof(*ticket), _Alignof(nx_i3c_ticket_t)) ||
        overlap(ticket, sizeof(*ticket), r, sizeof(*r)) ||
        (r->bytes != 0 &&
         overlap(ticket, sizeof(*ticket), r->data, r->bytes))) {
        return NX_ERROR_INVALID;
    }
    if (nx_request_state(&r->base) != NX_REQUEST_READY) {
        return NX_ERROR_STATE;
    }
    bool direct =
        r->kind == NX_I3C_PRIVATE || (r->kind == NX_I3C_CCC && !r->broadcast);
    if (direct && !endpoint_valid(c, r->endpoint)) {
        return NX_ERROR_STATE;
    }
    if (r->kind == NX_I3C_CCC && r->ccc == 0x8c &&
        r->bytes !=
            ((c->config.targets[r->endpoint.index].bcr & 4U) ? 3U : 2U)) {
        return NX_ERROR_INVALID;
    }
    if (c->slot.epoch == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    if (r->base.deadline != NX_DEADLINE_NEVER &&
        c->config.clock.read(c->config.clock.context) >= r->base.deadline) {
        return NX_ERROR_TIMEOUT;
    }
    result = c->config.ops->ready(c->config.context);
    if (result != NX_SUCCESS) {
        return result;
    }
    uint64_t epoch = 0;
    result = nx_request_slot_bind(&c->slot, &r->base, &epoch);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_request_admit(&r->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        c->slot.request = NULL;
        return result;
    }
    c->active = r;
    c->progress = 0;
    c->pending_target = SIZE_MAX;
    c->result = NX_SUCCESS;
    c->draining = false;
    *ticket = (nx_i3c_ticket_t){c, epoch};
    nx_i3c_command_t command = {
        .kind = r->kind,
        .bytes = (uint32_t)r->bytes,
        .address = direct ? c->config.targets[r->endpoint.index].address : 0,
        .ccc = r->ccc,
        .read = r->read,
        .broadcast = r->broadcast};
    c->config.ops->begin(c->config.context, &command);
    return NX_SUCCESS;
}

/** \brief Detach every core reference before final request publication. */
static void settle(nx_i3c_controller_t* c) {
    nx_request_t* request = &c->active->base;
    nx_result_t result = c->result;
    size_t progress = c->progress;
    c->active = NULL;
    c->slot.request = NULL;
    c->draining = false;
    c->pending_target = SIZE_MAX;
    nx_request_settle(request, result, progress);
}

/** \brief Withdraw once; discovery failure never discards assigned addresses.
 */
static void finish(nx_i3c_controller_t* c, nx_result_t result, bool abort) {
    if (c->draining) {
        return;
    }
    c->result = result;
    c->draining = true;
    if (result != NX_SUCCESS && c->active->kind == NX_I3C_DISCOVER) {
        c->faulted = true;
    }
    (void)nx_request_transition(&c->active->base, NX_REQUEST_DRAINING);
    if (abort) {
        c->config.ops->withdraw(c->config.context);
    }
}

/** \brief Loss counters never turn a lost event into an apparent reset. */
static void increment(uint32_t* value) {
    if (*value != UINT32_MAX) {
        ++*value;
    }
}

/** \brief Reject only the incoming event when the explicit ring is full. */
static nx_result_t enqueue(nx_i3c_controller_t* c, nx_i3c_event_t event) {
    if (c->event_count == c->config.event_capacity) {
        increment(&c->event_status.overflow);
        return NX_ERROR_OVERFLOW;
    }
    size_t index = c->event_head;
    size_t tail = c->config.event_capacity - index;
    index =
        c->event_count < tail ? index + c->event_count : c->event_count - tail;
    c->config.events[index] = event;
    ++c->event_count;
    return NX_SUCCESS;
}

/** \brief Copy bounded unsolicited events without running discovery. */
static nx_result_t events(nx_i3c_controller_t* c,
                          const nx_i3c_observation_t* observation) {
    nx_result_t result = NX_SUCCESS;
    if (observation->ibi) {
        size_t i = 0;
        while (i < c->config.target_count &&
               (!c->config.targets[i].assigned ||
                c->config.targets[i].tentative ||
                c->config.targets[i].address != observation->ibi_address)) {
            ++i;
        }
        bool payload =
            i < c->config.target_count && (c->config.targets[i].bcr & 4U) != 0;
        if (i == c->config.target_count ||
            (c->config.targets[i].bcr & 2U) == 0 ||
            observation->ibi_bytes > 4 ||
            (payload ? observation->ibi_bytes == 0
                     : observation->ibi_bytes != 0)) {
            increment(&c->event_status.rejected_ibi);
        } else {
            nx_i3c_event_t event = {
                .kind = NX_I3C_EVENT_IBI,
                .endpoint = {c, i, c->config.targets[i].epoch},
                .bytes = observation->ibi_bytes};
            memcpy(event.payload, observation->ibi_payload, event.bytes);
            result = enqueue(c, event);
        }
    }
    if (observation->hot_join) {
        nx_i3c_event_t event = {.kind = NX_I3C_EVENT_HOT_JOIN};
        nx_result_t queued = enqueue(c, event);
        if (queued != NX_SUCCESS) {
            result = queued;
        }
    }
    return result;
}

/** \brief Acknowledge the preceding assignment before consuming a new PID. */
static nx_result_t discovery(nx_i3c_controller_t* c,
                             const nx_i3c_observation_t* observation) {
    if (observation->assigned) {
        if (c->pending_target == SIZE_MAX) {
            return NX_ERROR_IO;
        }
        nx_i3c_target_state_t* t = &c->config.targets[c->pending_target];
        t->tentative = false;
        t->assigned = true;
        c->pending_target = SIZE_MAX;
    }
    if (observation->error != NX_SUCCESS) {
        return observation->error;
    }
    if (observation->identity) {
        if (c->pending_target != SIZE_MAX) {
            return NX_ERROR_IO;
        }
        size_t i = 0;
        while (i < c->config.target_count &&
               c->config.definitions[i].pid != observation->pid) {
            ++i;
        }
        if (i == c->config.target_count) {
            return NX_ERROR_NACK;
        }
        nx_i3c_target_state_t* t = &c->config.targets[i];
        if (t->assigned || t->tentative) {
            return NX_ERROR_IO;
        }
        if (t->epoch == UINT64_MAX) {
            return NX_ERROR_EXHAUSTED;
        }
        ++t->epoch;
        t->address = c->config.definitions[i].address;
        t->bcr = observation->bcr;
        t->dcr = observation->dcr;
        t->tentative = true;
        c->pending_target = i;
        c->config.ops->assign(c->config.context, t->address);
    }
    return observation->complete && c->pending_target != SIZE_MAX ? NX_ERROR_IO
                                                                  : NX_SUCCESS;
}

nx_result_t nx_i3c_service(nx_i3c_controller_t* c) {
    nx_result_t result = live(c);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (c->draining) {
        result = c->config.ops->quiescent(c->config.context);
        if (result == NX_SUCCESS) {
            settle(c);
        } else if (nx_request_state(&c->active->base) !=
                   NX_REQUEST_QUARANTINED) {
            (void)nx_request_transition(&c->active->base,
                                        NX_REQUEST_QUARANTINED);
        }
        return result;
    }
    nx_i3c_request_t* r = c->active;
    if (r != NULL && r->base.deadline != NX_DEADLINE_NEVER &&
        c->config.clock.read(c->config.clock.context) >= r->base.deadline) {
        finish(c, NX_ERROR_TIMEOUT, true);
        return NX_SUCCESS;
    }
    size_t capacity = r == NULL ? 0 : r->bytes - c->progress;
    if (capacity > c->config.batch_bytes) {
        capacity = c->config.batch_bytes;
    }
    const uint8_t* tx =
        r != NULL && !r->read && capacity != 0 ? r->data + c->progress : NULL;
    uint8_t* rx =
        r != NULL && r->read && capacity != 0 ? r->data + c->progress : NULL;
    nx_i3c_observation_t o = {0};
    c->config.ops->pump(c->config.context, tx, tx == NULL ? 0 : capacity, rx,
                        rx == NULL ? 0 : capacity, &o);
    if (o.transmitted > (tx == NULL ? 0 : capacity) ||
        o.received > (rx == NULL ? 0 : capacity) ||
        (r == NULL && (o.identity || o.assigned || o.complete)) ||
        (r != NULL && r->kind != NX_I3C_DISCOVER &&
         (o.identity || o.assigned))) {
        result = NX_ERROR_IO;
    } else {
        if (r != NULL) {
            c->progress += r->read ? o.received : o.transmitted;
        }
        result = r != NULL && r->kind == NX_I3C_DISCOVER ? discovery(c, &o)
                                                         : NX_SUCCESS;
        if (result == NX_SUCCESS && o.error != NX_SUCCESS) {
            result = o.error;
        }
        if (result == NX_SUCCESS && r != NULL && o.complete &&
            c->progress != r->bytes) {
            result = NX_ERROR_IO;
        }
    }
    nx_result_t event_result = events(c, &o);
    if (result != NX_SUCCESS) {
        c->faulted = true;
        if (r != NULL) {
            finish(c, result, true);
        } else {
            c->config.ops->withdraw(c->config.context);
        }
        return result;
    }
    if (r != NULL && o.complete) {
        finish(c, NX_SUCCESS, false);
    }
    return event_result;
}

nx_result_t nx_i3c_cancel(nx_i3c_ticket_t ticket) {
    nx_result_t result = live(ticket.controller);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_i3c_controller_t* c = ticket.controller;
    if (nx_request_slot_lookup(&c->slot, ticket.epoch) == NULL) {
        return NX_ERROR_STATE;
    }
    finish(c, NX_ERROR_CANCELLED, true);
    return NX_SUCCESS;
}

nx_result_t nx_i3c_endpoint(nx_i3c_controller_t* c, size_t index,
                            nx_i3c_endpoint_t* endpoint) {
    nx_result_t result = live(c);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!output(c, endpoint, sizeof(*endpoint), _Alignof(nx_i3c_endpoint_t)) ||
        index >= c->config.target_count) {
        return NX_ERROR_INVALID;
    }
    nx_i3c_endpoint_t value = {c, index, c->config.targets[index].epoch};
    if (!endpoint_valid(c, value)) {
        return NX_ERROR_STATE;
    }
    *endpoint = value;
    return NX_SUCCESS;
}

nx_result_t nx_i3c_event_take(nx_i3c_controller_t* c, nx_i3c_event_t* event) {
    nx_result_t result = live(c);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!output(c, event, sizeof(*event), _Alignof(nx_i3c_event_t))) {
        return NX_ERROR_INVALID;
    }
    if (c->event_count == 0) {
        return NX_ERROR_EMPTY;
    }
    *event = c->config.events[c->event_head];
    ++c->event_head;
    if (c->event_head == c->config.event_capacity) {
        c->event_head = 0;
    }
    --c->event_count;
    return NX_SUCCESS;
}

nx_result_t nx_i3c_event_status(nx_i3c_controller_t* c,
                                nx_i3c_event_status_t* status) {
    nx_result_t result = live(c);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!output(c, status, sizeof(*status), _Alignof(nx_i3c_event_status_t))) {
        return NX_ERROR_INVALID;
    }
    *status = c->event_status;
    return NX_SUCCESS;
}

nx_result_t nx_i3c_close(nx_i3c_controller_t* c) {
    nx_result_t result = live(c);
    if (result == NX_SUCCESS) {
        c->closed = true;
    }
    return result;
}

nx_result_t nx_i3c_reset(nx_i3c_controller_t* c) {
    nx_result_t result = live(c);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!c->closed) {
        return NX_ERROR_STATE;
    }
    result = c->config.ops->reset(c->config.context);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (c->active != NULL) {
        if (!c->draining) {
            c->result = NX_ERROR_CANCELLED;
        }
        settle(c);
    }
    for (size_t i = 0; i < c->config.target_count; ++i) {
        uint64_t epoch = c->config.targets[i].epoch;
        c->config.targets[i] = (nx_i3c_target_state_t){.epoch = epoch};
    }
    c->faulted = false;
    c->pending_target = SIZE_MAX;
    return NX_SUCCESS;
}

nx_result_t nx_i3c_reopen(nx_i3c_controller_t* c) {
    nx_result_t result = live(c);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!c->closed || c->faulted || c->active != NULL) {
        return NX_ERROR_STATE;
    }
    c->closed = false;
    return NX_SUCCESS;
}

nx_result_t nx_i3c_stop(nx_i3c_controller_t* c) {
    nx_result_t result = nx_i3c_close(c);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (c->active != NULL) {
        finish(c, NX_ERROR_CANCELLED, true);
        return NX_ERROR_BUSY;
    }
    if (c->event_count != 0) {
        return NX_ERROR_BUSY;
    }
    if (c->faulted) {
        return NX_ERROR_STATE;
    }
    /* Handing off the claim must not lose a live wire address table. */
    for (size_t i = 0; i < c->config.target_count; ++i) {
        if (c->config.targets[i].assigned || c->config.targets[i].tentative) {
            return NX_ERROR_STATE;
        }
    }
    result = c->config.ops->quiescent(c->config.context);
    if (result == NX_SUCCESS) {
        c->config.ops->release(c->config.context, c);
        c->joined = true;
    }
    return result;
}
