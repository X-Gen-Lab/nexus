/**
 * \file            user_io_async.c
 * \brief           Trusted copied cells over the existing UART execution owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/user_io_async_storage.h"
#include <string.h>

_Static_assert(sizeof(nx_user_io_async_message_t) == 48U,
               "copied input is twelve scalar words");
_Static_assert(sizeof(nx_user_io_async_reply_t) == 40U,
               "copied output is ten scalar words");

/** \brief Exact known privileged control extent, never a user loan. */
typedef struct {
    const void* address;
    size_t bytes;
    bool context;
} extent_t;

static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Check complete extents before addition or typed control access. */
static bool span(const void* address, size_t bytes, size_t alignment) {
    return address != NULL && bytes != 0U &&
           (uintptr_t)address % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)address;
}

static bool overlaps(const void* first, size_t a, const void* second,
                     size_t b) {
    return (uintptr_t)first < (uintptr_t)second + b &&
           (uintptr_t)second < (uintptr_t)first + a;
}

/** \brief Fixed user windows exclude every known privileged control/payload. */
static bool privileged(const nx_user_io_async_config_t* config,
                       const void* address, size_t bytes) {
    if (!span(address, bytes, 1U)) {
        return false;
    }
    for (size_t i = 0U; i < config->region_count; ++i) {
        const nx_user_io_copy_region_t* region = &config->regions[i];
        if (overlaps(address, bytes, (const void*)region->begin,
                     region->bytes)) {
            return false;
        }
    }
    return true;
}

/** \brief A permission range is nominal policy; the copy port proves access. */
static bool permitted(const nx_user_io_async_config_t* config,
                      const void* address, size_t bytes, size_t alignment,
                      uint32_t access) {
    if (!span(address, bytes, alignment)) {
        return false;
    }
    uintptr_t first = (uintptr_t)address;
    for (size_t i = 0U; i < config->region_count; ++i) {
        const nx_user_io_copy_region_t* region = &config->regions[i];
        if ((access & ~region->access) == 0U && first >= region->begin &&
            first - region->begin < region->bytes &&
            bytes <= region->bytes - (first - region->begin)) {
            return true;
        }
    }
    return false;
}

/** \brief Read only well-formed configuration before any external callback. */
static bool config_valid(const nx_user_io_async_config_t* config) {
    if (!span(config, sizeof(*config), _Alignof(nx_user_io_async_config_t)) ||
        config->session == 0U || config->region_count == 0U ||
        config->region_count > SIZE_MAX / sizeof(*config->regions) ||
        !span(config->regions, config->region_count * sizeof(*config->regions),
              _Alignof(nx_user_io_copy_region_t)) ||
        !span(config->copy.ops, sizeof(*config->copy.ops),
              _Alignof(nx_user_io_copy_ops_t)) ||
        config->copy.ops->claim == NULL || config->copy.ops->release == NULL ||
        config->copy.ops->drained == NULL ||
        config->copy.ops->identity == NULL ||
        config->copy.ops->copy_in == NULL ||
        config->copy.ops->copy_out == NULL ||
        !span(config->copy.context, config->copy.context_bytes, 1U) ||
        !span(config->owner, sizeof(*config->owner),
              _Alignof(nx_bus_owner_t)) ||
        !span(config->executor, sizeof(*config->executor),
              _Alignof(nx_uart_owner_executor_t)) ||
        !span(config->port, sizeof(*config->port), _Alignof(nx_uart_port_t)) ||
        !span(config->port->ops, sizeof(*config->port->ops),
              _Alignof(nx_uart_ops_t)) ||
        !span(config->port->context, config->port_context_bytes, 1U)) {
        return false;
    }
    for (size_t i = 0U; i < config->region_count; ++i) {
        const nx_user_io_copy_region_t* region = &config->regions[i];
        if (!span((const void*)region->begin, region->bytes, 1U) ||
            region->access == 0U ||
            (region->access &
             ~(uint32_t)(NX_USER_IO_COPY_READ | NX_USER_IO_COPY_WRITE)) != 0U) {
            return false;
        }
        for (size_t j = 0U; j < i; ++j) {
            if (overlaps((const void*)region->begin, region->bytes,
                         (const void*)config->regions[j].begin,
                         config->regions[j].bytes)) {
                return false;
            }
        }
    }
    nx_bus_owner_t* owner = config->owner;
    if (owner->capacity == 0U ||
        owner->capacity > SIZE_MAX / sizeof(*owner->slots) ||
        !span(owner->slots, owner->capacity * sizeof(*owner->slots),
              _Alignof(nx_owner_slot_t)) ||
        owner->head > owner->capacity || owner->tail > owner->capacity ||
        owner->active > owner->capacity || owner->free_head > owner->capacity ||
        owner->guard.enter == NULL || owner->guard.leave == NULL ||
        owner->clock.read == NULL ||
        owner->executor.context != config->executor ||
        config->executor->port != config->port || config->executor->pending ||
        config->executor->inner.data != NULL ||
        config->executor->inner.length != 0U) {
        return false;
    }
    /* The local constructor has no hardware effect. Its shared method identity
     * proves this is the existing UART bridge, not an arbitrary executor. */
    nx_uart_owner_executor_t probe;
    nx_owner_executor_port_t expected =
        nx_uart_owner_executor_port(&probe, config->port);
    return expected.start != NULL && owner->executor.start == expected.start &&
           owner->executor.service == expected.service &&
           owner->executor.cancel == expected.cancel &&
           owner->executor.terminal_authority == expected.terminal_authority;
}

/** \brief Cold pairwise checks never mutate a request or reserve a role. */
static bool storage_valid(nx_user_io_async_t* mediation,
                          nx_user_io_async_slot_t* slots, size_t count,
                          const nx_user_io_async_config_t* config) {
    nx_bus_owner_t* owner = config->owner;
    const extent_t controls[] = {
        {mediation, sizeof(*mediation), false},
        {slots, count * sizeof(*slots), false},
        {config, sizeof(*config), false},
        {config->regions, config->region_count * sizeof(*config->regions),
         false},
        {config->copy.ops, sizeof(*config->copy.ops), false},
        {config->copy.context, config->copy.context_bytes, true},
        {owner, sizeof(*owner), false},
        {owner->slots, owner->capacity * sizeof(*owner->slots), false},
        {config->executor, sizeof(*config->executor), false},
        {config->port, sizeof(*config->port), false},
        {config->port->ops, sizeof(*config->port->ops), false},
        {config->port->context, config->port_context_bytes, true},
        {owner->guard.context, config->guard_context_bytes, true},
        {owner->clock.context, config->clock_context_bytes, true},
        {owner->wake.context, config->wake_context_bytes, true}};
    const size_t control_count = sizeof(controls) / sizeof(controls[0]);
    for (size_t i = 0U; i < control_count; ++i) {
        const extent_t* item = &controls[i];
        if (item->address == NULL && item->bytes == 0U && item->context) {
            continue;
        }
        if (!privileged(config, item->address, item->bytes)) {
            return false;
        }
        for (size_t j = 0U; j < i; ++j) {
            const extent_t* earlier = &controls[j];
            if (earlier->bytes == 0U) {
                continue;
            }
            bool same_context = item->context && earlier->context &&
                                item->address == earlier->address &&
                                item->bytes == earlier->bytes;
            if (!same_context && overlaps(item->address, item->bytes,
                                          earlier->address, earlier->bytes)) {
                return false;
            }
        }
    }
    for (size_t i = 0U; i < count; ++i) {
        const nx_user_io_async_slot_t* cell = &slots[i];
        if (cell->capacity > UINT32_MAX ||
            !privileged(config, cell->data, cell->capacity) ||
            cell->identity.request != NULL || cell->identity.epoch != 0U ||
            nx_request_state(&cell->request) != NX_REQUEST_READY ||
            cell->operation.data != NULL || cell->operation.length != 0U ||
            cell->owner_ticket.slot != 0U || cell->owner_ticket.epoch != 0U) {
            return false;
        }
        for (size_t j = 0U; j < control_count; ++j) {
            if (controls[j].bytes != 0U &&
                overlaps(cell->data, cell->capacity, controls[j].address,
                         controls[j].bytes)) {
                return false;
            }
        }
        for (size_t j = 0U; j < i; ++j) {
            if (overlaps(cell->data, cell->capacity, slots[j].data,
                         slots[j].capacity)) {
                return false;
            }
        }
    }
    return true;
}

nx_result_t
nx_user_io_async_construct(nx_user_io_async_t* mediation,
                           nx_user_io_async_slot_t* slots, size_t count,
                           const nx_user_io_async_config_t* config) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!span(mediation, sizeof(*mediation), _Alignof(nx_user_io_async_t))) {
        return NX_ERROR_INVALID;
    }
    if (mediation->constructing) {
        return NX_ERROR_BUSY;
    }
    if (mediation->initialized) {
        return NX_ERROR_STATE;
    }
    if (count == 0U || count > UINT32_MAX ||
        count > SIZE_MAX / sizeof(*slots) ||
        !span(slots, count * sizeof(*slots),
              _Alignof(nx_user_io_async_slot_t)) ||
        !config_valid(config) ||
        !storage_valid(mediation, slots, count, config)) {
        return NX_ERROR_INVALID;
    }
    nx_user_io_async_t before = *mediation;
    mediation->constructing = true;
    if (!nx_bus_owner_idle(config->owner)) {
        *mediation = before;
        return NX_ERROR_BUSY;
    }
    nx_result_t result = config->copy.ops->claim(
        config->copy.context, config->principal, config->device,
        config->session, config->owner, mediation);
    if (result != NX_SUCCESS) {
        *mediation = before;
        return result;
    }
    /* All fallible checks precede the exclusive persistent session claim. */
    for (size_t i = 0U; i < count; ++i) {
        nx_request_initialize(&slots[i].request);
        memset(slots[i].data, 0, slots[i].capacity);
    }
    *mediation = (nx_user_io_async_t){
        .config = config, .slots = slots, .count = count, .initialized = true};
    return NX_SUCCESS;
}

static bool live(const nx_user_io_async_t* mediation) {
    return span(mediation, sizeof(*mediation), _Alignof(nx_user_io_async_t)) &&
           mediation->initialized && mediation->config != NULL &&
           mediation->slots != NULL && mediation->count != 0U;
}

static uint64_t words(uint32_t low, uint32_t high) {
    return (uint64_t)low | ((uint64_t)high << 32U);
}

/** \brief Only acquire-observed SETTLED permits reading result/count. */
static nx_user_io_async_reply_t
snapshot(const nx_user_io_async_config_t* config, uint32_t index,
         nx_user_io_async_slot_t* slot) {
    nx_request_state_t state = nx_request_state(&slot->request);
    nx_user_io_async_reply_t reply = {
        .abi = NX_USER_IO_ASYNC_ABI,
        .slot = index,
        .session_low = (uint32_t)config->session,
        .session_high = (uint32_t)(config->session >> 32U),
        .epoch_low = (uint32_t)slot->identity.epoch,
        .epoch_high = (uint32_t)(slot->identity.epoch >> 32U),
        .state = (uint32_t)state,
        .result = NX_ERROR_BUSY,
        .accepted = 1U};
    if (state == NX_REQUEST_SETTLED) {
        nx_result_t result;
        size_t transferred;
        if (nx_request_result(&slot->request, &result, &transferred) ==
                NX_SUCCESS &&
            transferred <= slot->operation.length) {
            reply.result = (uint32_t)result;
            reply.transferred = (uint32_t)transferred;
        } else {
            reply.result = NX_ERROR_IO;
        }
    }
    return reply;
}

/** \brief External user acceptance is copied-cell reservation, not HW start. */
static nx_result_t submit(nx_user_io_async_t* mediation,
                          const nx_user_io_async_message_t* input,
                          const uint8_t* payload, uint64_t epoch) {
    const nx_user_io_async_config_t* config = mediation->config;
    nx_user_io_async_slot_t* cell = &mediation->slots[input->slot];
    if (mediation->closing) {
        return NX_ERROR_STATE;
    }
    if (cell->identity.request != NULL) {
        return NX_ERROR_BUSY;
    }
    if (cell->identity.epoch == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    if (epoch != cell->identity.epoch + 1U) {
        return NX_ERROR_STATE;
    }
    if (input->length == 0U || input->length > cell->capacity) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result =
        config->copy.ops->copy_in(config->copy.context, config->principal,
                                  cell->data, payload, input->length);
    if (result != NX_SUCCESS || mediation->closing) {
        memset(cell->data, 0, cell->capacity);
        return result != NX_SUCCESS ? result : NX_ERROR_STATE;
    }
    result = nx_request_prepare(
        &cell->request, words(input->deadline_low, input->deadline_high));
    if (result != NX_SUCCESS) {
        memset(cell->data, 0, cell->capacity);
        return result;
    }
    uint64_t issued;
    result = nx_request_slot_bind(&cell->identity, &cell->request, &issued);
    if (result != NX_SUCCESS) {
        memset(cell->data, 0, cell->capacity);
        return result;
    }
    cell->operation = (nx_uart_owner_operation_t){cell->data, input->length};
    result = nx_bus_owner_submit(config->owner, &cell->request,
                                 &cell->operation, NULL, &cell->owner_ticket);
    if (result != NX_SUCCESS) {
        /* The existing owner's rejected admission retains no references. The
         * copied cell remains accepted and reports that terminal error. */
        if (nx_request_admit(&cell->request, NX_REQUEST_ACTIVE) == NX_SUCCESS) {
            nx_request_settle(&cell->request, result, 0U);
        } else {
            (void)nx_request_transition(&cell->request, NX_REQUEST_QUARANTINED);
        }
    } else if (mediation->closing &&
               nx_request_state(&cell->request) != NX_REQUEST_SETTLED) {
        (void)nx_bus_owner_cancel(config->owner, cell->owner_ticket);
    }
    return NX_SUCCESS;
}

/** \brief Validate every transient user span before invoking a copy callback.
 */
static nx_result_t dispatch(nx_user_io_async_t* mediation,
                            const nx_user_io_async_message_t* message,
                            const uint8_t* payload,
                            nx_user_io_async_reply_t* reply) {
    const nx_user_io_async_config_t* config = mediation->config;
    uint32_t principal = 0U;
    nx_result_t result =
        config->copy.ops->identity(config->copy.context, &principal);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (principal != config->principal) {
        return NX_ERROR_PERMISSION;
    }
    if (!permitted(config, message, sizeof(*message),
                   _Alignof(nx_user_io_async_message_t),
                   NX_USER_IO_COPY_READ) ||
        !permitted(config, reply, sizeof(*reply),
                   _Alignof(nx_user_io_async_reply_t), NX_USER_IO_COPY_WRITE)) {
        return NX_ERROR_PERMISSION;
    }
    if (overlaps(message, sizeof(*message), reply, sizeof(*reply))) {
        return NX_ERROR_INVALID;
    }
    nx_user_io_async_message_t input = {0};
    result = config->copy.ops->copy_in(config->copy.context, principal, &input,
                                       message, sizeof(input));
    if (result != NX_SUCCESS) {
        return result;
    }
    if (input.abi != NX_USER_IO_ASYNC_ABI || input.reserved != 0U ||
        input.action < NX_USER_IO_ASYNC_SUBMIT ||
        input.action > NX_USER_IO_ASYNC_CONSUME ||
        input.slot >= mediation->count) {
        return NX_ERROR_INVALID;
    }
    if (input.device != config->device) {
        return NX_ERROR_PERMISSION;
    }
    if (words(input.session_low, input.session_high) != config->session) {
        return NX_ERROR_STATE;
    }
    nx_user_io_async_slot_t* cell = &mediation->slots[input.slot];
    uint64_t epoch = words(input.epoch_low, input.epoch_high);
    if (input.action == NX_USER_IO_ASYNC_SUBMIT) {
        if (mediation->closing) {
            return NX_ERROR_STATE;
        }
        if (input.length == 0U || input.length > cell->capacity) {
            return NX_ERROR_INVALID;
        }
        if (!permitted(config, payload, input.length, 1U,
                       NX_USER_IO_COPY_READ)) {
            return NX_ERROR_PERMISSION;
        }
        if (overlaps(payload, input.length, message, sizeof(*message)) ||
            overlaps(payload, input.length, reply, sizeof(*reply))) {
            return NX_ERROR_INVALID;
        }
        result = submit(mediation, &input, payload, epoch);
        if (result != NX_SUCCESS) {
            return result;
        }
    } else {
        if (payload != NULL || input.length != 0U || input.deadline_low != 0U ||
            input.deadline_high != 0U) {
            return NX_ERROR_INVALID;
        }
        if (nx_request_slot_lookup(&cell->identity, epoch) != &cell->request) {
            return NX_ERROR_STATE;
        }
        if (input.action == NX_USER_IO_ASYNC_CANCEL &&
            nx_request_state(&cell->request) != NX_REQUEST_SETTLED) {
            result = nx_bus_owner_cancel(config->owner, cell->owner_ticket);
            if (result != NX_SUCCESS &&
                nx_request_state(&cell->request) != NX_REQUEST_SETTLED) {
                return result;
            }
        }
        if (input.action == NX_USER_IO_ASYNC_CONSUME &&
            (nx_request_state(&cell->request) != NX_REQUEST_SETTLED ||
             !nx_bus_owner_idle(config->owner))) {
            return NX_ERROR_BUSY;
        }
    }
    nx_user_io_async_reply_t output = snapshot(config, input.slot, cell);
    result = config->copy.ops->copy_out(config->copy.context, principal, reply,
                                        &output, sizeof(output));
    if (result == NX_SUCCESS && input.action == NX_USER_IO_ASYNC_CONSUME) {
        result = nx_request_slot_release(&cell->identity, epoch);
        if (result == NX_SUCCESS) {
            /* Owner idle has joined its publisher; no callback/reference to
             * this descriptor or payload can survive Core SETTLED and join. */
            memset(cell->data, 0, cell->capacity);
            cell->operation = (nx_uart_owner_operation_t){0};
            cell->owner_ticket = (nx_owner_ticket_t){0};
        }
    }
    return result;
}

nx_result_t nx_user_io_async_dispatch(nx_user_io_async_t* mediation,
                                      const nx_user_io_async_message_t* message,
                                      const uint8_t* payload,
                                      nx_user_io_async_reply_t* reply) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!live(mediation) || mediation->joined || mediation->reaped) {
        return NX_ERROR_STATE;
    }
    if (mediation->working || mediation->closing_call) {
        return NX_ERROR_BUSY;
    }
    mediation->working = true;
    nx_result_t result = dispatch(mediation, message, payload, reply);
    mediation->working = false;
    return result;
}

nx_result_t nx_user_io_async_close(nx_user_io_async_t* mediation) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!live(mediation)) {
        return NX_ERROR_STATE;
    }
    if (mediation->releasing || mediation->closing_call || mediation->reaping) {
        return NX_ERROR_BUSY;
    }
    if (mediation->joined) {
        return NX_SUCCESS;
    }
    mediation->closing_call = true;
    mediation->closing = true;
    nx_result_t result = NX_SUCCESS;
    for (size_t i = 0U; i < mediation->count; ++i) {
        nx_user_io_async_slot_t* cell = &mediation->slots[i];
        if (cell->identity.request != NULL &&
            nx_request_state(&cell->request) != NX_REQUEST_SETTLED) {
            nx_result_t cancelled = nx_bus_owner_cancel(
                mediation->config->owner, cell->owner_ticket);
            if (cancelled != NX_SUCCESS &&
                nx_request_state(&cell->request) != NX_REQUEST_SETTLED &&
                result == NX_SUCCESS) {
                result = cancelled;
            }
        }
    }
    mediation->closing_call = false;
    return result;
}

nx_result_t nx_user_io_async_join(nx_user_io_async_t* mediation) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!live(mediation)) {
        return NX_ERROR_STATE;
    }
    if (mediation->working || mediation->releasing || mediation->closing_call) {
        return NX_ERROR_BUSY;
    }
    if (!mediation->closing) {
        return NX_ERROR_STATE;
    }
    if (mediation->joined) {
        return NX_SUCCESS;
    }
    for (size_t i = 0U; i < mediation->count; ++i) {
        if (mediation->slots[i].identity.request != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    mediation->working = true;
    mediation->releasing = true;
    nx_result_t result = NX_ERROR_BUSY;
    if (nx_bus_owner_idle(mediation->config->owner)) {
        const nx_user_io_async_config_t* config = mediation->config;
        result = config->copy.ops->release(config->copy.context, mediation,
                                           config->session);
        if (result == NX_SUCCESS) {
            mediation->joined = true;
        }
    }
    mediation->releasing = false;
    mediation->working = false;
    return result;
}

/** \brief Retire copied cells only after both independent joins prove exit. */
nx_result_t nx_user_io_async_reap(nx_user_io_async_t* mediation) {
    if (!task()) {
        return NX_ERROR_CONTEXT;
    }
    if (!live(mediation)) {
        return NX_ERROR_STATE;
    }
    if (mediation->working || mediation->releasing || mediation->closing_call ||
        mediation->reaping) {
        return NX_ERROR_BUSY;
    }
    if (!mediation->closing) {
        return NX_ERROR_STATE;
    }
    if (mediation->joined || mediation->reaped) {
        return NX_SUCCESS;
    }
    mediation->working = true;
    mediation->reaping = true;
    nx_result_t result = NX_SUCCESS;
    /* Preflight every cell before any retirement. No observer may modify these
     * private identities/requests; serialized control prevents a new borrower.
     */
    for (size_t i = 0U; i < mediation->count; ++i) {
        nx_user_io_async_slot_t* cell = &mediation->slots[i];
        if (cell->identity.request == NULL) {
            continue;
        }
        if (nx_request_slot_lookup(&cell->identity, cell->identity.epoch) !=
            &cell->request) {
            result = NX_ERROR_STATE;
            break;
        }
        if (nx_request_state(&cell->request) != NX_REQUEST_SETTLED) {
            result = NX_ERROR_BUSY;
            break;
        }
    }
    const nx_user_io_async_config_t* config = mediation->config;
    if (result == NX_SUCCESS && !nx_bus_owner_idle(config->owner)) {
        result = NX_ERROR_BUSY;
    }
    if (result == NX_SUCCESS) {
        result = config->copy.ops->drained(config->copy.context, mediation,
                                           config->session);
    }
    if (result == NX_SUCCESS) {
        mediation->reaped = true;
        for (size_t i = 0U; i < mediation->count; ++i) {
            nx_user_io_async_slot_t* cell = &mediation->slots[i];
            if (cell->identity.request != NULL) {
                /* Exact identity and SETTLED were prevalidated for ALL cells;
                 * joined observers cannot change them. Release cannot fail and
                 * no callback/fallible operation follows this final proof. */
                (void)nx_request_slot_release(&cell->identity,
                                              cell->identity.epoch);
                memset(cell->data, 0, cell->capacity);
                cell->operation = (nx_uart_owner_operation_t){0};
                cell->owner_ticket = (nx_owner_ticket_t){0};
            }
        }
    }
    mediation->reaping = false;
    mediation->working = false;
    return result;
}
