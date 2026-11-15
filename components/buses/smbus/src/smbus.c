/**
 * \file            smbus.c
 * \brief           Packet PEC adapter retaining the underlying I2C wire loan
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/smbus.h"
#include "nexus/arch/arch.h"
#include <string.h>

/** \brief Check an entire object before dereferencing its typed fields. */
static bool span(const void* data, size_t bytes, size_t alignment) {
    return data != NULL && (uintptr_t)data % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)data;
}
/** \brief Every compared extent has already passed nonwrapping admission. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Reject metadata aliases before retaining the immutable binding. */
static bool binding_valid(const nx_smbus_t* owner,
                          const nx_smbus_binding_t* binding) {
    return span(binding, sizeof(*binding), _Alignof(nx_smbus_binding_t)) &&
           !overlap(owner, sizeof(*owner), binding, sizeof(*binding)) &&
           binding->address >= 8U && binding->address <= 119U &&
           span(binding->endpoint, sizeof(*binding->endpoint),
                _Alignof(nx_i2c_endpoint_t)) &&
           span(binding->controller, sizeof(*binding->controller),
                _Alignof(nx_i2c_port_t)) &&
           span(binding->endpoint->ops, sizeof(*binding->endpoint->ops),
                _Alignof(nx_i2c_endpoint_ops_t)) &&
           span(binding->controller->ops, sizeof(*binding->controller->ops),
                _Alignof(nx_i2c_ops_t)) &&
           binding->endpoint->context != NULL &&
           binding->controller->context != NULL &&
           !overlap(owner, sizeof(*owner), binding->endpoint,
                    sizeof(*binding->endpoint)) &&
           !overlap(owner, sizeof(*owner), binding->controller,
                    sizeof(*binding->controller)) &&
           !overlap(owner, sizeof(*owner), binding->endpoint->ops,
                    sizeof(*binding->endpoint->ops)) &&
           !overlap(owner, sizeof(*owner), binding->controller->ops,
                    sizeof(*binding->controller->ops)) &&
           !overlap(owner, sizeof(*owner), binding->endpoint->context, 1U) &&
           !overlap(owner, sizeof(*owner), binding->controller->context, 1U);
}
/** \brief Enclose every external callback with the serialized reentry guard. */
static nx_result_t control(nx_smbus_t* owner) {
    if (!span(owner, sizeof(*owner), _Alignof(nx_smbus_t)))
        return NX_ERROR_INVALID;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (owner->in_call)
        return NX_ERROR_BUSY;
    return owner->initialized ? NX_SUCCESS : NX_ERROR_STATE;
}
nx_result_t nx_smbus_initialize(nx_smbus_t* owner,
                                const nx_smbus_binding_t* binding) {
    if (!span(owner, sizeof(*owner), _Alignof(nx_smbus_t)))
        return NX_ERROR_INVALID;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (owner->in_call || owner->initialized || owner->active != NULL)
        return NX_ERROR_BUSY;
    if (!binding_valid(owner, binding))
        return NX_ERROR_INVALID;
    if (binding->endpoint->ops->submit == NULL ||
        binding->controller->ops->cancel == NULL ||
        binding->controller->ops->service == NULL)
        return NX_ERROR_UNSUPPORTED;
    if (owner->generation == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    owner->in_call = true;
    bool matched =
        nx_i2c_endpoint_on_port(binding->endpoint, binding->controller);
    owner->in_call = false;
    if (!matched)
        return NX_ERROR_INVALID;
    owner->binding = binding;
    ++owner->generation;
    owner->initialized = true;
    owner->closing = false;
    nx_request_initialize(&owner->transfer.base);
    return NX_SUCCESS;
}
/** \brief Known public metadata is never writable or transmitted packet data.
 */
static bool outside_control(const nx_smbus_t* owner, const void* data,
                            size_t bytes) {
    const nx_smbus_binding_t* binding = owner->binding;
    return span(data, bytes, 1U) &&
           !overlap(data, bytes, owner, sizeof(*owner)) &&
           !overlap(data, bytes, binding, sizeof(*binding)) &&
           !overlap(data, bytes, binding->endpoint,
                    sizeof(*binding->endpoint)) &&
           !overlap(data, bytes, binding->controller,
                    sizeof(*binding->controller)) &&
           !overlap(data, bytes, binding->endpoint->ops,
                    sizeof(*binding->endpoint->ops)) &&
           !overlap(data, bytes, binding->controller->ops,
                    sizeof(*binding->controller->ops)) &&
           !overlap(data, bytes, binding->endpoint->context, 1U) &&
           !overlap(data, bytes, binding->controller->context, 1U);
}
/** \brief Validate packet grammar and every reserved byte before admission. */
static bool packet(const nx_smbus_t* owner, const nx_smbus_request_t* request,
                   nx_smbus_tx_t tx, nx_smbus_rx_t rx) {
    if (!span(request, sizeof(*request), _Alignof(nx_smbus_request_t)) ||
        !outside_control(owner, request, sizeof(*request)) ||
        (tx.payload_bytes == 0U && rx.payload_bytes == 0U) ||
        tx.payload_bytes > NX_SMBUS_MAX_PAYLOAD_BYTES ||
        rx.payload_bytes > NX_SMBUS_MAX_PAYLOAD_BYTES ||
        (tx.payload_bytes == 0U && (tx.data != NULL || tx.pec != NULL)) ||
        (rx.payload_bytes == 0U && (rx.data != NULL || rx.capacity != 0U)))
        return false;
    size_t tx_wire = tx.payload_bytes;
    if (tx.payload_bytes != 0U && rx.payload_bytes == 0U) {
        if (!span(tx.data, tx.payload_bytes + 1U, 1U) ||
            (uintptr_t)tx.pec != (uintptr_t)tx.data + tx.payload_bytes)
            return false;
        ++tx_wire;
    } else if (tx.pec != NULL) {
        return false;
    }
    if (tx_wire != 0U && (!outside_control(owner, tx.data, tx_wire) ||
                          overlap(tx.data, tx_wire, request, sizeof(*request))))
        return false;
    if (rx.payload_bytes != 0U &&
        (rx.capacity < rx.payload_bytes + 1U ||
         !outside_control(owner, rx.data, rx.capacity) ||
         overlap(rx.data, rx.capacity, request, sizeof(*request)) ||
         (tx_wire != 0U && overlap(rx.data, rx.capacity, tx.data, tx_wire))))
        return false;
    return true;
}
nx_result_t nx_smbus_prepare(nx_smbus_t* owner, nx_smbus_request_t* request,
                             nx_smbus_tx_t tx, nx_smbus_rx_t rx,
                             nx_time_us_t deadline) {
    nx_result_t result = control(owner);
    if (result != NX_SUCCESS)
        return result;
    if (owner->closing)
        return NX_ERROR_STATE;
    if (owner->active == request)
        return NX_ERROR_BUSY;
    if (deadline == NX_DEADLINE_NEVER || !packet(owner, request, tx, rx))
        return NX_ERROR_INVALID;
    result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->tx = tx;
        request->rx = rx;
        request->owner = owner;
        request->generation = owner->generation;
        request->pec = NX_SMBUS_PEC_NOT_CHECKED;
    }
    return result;
}
/** \brief SMBus CRC8, polynomial07h, zero seed, no reflection or final XOR. */
static uint8_t byte_pec(uint8_t pec, uint8_t byte) {
    pec ^= byte;
    for (unsigned i = 0U; i < 8U; ++i)
        pec =
            (uint8_t)(((uint32_t)pec << 1U) ^ ((pec & 0x80U) != 0U ? 7U : 0U));
    return pec;
}
/** \brief A packet PEC includes both direction addresses and every payload. */
static uint8_t tx_pec(const nx_smbus_t* owner,
                      const nx_smbus_request_t* request) {
    uint8_t pec = 0U;
    if (request->tx.payload_bytes != 0U) {
        pec = byte_pec(pec, (uint8_t)(owner->binding->address << 1U));
        for (size_t i = 0U; i < request->tx.payload_bytes; ++i)
            pec = byte_pec(pec, request->tx.data[i]);
    }
    return pec;
}
/** \brief Return only retired transport references, then final-publish outer.
 */
static nx_result_t finish(nx_smbus_t* owner, nx_result_t result) {
    nx_smbus_request_t* request = owner->active;
    size_t wire_bytes =
        request->tx.payload_bytes + request->rx.payload_bytes + 1U;
    if (result == NX_SUCCESS && owner->transfer.base.transferred != wire_bytes)
        result = NX_ERROR_IO;
    if (result == NX_SUCCESS && request->rx.payload_bytes != 0U) {
        uint8_t pec = byte_pec(tx_pec(owner, request),
                               (uint8_t)((owner->binding->address << 1U) | 1U));
        for (size_t i = 0U; i < request->rx.payload_bytes; ++i)
            pec = byte_pec(pec, request->rx.data[i]);
        bool valid = pec == request->rx.data[request->rx.payload_bytes];
        request->pec = valid ? NX_SMBUS_PEC_VALID : NX_SMBUS_PEC_ERROR;
        if (!valid)
            result = NX_ERROR_IO;
    }
    size_t transferred = result == NX_SUCCESS ? request->tx.payload_bytes +
                                                    request->rx.payload_bytes
                                              : 0U;
    owner->active = NULL;
    owner->transfer.messages = NULL;
    owner->transfer.count = 0U;
    memset(owner->messages, 0, sizeof(owner->messages));
    nx_request_settle(&request->base, result, transferred);
    return result;
}
/** \brief Observe the inner acquire predicate; cancel never substitutes for it.
 */
static nx_result_t observe(nx_smbus_t* owner) {
    nx_request_state_t inner = nx_request_state(&owner->transfer.base);
    nx_request_state_t outer = nx_request_state(&owner->active->base);
    if (inner == NX_REQUEST_SETTLED)
        return finish(owner, owner->transfer.base.result);
    if (inner == NX_REQUEST_ACTIVE && outer == NX_REQUEST_QUEUED)
        (void)nx_request_transition(&owner->active->base, NX_REQUEST_ACTIVE);
    if (inner == NX_REQUEST_DRAINING || inner == NX_REQUEST_QUARANTINED ||
        inner == NX_REQUEST_READY) {
        if (outer == NX_REQUEST_QUEUED || outer == NX_REQUEST_ACTIVE)
            (void)nx_request_transition(&owner->active->base,
                                        NX_REQUEST_DRAINING);
        if (inner != NX_REQUEST_DRAINING && outer != NX_REQUEST_QUARANTINED)
            (void)nx_request_transition(&owner->active->base,
                                        NX_REQUEST_QUARANTINED);
    }
    return NX_ERROR_BUSY;
}
nx_result_t nx_smbus_submit(nx_smbus_t* owner, nx_smbus_request_t* request) {
    nx_result_t result = control(owner);
    if (result != NX_SUCCESS)
        return result;
    if (owner->closing)
        return NX_ERROR_STATE;
    if (owner->active != NULL)
        return NX_ERROR_BUSY;
    if (!span(request, sizeof(*request), _Alignof(nx_smbus_request_t)) ||
        !outside_control(owner, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    if (request->owner != owner || request->generation != owner->generation ||
        nx_request_state(&request->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    if (!packet(owner, request, request->tx, request->rx) ||
        request->base.deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    owner->in_call = true;
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        owner->in_call = false;
        return NX_ERROR_TIMEOUT;
    }
    result = nx_request_admit(&request->base, NX_REQUEST_QUEUED);
    if (result != NX_SUCCESS) {
        owner->in_call = false;
        return result;
    }
    owner->active = request;
    request->pec = NX_SMBUS_PEC_NOT_CHECKED;
    size_t count = 0U;
    if (request->tx.payload_bytes != 0U) {
        size_t bytes = request->tx.payload_bytes;
        if (request->rx.payload_bytes == 0U) {
            *request->tx.pec = tx_pec(owner, request);
            ++bytes;
        }
        owner->messages[count++] =
            nx_i2c_message_write(request->tx.data, bytes);
    }
    if (request->rx.payload_bytes != 0U)
        owner->messages[count++] = nx_i2c_message_read(
            request->rx.data, request->rx.payload_bytes + 1U);
    nx_request_initialize(&owner->transfer.base);
    result = nx_i2c_request_prepare(&owner->transfer, owner->messages, count,
                                    request->base.deadline);
    if (result == NX_SUCCESS)
        result =
            nx_i2c_endpoint_submit(owner->binding->endpoint, &owner->transfer);
    if (result != NX_SUCCESS)
        (void)finish(owner, result);
    else
        (void)observe(owner);
    owner->in_call = false;
    return NX_SUCCESS;
}
nx_result_t nx_smbus_service(nx_smbus_t* owner) {
    nx_result_t result = control(owner);
    if (result != NX_SUCCESS || owner->active == NULL)
        return result;
    owner->in_call = true;
    nx_i2c_port_service(owner->binding->controller);
    result = observe(owner);
    owner->in_call = false;
    return result;
}
/** \brief An already retired I2C packet wins a later cancellation request. */
static nx_result_t cancel_active(nx_smbus_t* owner) {
    if (nx_request_state(&owner->transfer.base) == NX_REQUEST_SETTLED) {
        (void)observe(owner);
        return NX_SUCCESS;
    }
    nx_result_t result =
        nx_i2c_port_cancel(owner->binding->controller, &owner->transfer);
    if (result == NX_SUCCESS && owner->active != NULL) {
        nx_request_state_t state = nx_request_state(&owner->active->base);
        if (state == NX_REQUEST_ACTIVE || state == NX_REQUEST_QUEUED)
            (void)nx_request_transition(&owner->active->base,
                                        NX_REQUEST_DRAINING);
        (void)observe(owner);
    }
    return result;
}
nx_result_t nx_smbus_cancel(nx_smbus_t* owner, nx_smbus_request_t* request) {
    nx_result_t result = control(owner);
    if (result != NX_SUCCESS)
        return result;
    if (!span(request, sizeof(*request), _Alignof(nx_smbus_request_t)) ||
        !outside_control(owner, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    if (request->owner != owner || request->generation != owner->generation)
        return NX_ERROR_STATE;
    if (nx_request_state(&request->base) == NX_REQUEST_SETTLED)
        return NX_SUCCESS;
    if (owner->active != request)
        return NX_ERROR_STATE;
    owner->in_call = true;
    result = cancel_active(owner);
    owner->in_call = false;
    return result;
}
nx_result_t nx_smbus_stop(nx_smbus_t* owner) {
    nx_result_t result = control(owner);
    if (result == NX_ERROR_STATE && owner->active == NULL &&
        owner->binding == NULL)
        return NX_SUCCESS;
    if (result != NX_SUCCESS)
        return result;
    owner->in_call = true;
    owner->closing = true;
    if (owner->active != NULL) {
        (void)cancel_active(owner);
        if (owner->active != NULL) {
            nx_i2c_port_service(owner->binding->controller);
            (void)observe(owner);
        }
    }
    result = owner->active != NULL ? NX_ERROR_BUSY : NX_SUCCESS;
    if (result == NX_SUCCESS) {
        owner->binding = NULL;
        owner->initialized = false;
    }
    owner->in_call = false;
    return result;
}
