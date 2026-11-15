/**
 * \file            spi_dma_owner.c
 * \brief           Preserve segmented provider loans through outer settlement
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/spi_dma_owner.h"
#include "nexus/arch/arch.h"

/** \brief Remove private descriptor references before releasing outer storage.
 */
static void detach(nx_spi_dma_owner_executor_t* executor) {
    executor->inner.segments = NULL;
    executor->inner.count = 0U;
    executor->inner.word_bits = 0U;
    executor->pending = false;
}
/** \brief A foreign controller is rejected before any private preparation. */
static nx_result_t start(void* context, void* operation,
                         nx_time_us_t deadline) {
    nx_spi_dma_owner_executor_t* executor = context;
    const nx_spi_dma_owner_operation_t* transfer = operation;
    if (executor->pending) {
        return NX_ERROR_BUSY;
    }
    if (transfer == NULL ||
        !nx_spi_dma_owner_contains(executor->binding, transfer->endpoint)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_spi_dma_transaction_prepare(
        &executor->inner, transfer->segments, transfer->count,
        transfer->word_bits, deadline);
    if (result == NX_SUCCESS) {
        result =
            nx_spi_dma_transaction_submit(transfer->endpoint, &executor->inner);
    }
    if (result != NX_SUCCESS) {
        detach(executor);
        return result;
    }
    executor->pending = true;
    return NX_SUCCESS;
}
/** \brief Inner acquire publication precedes all outer reference release. */
static nx_result_t service(void* context, nx_result_t* result,
                           size_t* transferred) {
    nx_spi_dma_owner_executor_t* executor = context;
    if (!executor->pending) {
        return NX_ERROR_STATE;
    }
    nx_spi_dma_transaction_service(executor->binding->port);
    nx_request_state_t state = nx_request_state(&executor->inner.base);
    if (state != NX_REQUEST_SETTLED) {
        return state == NX_REQUEST_QUARANTINED ? NX_ERROR_IO : NX_ERROR_BUSY;
    }
    nx_result_t collected =
        nx_request_result(&executor->inner.base, result, transferred);
    if (collected == NX_SUCCESS) {
        detach(executor);
    }
    return collected;
}
/** \brief Earlier wire or deadline facts take precedence over late cancel. */
static nx_result_t cancel(void* context) {
    nx_spi_dma_owner_executor_t* executor = context;
    if (!executor->pending) {
        return NX_ERROR_STATE;
    }
    nx_spi_dma_transaction_service(executor->binding->port);
    return nx_request_state(&executor->inner.base) == NX_REQUEST_SETTLED
               ? NX_SUCCESS
               : nx_spi_dma_transaction_cancel(executor->binding->port,
                                               &executor->inner);
}
nx_owner_executor_port_t
nx_spi_dma_owner_executor_port(nx_spi_dma_owner_executor_t* executor,
                               const nx_spi_dma_owner_binding_t* binding) {
    if (executor == NULL || binding == NULL || binding->port == NULL ||
        binding->port->ops == NULL || binding->port->context == NULL ||
        binding->contains == NULL || binding->port->ops->cancel == NULL ||
        binding->port->ops->service == NULL ||
        binding->port->ops->stop == NULL || executor->pending ||
        nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return (nx_owner_executor_port_t){0};
    }
    *executor = (nx_spi_dma_owner_executor_t){.binding = binding};
    nx_request_initialize(&executor->inner.base);
    return (nx_owner_executor_port_t){executor, start, service, cancel, true};
}
/** \brief Checked metadata spans never compare unrelated C pointers. */
static bool overlaps(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t first = (uintptr_t)a;
    uintptr_t second = (uintptr_t)b;
    return an > UINTPTR_MAX - first || bn > UINTPTR_MAX - second ||
           (first < second + bn && second < first + an);
}
/** \brief Constant-sized control list does not scan owner queue capacity. */
typedef struct {
    const void* address;
    size_t bytes;
} control_t;
/** \brief A validated word span never authorizes writes to control metadata. */
static bool payload_disjoint(const nx_spi_dma_transaction_request_t* view,
                             const control_t* control) {
    size_t width = view->word_bits / 8U;
    for (size_t i = 0U; i < view->count; ++i) {
        const nx_spi_dma_segment_t* segment = &view->segments[i];
        size_t tx = (segment->memory_flags & NX_SPI_MEMORY_TX_FIXED) != 0U
                        ? width
                        : segment->words * width;
        size_t rx = (segment->memory_flags & NX_SPI_MEMORY_RX_FIXED) != 0U
                        ? width
                        : segment->words * width;
        if (overlaps(segment->tx, tx, control->address, control->bytes) ||
            overlaps(segment->rx, rx, control->address, control->bytes)) {
            return false;
        }
    }
    return true;
}
nx_result_t nx_spi_dma_owner_submit(nx_bus_owner_t* owner,
                                    nx_request_t* request,
                                    nx_spi_dma_owner_operation_t* operation,
                                    const nx_wait_port_t* completion,
                                    nx_owner_ticket_t* ticket) {
    if (owner == NULL || request == NULL || operation == NULL ||
        ticket == NULL || owner->executor.start != start ||
        owner->executor.context == NULL || owner->slots == NULL ||
        owner->capacity == 0U ||
        owner->capacity > SIZE_MAX / sizeof(*owner->slots) ||
        operation->endpoint == NULL || operation->segments == NULL ||
        operation->count == 0U || operation->count > NX_SPI_MAX_SEGMENTS ||
        (operation->word_bits != 8U && operation->word_bits != 16U)) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    const nx_spi_dma_owner_executor_t* executor = owner->executor.context;
    const nx_spi_dma_owner_binding_t* binding = executor->binding;
    if (binding == NULL || binding->port == NULL ||
        binding->port->ops == NULL || operation->endpoint->ops == NULL) {
        return NX_ERROR_INVALID;
    }
    const control_t controls[] = {
        {owner, sizeof(*owner)},
        {owner->slots, owner->capacity * sizeof(*owner->slots)},
        {executor, sizeof(*executor)},
        {binding, sizeof(*binding)},
        {binding->port, sizeof(*binding->port)},
        {binding->port->ops, sizeof(*binding->port->ops)},
        {operation->endpoint, sizeof(*operation->endpoint)},
        {operation->endpoint->ops, sizeof(*operation->endpoint->ops)},
        {request, sizeof(*request)},
        {operation, sizeof(*operation)},
        {ticket, sizeof(*ticket)},
        {completion, completion == NULL ? 0U : sizeof(*completion)}};
    const size_t count = sizeof(controls) / sizeof(controls[0]);
    for (size_t i = 0U; i < count; ++i) {
        if (controls[i].bytes == 0U) {
            continue;
        }
        if (overlaps(operation->segments,
                     operation->count * sizeof(*operation->segments),
                     controls[i].address, controls[i].bytes)) {
            return NX_ERROR_INVALID;
        }
        if (i < 8U && (overlaps(request, sizeof(*request), controls[i].address,
                                controls[i].bytes) ||
                       overlaps(operation, sizeof(*operation),
                                controls[i].address, controls[i].bytes) ||
                       overlaps(ticket, sizeof(*ticket), controls[i].address,
                                controls[i].bytes))) {
            return NX_ERROR_INVALID;
        }
    }
    if (overlaps(request, sizeof(*request), operation, sizeof(*operation)) ||
        overlaps(ticket, sizeof(*ticket), request, sizeof(*request)) ||
        overlaps(ticket, sizeof(*ticket), operation, sizeof(*operation)) ||
        (completion != NULL && (overlaps(request, sizeof(*request), completion,
                                         sizeof(*completion)) ||
                                overlaps(ticket, sizeof(*ticket), completion,
                                         sizeof(*completion))))) {
        return NX_ERROR_INVALID;
    }
    const nx_spi_dma_transaction_request_t view = {
        .segments = operation->segments,
        .count = operation->count,
        .word_bits = operation->word_bits};
    nx_result_t result = nx_spi_dma_transaction_validate(&view);
    if (result != NX_SUCCESS) {
        return result;
    }
    for (size_t i = 0U; i < count; ++i) {
        if (controls[i].bytes != 0U && !payload_disjoint(&view, &controls[i])) {
            return NX_ERROR_INVALID;
        }
    }
    if (!nx_spi_dma_owner_contains(binding, operation->endpoint)) {
        return NX_ERROR_INVALID;
    }
    return nx_bus_owner_submit(owner, request, operation, completion, ticket);
}
