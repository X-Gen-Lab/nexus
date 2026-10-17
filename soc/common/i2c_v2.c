/**
 * \file            i2c_v2.c
 * \brief           Finite repeated-START V2 protocol with proved abort drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "i2c_v2.h"
#include "nexus/arch/arch.h"
#include "nexus/core/fault.h"
#define PE      (1U << 0)
#define RD      (1U << 10)
#define START   (1U << 13)
#define STOP    (1U << 14)
#define RELOAD  (1U << 24)
#define AUTOEND (1U << 25)
#define TXIS    (1U << 1)
#define RXNE    (1U << 2)
#define NACK    (1U << 4)
#define STOPF   (1U << 5)
#define TC      (1U << 6)
#define TCR     (1U << 7)
#define BERR    (1U << 8)
#define ARLO    (1U << 9)
#define OVR     (1U << 10)
#define BUSY    (1U << 15)
#define ERRORS  (NACK | BERR | ARLO | OVR)
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_i2c_v2_state_t* p) {
    return p != NULL && p->registers != NULL && p->reset != NULL &&
           p->reset_mask != 0U &&
           (p->reset_mask & (p->reset_mask - 1U)) == 0U && p->poll_limit != 0U;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool span(const void* a, size_t n) {
    return a != NULL && n != 0U && (uintptr_t)a <= UINTPTR_MAX - n;
}
static bool protected(const nx_i2c_v2_endpoint_state_t* e, const void* a,
                      size_t n) {
    return overlap(a, n, e, sizeof(*e)) ||
           overlap(a, n, e->port, sizeof(*e->port)) ||
           overlap(a, n, e->port->registers, sizeof(*e->port->registers)) ||
           overlap(a, n, (const void*)e->port->reset, sizeof(*e->port->reset));
}
/** \brief Both reset edges and disabled/idle readbacks precede loan release. */
static nx_result_t reset(nx_i2c_v2_state_t* p) {
    NX_I2C_V2_WRITE(p->reset, NX_I2C_V2_READ(p->reset) | p->reset_mask);
    nx_arch_dsb();
    if ((NX_I2C_V2_READ(p->reset) & p->reset_mask) == 0U)
        return NX_ERROR_IO;
    NX_I2C_V2_WRITE(p->reset, NX_I2C_V2_READ(p->reset) & ~p->reset_mask);
    nx_arch_dsb();
    if ((NX_I2C_V2_READ(p->reset) & p->reset_mask) != 0U ||
        NX_I2C_V2_READ(&p->registers->cr1) != 0U ||
        (NX_I2C_V2_READ(&p->registers->isr) & BUSY) != 0U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
static nx_result_t configure(nx_i2c_v2_state_t* p) {
    NX_I2C_V2_WRITE(&p->registers->cr2, 0U);
    NX_I2C_V2_WRITE(&p->registers->timing, p->timing);
    NX_I2C_V2_WRITE(&p->registers->icr, ERRORS | STOPF);
    NX_I2C_V2_WRITE(&p->registers->cr1, PE);
    nx_arch_dsb();
    return NX_I2C_V2_READ(&p->registers->cr1) == PE &&
                   NX_I2C_V2_READ(&p->registers->timing) == p->timing
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
nx_result_t nx_i2c_v2_initialize(nx_i2c_v2_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->initialized || p->retained || p->active ||
        NX_I2C_V2_READ(&p->registers->cr1) != 0U ||
        NX_I2C_V2_READ(&p->registers->cr2) != 0U ||
        NX_I2C_V2_READ(&p->registers->oar1) != 0U ||
        NX_I2C_V2_READ(&p->registers->oar2) != 0U ||
        NX_I2C_V2_READ(&p->registers->timeout) != 0U ||
        (NX_I2C_V2_READ(&p->registers->isr) & (BUSY | ERRORS | STOPF)) != 0U)
        return NX_ERROR_BUSY;
    p->retained = true;
    p->fault = false;
    nx_result_t result = configure(p);
    p->initialized = result == NX_SUCCESS;
    return result;
}
static nx_result_t wait(nx_i2c_v2_state_t* p, uint32_t mask, uint32_t expected,
                        nx_time_us_t deadline) {
    uint32_t remaining = p->poll_limit;
    do {
        NX_I2C_V2_POLL(p);
        uint32_t flags = NX_I2C_V2_READ(&p->registers->isr);
        if ((flags & ARLO) != 0U)
            return NX_ERROR_ARBITRATION;
        if ((flags & NACK) != 0U)
            return NX_ERROR_NACK;
        if ((flags & BERR) != 0U && p->ignore_master_berr) {
            NX_I2C_V2_WRITE(&p->registers->icr, BERR);
            flags &= ~BERR;
        }
        if ((flags & (BERR | OVR)) != 0U)
            return NX_ERROR_IO;
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
        if ((flags & mask) == expected)
            return NX_SUCCESS;
    } while (--remaining != 0U);
    return NX_ERROR_TIMEOUT;
}
static _Noreturn void retain_and_park(nx_i2c_v2_state_t* p) {
    p->fault = true;
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
static nx_result_t transaction_bounded(void* context,
                                       nx_i2c_message_t* messages, size_t count,
                                       nx_time_us_t deadline,
                                       size_t* transferred, bool extended) {
    const nx_i2c_v2_endpoint_state_t* e = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (e == NULL || !valid(e->port) || e->address > 127U ||
        !span(transferred, sizeof(*transferred)) || messages == NULL ||
        count == 0U || count > NX_I2C_MAX_MESSAGES ||
        !span(messages, count * sizeof(*messages)))
        return NX_ERROR_INVALID;
    nx_i2c_request_t check = {.messages = messages, .count = count};
    nx_result_t result = extended
                             ? nx_i2c_long_messages_validate(messages, count)
                             : nx_i2c_request_validate(&check);
    if (result != NX_SUCCESS)
        return result;
    size_t descriptor_bytes = count * sizeof(*messages);
    if (protected(e, messages, descriptor_bytes) ||
        protected(e, transferred, sizeof(*transferred)) ||
        overlap(messages, descriptor_bytes, transferred, sizeof(*transferred)))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < count; ++i) {
        if (protected(e, messages[i].data, messages[i].length) ||
            overlap(messages[i].data, messages[i].length, transferred,
                    sizeof(*transferred)))
            return NX_ERROR_INVALID;
    }
    *transferred = 0U;
    nx_i2c_v2_state_t* p = e->port;
    if (!p->initialized || p->fault)
        return NX_ERROR_STATE;
    if (p->active || (NX_I2C_V2_READ(&p->registers->isr) & BUSY) != 0U)
        return NX_ERROR_BUSY;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    p->active = true;
    for (size_t m = 0U; m < count && result == NX_SUCCESS; ++m) {
        size_t position = 0U;
        do {
            size_t chunk = messages[m].length - position;
            if (p->receive_reload_one && messages[m].read &&
                messages[m].length > 255U)
                chunk = 1U;
            else if (chunk > 255U)
                chunk = 255U;
            bool last_chunk = position + chunk == messages[m].length;
            uint32_t control =
                ((uint32_t)e->address << 1U) | ((uint32_t)chunk << 16U) |
                (messages[m].read ? RD : 0U) | (position == 0U ? START : 0U) |
                (!last_chunk ? RELOAD : (m + 1U == count ? AUTOEND : 0U));
            NX_I2C_V2_WRITE(&p->registers->cr2, control);
            for (size_t i = 0U; i < chunk && result == NX_SUCCESS; ++i) {
                result = wait(p, messages[m].read ? RXNE : TXIS,
                              messages[m].read ? RXNE : TXIS, deadline);
                if (result != NX_SUCCESS)
                    break;
                if (messages[m].read)
                    messages[m].data[position] =
                        (uint8_t)NX_I2C_V2_READ(&p->registers->rxdr);
                else
                    NX_I2C_V2_WRITE(&p->registers->txdr,
                                    messages[m].data[position]);
                ++position;
                ++*transferred;
            }
            if (result == NX_SUCCESS)
                result =
                    wait(p, !last_chunk ? TCR : (m + 1U == count ? STOPF : TC),
                         !last_chunk ? TCR : (m + 1U == count ? STOPF : TC),
                         deadline);
        } while (result == NX_SUCCESS && position < messages[m].length);
    }
    if (result == NX_SUCCESS)
        result = wait(p, BUSY, 0U, deadline);
    if (result != NX_SUCCESS) {
        /* Do not issue STOP after ARLO: another master owns the bus. Local
         * controller reset withdraws all memory accesses without bus recovery.
         */
        p->fault = true;
        if (reset(p) != NX_SUCCESS)
            retain_and_park(p);
    } else {
        NX_I2C_V2_WRITE(&p->registers->icr, STOPF);
        NX_I2C_V2_WRITE(&p->registers->cr2, 0U);
        nx_arch_dsb();
        /* The bus is already drained, so caller buffers may return. Reuse
         * still requires independent STOPCF and control-clear proof. */
        if ((NX_I2C_V2_READ(&p->registers->isr) & (STOPF | BUSY)) != 0U ||
            NX_I2C_V2_READ(&p->registers->cr2) != 0U) {
            p->fault = true;
            result = NX_ERROR_IO;
        }
    }
    p->active = false;
    return result;
}
/** \brief The base face retains its existing finite256-byte admission bound.
 */
static nx_result_t transaction(void* context, nx_i2c_message_t* messages,
                               size_t count, nx_time_us_t deadline,
                               size_t* transferred) {
    return transaction_bounded(context, messages, count, deadline, transferred,
                               false);
}
/** \brief Reload only NBYTES at TCR; each message retains its one address
 * phase.
 */
static nx_result_t long_transaction(void* context, nx_i2c_message_t* messages,
                                    size_t count, nx_time_us_t deadline,
                                    size_t* transferred) {
    return transaction_bounded(context, messages, count, deadline, transferred,
                               true);
}
static nx_result_t recover(void* context) {
    nx_i2c_v2_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !p->retained)
        return NX_ERROR_STATE;
    if (p->active)
        return NX_ERROR_BUSY;
    nx_result_t result = reset(p);
    if (result == NX_SUCCESS)
        result = configure(p);
    if (result == NX_SUCCESS &&
        (NX_I2C_V2_READ(&p->registers->isr) & BUSY) != 0U)
        result = NX_ERROR_BUSY;
    p->fault = result != NX_SUCCESS;
    p->initialized = result == NX_SUCCESS;
    return result;
}
static nx_result_t stop(void* context) {
    nx_i2c_v2_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->active)
        return NX_ERROR_BUSY;
    if (!p->retained)
        return NX_SUCCESS;
    nx_result_t result = reset(p);
    if (result == NX_SUCCESS) {
        p->initialized = false;
        p->retained = false;
        p->fault = false;
    }
    return result;
}
static bool on_port(const void* context, const nx_i2c_port_t* port) {
    const nx_i2c_v2_endpoint_state_t* e = context;
    return e != NULL && port != NULL && port->ops == &nx_i2c_v2_ops &&
           port->context == e->port;
}
const nx_i2c_ops_t nx_i2c_v2_ops = {.recover = recover, .stop = stop};
const nx_i2c_endpoint_ops_t nx_i2c_v2_endpoint_ops = {
    .transaction = transaction, .on_port = on_port};
const nx_i2c_long_ops_t nx_i2c_v2_long_endpoint_ops = {.transaction =
                                                           long_transaction};
