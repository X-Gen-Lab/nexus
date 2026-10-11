/**
 * \file            user_io_broker.c
 * \brief           Trusted scalar dispatch and retained user-address ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "user_io_broker_impl.h"
#include <string.h>

/** \brief Arithmetic and alignment precede interpretation of private objects.
 */
static bool span_valid(const void* address, size_t bytes, size_t alignment) {
    uintptr_t start = (uintptr_t)address;
    return address != NULL && bytes != 0U && start % alignment == 0U &&
           bytes <= UINTPTR_MAX - start;
}
/** \brief Checked spans only; overflow must never create a containment result.
 */
static bool overlaps(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return a < b + second_bytes && b < a + first_bytes;
}
/** \brief This component cannot grant CPU privilege or authenticate an SVC. */
static bool kernel_context(void) {
    return nx_arch_is_privileged() && nx_arch_exception_number() == 0U;
}
/** \brief Every known control must remain entirely outside configured user RAM.
 */
static bool privileged_span(const nx_user_io_broker_config_t* config,
                            const void* address, size_t bytes) {
    if (!span_valid(address, bytes, 1U)) {
        return false;
    }
    for (size_t i = 0U; i < config->region_count; ++i) {
        if (overlaps(address, bytes, config->regions[i].base,
                     config->regions[i].bytes)) {
            return false;
        }
    }
    return true;
}
/** \brief Exact trusted identity, access and complete single-window
 * containment. */
static bool permitted(const nx_user_io_broker_config_t* config,
                      uint32_t principal, const void* address, size_t bytes,
                      uint32_t access) {
    uintptr_t start = (uintptr_t)address;
    for (size_t i = 0U; i < config->region_count; ++i) {
        const nx_user_io_region_t* region = &config->regions[i];
        uintptr_t base = (uintptr_t)region->base;
        if (region->principal == principal &&
            (access & ~region->access) == 0U && start >= base &&
            start + bytes <= base + region->bytes) {
            return true;
        }
    }
    return false;
}
/** \brief Internal proof; only these two known child tokens may share a call.
 */
static nx_result_t call_drained(const void* context);

/** \brief Kernel metadata must not overwrite any known borrowed static control.
 */
static bool config_control_overlap(const nx_user_io_broker_config_t* config,
                                   const void* address, size_t bytes) {
    if (overlaps(address, bytes, config, sizeof(*config)) ||
        overlaps(address, bytes, config->regions,
                 config->region_count * sizeof(*config->regions)) ||
        overlaps(address, bytes, config->grants,
                 config->grant_count * sizeof(*config->grants)) ||
        overlaps(address, bytes, config->memory.ops,
                 sizeof(*config->memory.ops)) ||
        overlaps(address, bytes, config->memory.context,
                 config->memory.context_bytes)) {
        return true;
    }
    for (size_t i = 0U; i < config->grant_count; ++i) {
        const nx_user_io_gpio_grant_t* grant = &config->grants[i];
        if (overlaps(address, bytes, grant->port, sizeof(*grant->port)) ||
            overlaps(address, bytes, grant->port->ops,
                     sizeof(*grant->port->ops)) ||
            overlaps(address, bytes, grant->port->context,
                     grant->context_bytes)) {
            return true;
        }
    }
    return false;
}

/** \brief Validate arrays first; later scans rely on immutable checked policy.
 */
static bool config_valid(const nx_user_io_broker_t* broker,
                         const nx_user_io_broker_config_t* config) {
    if (!span_valid(config, sizeof(*config),
                    _Alignof(nx_user_io_broker_config_t)) ||
        config->region_count == 0U || config->grant_count == 0U ||
        config->loan_capacity == 0U ||
        config->region_count > SIZE_MAX / sizeof(*config->regions) ||
        config->grant_count > SIZE_MAX / sizeof(*config->grants) ||
        !span_valid(config->regions,
                    config->region_count * sizeof(*config->regions),
                    _Alignof(nx_user_io_region_t)) ||
        !span_valid(config->grants,
                    config->grant_count * sizeof(*config->grants),
                    _Alignof(nx_user_io_gpio_grant_t))) {
        return false;
    }
    for (size_t i = 0U; i < config->region_count; ++i) {
        const nx_user_io_region_t* region = &config->regions[i];
        if (!span_valid(region->base, region->bytes, 1U) ||
            region->access == 0U ||
            (region->access &
             ~(uint32_t)(NX_USER_MEMORY_READ | NX_USER_MEMORY_WRITE)) != 0U) {
            return false;
        }
        for (size_t j = 0U; j < i; ++j) {
            if (overlaps(region->base, region->bytes, config->regions[j].base,
                         config->regions[j].bytes)) {
                return false;
            }
        }
    }
    const nx_user_io_memory_port_t* memory = &config->memory;
    if (!span_valid(memory->ops, sizeof(*memory->ops),
                    _Alignof(nx_user_io_memory_ops_t)) ||
        memory->ops->identity == NULL || memory->ops->pin == NULL ||
        memory->ops->unpin == NULL ||
        !privileged_span(config, broker, sizeof(*broker)) ||
        !privileged_span(config, config, sizeof(*config)) ||
        !privileged_span(config, config->regions,
                         config->region_count * sizeof(*config->regions)) ||
        !privileged_span(config, config->grants,
                         config->grant_count * sizeof(*config->grants)) ||
        !privileged_span(config, memory->ops, sizeof(*memory->ops)) ||
        !privileged_span(config, memory->context, memory->context_bytes)) {
        return false;
    }
    for (size_t i = 0U; i < config->grant_count; ++i) {
        const nx_user_io_gpio_grant_t* grant = &config->grants[i];
        if (!span_valid(grant->port, sizeof(*grant->port),
                        _Alignof(nx_gpio_port_t)) ||
            !span_valid(grant->port->ops, sizeof(*grant->port->ops),
                        _Alignof(nx_gpio_ops_t)) ||
            (grant->read_mask | grant->write_mask) == 0U ||
            !privileged_span(config, grant->port, sizeof(*grant->port)) ||
            !privileged_span(config, grant->port->ops,
                             sizeof(*grant->port->ops)) ||
            !privileged_span(config, grant->port->context,
                             grant->context_bytes)) {
            return false;
        }
        for (size_t j = 0U; j < i; ++j) {
            if (grant->principal == config->grants[j].principal &&
                grant->device == config->grants[j].device) {
                return false;
            }
        }
    }
    return !config_control_overlap(config, broker, sizeof(*broker));
}
/** \brief Provider storage and frozen policy stay live throughout loan
 * ownership. */
static bool live(const nx_user_io_broker_t* broker) {
    return span_valid(broker, sizeof(*broker), _Alignof(nx_user_io_broker_t)) &&
           broker->initialized && broker->config != NULL &&
           broker->count <= broker->config->loan_capacity;
}
/** \brief Exact retained pointer rather than copied metadata or user addresses.
 */
static nx_user_io_loan_t** retained(nx_user_io_broker_t* broker,
                                    const nx_user_io_loan_t* slot) {
    nx_user_io_loan_t** link = &broker->head;
    for (size_t i = 0U; i < broker->count; ++i) {
        if (!span_valid(*link, sizeof(**link), _Alignof(nx_user_io_loan_t)) ||
            (*link)->owner != broker || (*link)->epoch == 0U) {
            return NULL;
        }
        if (*link == slot) {
            return link;
        }
        link = &(*link)->next;
    }
    return NULL;
}
/** \brief A private GPIO call owns precisely its two embedded child loans. */
static bool call_child(nx_user_io_broker_t* broker,
                       const nx_user_io_loan_t* slot,
                       nx_user_io_drain_port_t drain) {
    if (drain.check != call_drained ||
        drain.context_bytes != sizeof(nx_user_io_call_t) ||
        !span_valid(drain.context, sizeof(nx_user_io_call_t),
                    _Alignof(nx_user_io_call_t))) {
        return false;
    }
    const nx_user_io_call_t* call = drain.context;
    return call->owner == broker &&
           (slot == &call->input || slot == &call->output);
}

/** \brief Reject controls overlapping each other or any writable peer borrow.
 */
static bool loan_spans_valid(nx_user_io_broker_t* broker,
                             const nx_user_io_loan_t* slot, const void* address,
                             size_t bytes, uint32_t access,
                             nx_user_io_drain_port_t drain) {
    const nx_user_io_broker_config_t* config = broker->config;
    bool child = call_child(broker, slot, drain);
    if (!privileged_span(config, slot, sizeof(*slot)) ||
        !privileged_span(config, drain.context, drain.context_bytes) ||
        drain.check == NULL ||
        overlaps(slot, sizeof(*slot), broker, sizeof(*broker)) ||
        config_control_overlap(config, slot, sizeof(*slot)) ||
        (overlaps(slot, sizeof(*slot), drain.context, drain.context_bytes) &&
         !child)) {
        return false;
    }
    nx_user_io_loan_t* other = broker->head;
    for (size_t i = 0U; i < broker->count; ++i) {
        if (!span_valid(other, sizeof(*other), _Alignof(nx_user_io_loan_t)) ||
            other->owner != broker ||
            overlaps(slot, sizeof(*slot), other, sizeof(*other)) ||
            ((overlaps(slot, sizeof(*slot), other->drain.context,
                       other->drain.context_bytes) ||
              overlaps(drain.context, drain.context_bytes, other,
                       sizeof(*other))) &&
             !(child && other->drain.context == drain.context &&
               other->drain.check == call_drained &&
               call_child(broker, other, other->drain))) ||
            (((access | other->access) & NX_USER_MEMORY_WRITE) != 0U &&
             overlaps(address, bytes, other->address, other->bytes))) {
            return false;
        }
        other = other->next;
    }
    return other == NULL;
}
/** \brief Detach the source list before final private token stores. */
static void detach(nx_user_io_broker_t* broker, nx_user_io_loan_t* slot,
                   nx_user_io_loan_t** link) {
    *link = slot->next;
    --broker->count;
    uint64_t epoch = slot->epoch;
    *slot = (nx_user_io_loan_t){.epoch = epoch};
}

nx_result_t
nx_user_io_broker_construct(nx_user_io_broker_t* broker,
                            const nx_user_io_broker_config_t* config) {
    if (!span_valid(broker, sizeof(*broker), _Alignof(nx_user_io_broker_t)) ||
        !span_valid(config, sizeof(*config),
                    _Alignof(nx_user_io_broker_config_t)) ||
        overlaps(broker, sizeof(*broker), config, sizeof(*config))) {
        return NX_ERROR_INVALID;
    }
    if (!kernel_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!config_valid(broker, config)) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (!broker->initialized && broker->config == NULL &&
        broker->head == NULL && broker->count == 0U && !broker->closing &&
        !broker->working) {
        broker->config = config;
        broker->initialized = true;
        result = NX_SUCCESS;
    }
    nx_arch_irq_restore(saved);
    return result;
}

nx_user_io_loan_result_t
nx_user_io_loan_acquire(nx_user_io_broker_t* broker, nx_user_io_loan_t* slot,
                        uint32_t principal, void* address, size_t bytes,
                        uint32_t access, nx_user_io_drain_port_t drain) {
    nx_user_io_loan_result_t result = {.result = NX_ERROR_INVALID};
    if (!span_valid(broker, sizeof(*broker), _Alignof(nx_user_io_broker_t)) ||
        !span_valid(slot, sizeof(*slot), _Alignof(nx_user_io_loan_t)) ||
        !span_valid(address, bytes, 1U) || access == 0U ||
        (access & ~(uint32_t)(NX_USER_MEMORY_READ | NX_USER_MEMORY_WRITE)) !=
            0U) {
        return result;
    }
    if (!kernel_context() || nx_arch_irq_is_masked()) {
        result.result = NX_ERROR_CONTEXT;
        return result;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!live(broker)) {
        result.result = NX_ERROR_STATE;
    } else if (retained(broker, slot) != NULL) {
        result.result = NX_ERROR_BUSY;
    } else if (!loan_spans_valid(broker, slot, address, bytes, access, drain)) {
        result.result = NX_ERROR_INVALID;
    } else if (!permitted(broker->config, principal, address, bytes, access)) {
        result.result = NX_ERROR_PERMISSION;
    } else if (slot->owner != NULL || slot->next != NULL ||
               slot->address != NULL || slot->bytes != 0U || slot->working ||
               slot->pinned) {
        result.result = NX_ERROR_STATE;
    } else if (broker->closing) {
        result.result = NX_ERROR_BUSY;
    } else if (broker->count == broker->config->loan_capacity ||
               slot->epoch == UINT64_MAX) {
        result.result = NX_ERROR_EXHAUSTED;
    } else {
        ++slot->epoch;
        slot->owner = broker;
        slot->address = address;
        slot->bytes = bytes;
        slot->principal = principal;
        slot->access = access;
        slot->drain = drain;
        slot->working = true;
        slot->next = broker->head;
        broker->head = slot;
        ++broker->count;
        result.result = NX_SUCCESS;
    }
    nx_arch_irq_restore(saved);
    if (result.result != NX_SUCCESS) {
        return result;
    }
    const nx_user_io_memory_port_t* memory = &broker->config->memory;
    nx_result_t pin = memory->ops->pin(memory->context, principal, address,
                                       bytes, access, &slot->cookie);
    saved = nx_arch_irq_save();
    if (pin == NX_SUCCESS) {
        slot->pinned = true;
        slot->working = false;
        result.accepted = true;
        result.ticket = (nx_user_io_loan_ticket_t){slot, slot->epoch};
        result.result = broker->closing ? NX_ERROR_CANCELLED : NX_SUCCESS;
    } else {
        /* Pin rejection's zero-effects contract permits metadata withdrawal;
         * a successful pin always remains ACCEPTED through late revocation. */
        nx_user_io_loan_t** link = retained(broker, slot);
        detach(broker, slot, link);
        result.result = pin;
    }
    nx_arch_irq_restore(saved);
    return result;
}

nx_result_t nx_user_io_loan_release(nx_user_io_broker_t* broker,
                                    nx_user_io_loan_ticket_t ticket) {
    if (!span_valid(broker, sizeof(*broker), _Alignof(nx_user_io_broker_t)) ||
        !span_valid(ticket.slot, sizeof(*ticket.slot),
                    _Alignof(nx_user_io_loan_t))) {
        return NX_ERROR_INVALID;
    }
    if (!kernel_context() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    nx_user_io_loan_t* slot = ticket.slot;
    if (live(broker) && retained(broker, slot) != NULL &&
        slot->epoch == ticket.epoch) {
        if (slot->working || !slot->pinned) {
            result = NX_ERROR_BUSY;
        } else {
            slot->working = true;
            result = NX_SUCCESS;
        }
    }
    nx_arch_irq_restore(saved);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = slot->drain.check(slot->drain.context);
    if (result == NX_SUCCESS) {
        const nx_user_io_memory_port_t* memory = &broker->config->memory;
        result = memory->ops->unpin(memory->context, slot->cookie);
    }
    saved = nx_arch_irq_save();
    if (result == NX_SUCCESS) {
        detach(broker, slot, retained(broker, slot));
    } else {
        slot->working = false;
    }
    nx_arch_irq_restore(saved);
    return result;
}

nx_result_t nx_user_io_broker_close(nx_user_io_broker_t* broker) {
    if (!kernel_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (live(broker)) {
        broker->closing = true;
        result = NX_SUCCESS;
    }
    nx_arch_irq_restore(saved);
    return result;
}

nx_result_t nx_user_io_broker_quiescent(const nx_user_io_broker_t* broker) {
    if (!kernel_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (live(broker)) {
        result = broker->count != 0U || broker->head != NULL || broker->working
                     ? NX_ERROR_BUSY
                 : broker->closing ? NX_SUCCESS
                                   : NX_ERROR_STATE;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Capture trusted policy once; user messages contain only compact IDs.
 */
static const nx_user_io_gpio_grant_t*
grant_for(const nx_user_io_broker_config_t* config, uint32_t principal,
          const nx_user_io_message_t* message, nx_result_t* result) {
    if (message->abi != NX_USER_IO_ABI || message->reserved[0] != 0U ||
        message->reserved[1] != 0U) {
        *result = NX_ERROR_INVALID;
        return NULL;
    }
    if (message->operation != NX_USER_GPIO_WRITE &&
        message->operation != NX_USER_GPIO_READ) {
        *result = NX_ERROR_UNSUPPORTED;
        return NULL;
    }
    if ((message->operation == NX_USER_GPIO_WRITE &&
         (message->read_mask != 0U ||
          (message->set_mask & message->reset_mask) != 0U)) ||
        (message->operation == NX_USER_GPIO_READ &&
         (message->set_mask != 0U || message->reset_mask != 0U ||
          message->read_mask == 0U))) {
        *result = NX_ERROR_INVALID;
        return NULL;
    }
    for (size_t i = 0U; i < config->grant_count; ++i) {
        const nx_user_io_gpio_grant_t* grant = &config->grants[i];
        if (grant->principal != principal || grant->device != message->device) {
            continue;
        }
        uint32_t mask = message->operation == NX_USER_GPIO_WRITE
                            ? message->set_mask | message->reset_mask
                            : message->read_mask;
        uint32_t allowed = message->operation == NX_USER_GPIO_WRITE
                               ? grant->write_mask
                               : grant->read_mask;
        if (allowed == 0U || (mask & ~allowed) != 0U) {
            break;
        }
        *result = NX_SUCCESS;
        return grant;
    }
    *result = NX_ERROR_PERMISSION;
    return NULL;
}
/** \brief Synchronous bounded GPIO and copies have no deferred provider access.
 */
static nx_result_t call_drained(const void* context) {
    const nx_user_io_call_t* call = context;
    return call->inflight ? NX_ERROR_BUSY : NX_SUCCESS;
}
/** \brief Report outcome separately from retained cleanup, never hide a write.
 */
static nx_user_io_result_t call_result(const nx_user_io_call_t* call) {
    return (nx_user_io_result_t){
        .result = call->result,
        .cleanup_result = call->cleanup_result,
        .accepted = call->accepted,
        .retained = call->input.owner != NULL || call->output.owner != NULL,
        .device_effect_possible = call->device_effect_possible};
}
/** \brief Retry output then input cleanup; no device, reply or user read
 * occurs. */
static void call_cleanup(nx_user_io_broker_t* broker, nx_user_io_call_t* call) {
    nx_result_t cleanup = NX_SUCCESS;
    if (call->output.owner != NULL) {
        cleanup = nx_user_io_loan_release(
            broker,
            (nx_user_io_loan_ticket_t){&call->output, call->output.epoch});
    }
    if (call->input.owner != NULL) {
        const nx_result_t input = nx_user_io_loan_release(
            broker,
            (nx_user_io_loan_ticket_t){&call->input, call->input.epoch});
        if (cleanup == NX_SUCCESS) {
            cleanup = input;
        }
    }
    call->cleanup_result = cleanup;
    if (call->output.owner == NULL && call->input.owner == NULL) {
        call->owner = NULL;
    }
}
/** \brief Kernel slot cannot overlap user RAM, broker or immutable controls. */
static bool call_storage_valid(const nx_user_io_broker_t* broker,
                               const nx_user_io_call_t* call) {
    if (!span_valid(call, sizeof(*call), _Alignof(nx_user_io_call_t)) ||
        !privileged_span(broker->config, call, sizeof(*call)) ||
        overlaps(call, sizeof(*call), broker, sizeof(*broker)) ||
        config_control_overlap(broker->config, call, sizeof(*call))) {
        return false;
    }
    const nx_user_io_loan_t* other = broker->head;
    for (size_t i = 0U; i < broker->count; ++i) {
        if (!span_valid(other, sizeof(*other), _Alignof(nx_user_io_loan_t)) ||
            other->owner != broker) {
            return false;
        }
        if ((overlaps(call, sizeof(*call), other, sizeof(*other)) ||
             overlaps(call, sizeof(*call), other->drain.context,
                      other->drain.context_bytes)) &&
            !(call->owner == broker && other->drain.context == call &&
              other->drain.check == call_drained &&
              (other == &call->input || other == &call->output))) {
            return false;
        }
        other = other->next;
    }
    return other == NULL;
}

nx_user_io_result_t nx_user_io_broker_gpio(nx_user_io_broker_t* broker,
                                           nx_user_io_call_t* call,
                                           const nx_user_io_message_t* message,
                                           nx_user_io_reply_t* reply) {
    nx_user_io_result_t reject = {.result = NX_ERROR_INVALID};
    if (!span_valid(broker, sizeof(*broker), _Alignof(nx_user_io_broker_t)) ||
        !span_valid(message, sizeof(*message),
                    _Alignof(nx_user_io_message_t)) ||
        !span_valid(reply, sizeof(*reply), _Alignof(nx_user_io_reply_t)) ||
        overlaps(message, sizeof(*message), reply, sizeof(*reply))) {
        return reject;
    }
    if (!kernel_context() || nx_arch_irq_is_masked()) {
        reject.result = NX_ERROR_CONTEXT;
        return reject;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!live(broker) || !call_storage_valid(broker, call)) {
        nx_arch_irq_restore(saved);
        return reject;
    }
    if (call->owner != NULL || call->inflight || call->input.owner != NULL ||
        call->output.owner != NULL || broker->working || broker->closing) {
        reject.result = NX_ERROR_BUSY;
        nx_arch_irq_restore(saved);
        return reject;
    }
    broker->working = true;
    call->owner = broker;
    call->accepted = false;
    call->device_effect_possible = false;
    call->inflight = true;
    call->result = NX_SUCCESS;
    call->cleanup_result = NX_SUCCESS;
    nx_arch_irq_restore(saved);
    uint32_t principal = 0U;
    const nx_user_io_memory_port_t* memory = &broker->config->memory;
    call->result = memory->ops->identity(memory->context, &principal);
    nx_user_io_drain_port_t drain = {call_drained, call, sizeof(*call)};
    const nx_user_io_gpio_grant_t* grant = NULL;
    if (call->result == NX_SUCCESS) {
        nx_user_io_loan_result_t input = nx_user_io_loan_acquire(
            broker, &call->input, principal, (void*)message, sizeof(*message),
            NX_USER_MEMORY_READ, drain);
        call->accepted = input.accepted;
        call->result = input.result;
        if (call->result == NX_SUCCESS) {
            memcpy(&call->snapshot, message, sizeof(call->snapshot));
            grant = grant_for(broker->config, principal, &call->snapshot,
                              &call->result);
        }
    }
    if (call->result == NX_SUCCESS) {
        nx_user_io_loan_result_t output = nx_user_io_loan_acquire(
            broker, &call->output, principal, reply, sizeof(*reply),
            NX_USER_MEMORY_WRITE, drain);
        call->result = output.result;
    }
    if (call->result == NX_SUCCESS) {
        bool dispatched = false;
        saved = nx_arch_irq_save();
        if (broker->closing) {
            call->result = NX_ERROR_CANCELLED;
        } else if (call->snapshot.operation == NX_USER_GPIO_WRITE) {
            /* Bounded scalar MMIO stays in the final revocation guard: either
             * the effect precedes close, or close prohibits the effect. */
            call->device_effect_possible = true;
            dispatched = true;
            call->result =
                nx_gpio_port_write(grant->port, call->snapshot.set_mask,
                                   call->snapshot.reset_mask);
            call->reply = (nx_user_io_reply_t){call->result, 0U};
        } else {
            dispatched = true;
            uint32_t value = 0U;
            call->result = nx_gpio_port_read(grant->port, &value);
            call->reply = (nx_user_io_reply_t){
                call->result, call->result == NX_SUCCESS
                                  ? value & call->snapshot.read_mask
                                  : 0U};
        }
        nx_arch_irq_restore(saved);
        if (dispatched) {
            memcpy(reply, &call->reply, sizeof(*reply));
        }
    }
    call->inflight = false;
    call_cleanup(broker, call);
    saved = nx_arch_irq_save();
    broker->working = false;
    nx_user_io_result_t result = call_result(call);
    nx_arch_irq_restore(saved);
    return result;
}

nx_user_io_result_t nx_user_io_broker_gpio_finish(nx_user_io_broker_t* broker,
                                                  nx_user_io_call_t* call) {
    nx_user_io_result_t reject = {.result = NX_ERROR_INVALID};
    if (!kernel_context() || nx_arch_irq_is_masked()) {
        reject.result = NX_ERROR_CONTEXT;
        return reject;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!live(broker) || !call_storage_valid(broker, call)) {
        nx_arch_irq_restore(saved);
        return reject;
    }
    if (call->owner != broker || call->inflight || broker->working) {
        reject.result = NX_ERROR_STATE;
        nx_arch_irq_restore(saved);
        return reject;
    }
    broker->working = true;
    nx_arch_irq_restore(saved);
    call_cleanup(broker, call);
    saved = nx_arch_irq_save();
    broker->working = false;
    nx_user_io_result_t result = call_result(call);
    nx_arch_irq_restore(saved);
    return result;
}
