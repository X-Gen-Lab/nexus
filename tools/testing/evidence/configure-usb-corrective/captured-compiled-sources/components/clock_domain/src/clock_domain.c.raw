/**
 * \file            clock_domain.c
 * \brief           Exact consumer identity and closed epoch drain barrier
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "clock_domain_impl.h"
#include "nexus/arch/arch.h"

/** \brief Validate arithmetic/alignment before interpreting caller storage. */
static bool range_valid(const void* span, size_t bytes, size_t alignment) {
    uintptr_t begin = (uintptr_t)span;
    return span != NULL && bytes != 0U && begin % alignment == 0U &&
           bytes <= UINTPTR_MAX - begin;
}

/** \brief Both spans must first pass the checked arithmetic contract. */
static bool overlap(const void* first, size_t first_bytes, const void* second,
                    size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return a < b + second_bytes && b < a + first_bytes;
}

/** \brief Exclude user, NMI and fault paths from the configurable IRQ guard. */
static bool context_valid(bool task) {
    if (!nx_arch_is_privileged()) {
        return false;
    }
    uint32_t exception = nx_arch_exception_number();
    return exception == 0U || (!task && exception >= 14U);
}

/** \brief Zero capacity and unused/corrupt state never publish a consumer. */
static bool live(const nx_clock_domain_t* domain) {
    return range_valid(domain, sizeof(*domain), _Alignof(nx_clock_domain_t)) &&
           domain->initialized && domain->capacity != 0U &&
           domain->count <= domain->capacity && domain->epoch != 0U;
}

bool nx_clock_domain_span_valid(const nx_clock_domain_t* domain,
                                const void* span, size_t bytes) {
    if (!live(domain) || !range_valid(span, bytes, 1U) ||
        overlap(domain, sizeof(*domain), span, bytes)) {
        return false;
    }
    const nx_clock_lease_t* token = domain->head;
    for (size_t checked = 0U; checked < domain->count; ++checked) {
        if (!range_valid(token, sizeof(*token), _Alignof(nx_clock_lease_t)) ||
            token->owner != domain || token->epoch != domain->epoch ||
            overlap(token, sizeof(*token), span, bytes)) {
            return false;
        }
        token = token->next;
    }
    return token == NULL;
}

nx_result_t nx_clock_domain_construct(nx_clock_domain_t* domain,
                                      size_t capacity) {
    if (!range_valid(domain, sizeof(*domain), _Alignof(nx_clock_domain_t)) ||
        capacity == 0U) {
        return NX_ERROR_INVALID;
    }
    if (!context_valid(true)) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (!domain->initialized && domain->head == NULL && domain->epoch == 0U &&
        domain->count == 0U && domain->capacity == 0U && !domain->closing) {
        domain->capacity = capacity;
        domain->epoch = 1U;
        domain->initialized = true;
        result = NX_SUCCESS;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Exact retained pointer, never a copied token with matching fields. */
static bool retained_identity(const nx_clock_domain_t* domain,
                              const nx_clock_lease_t* lease) {
    const nx_clock_lease_t* token = domain->head;
    for (size_t checked = 0U; checked < domain->count; ++checked) {
        if (!range_valid(token, sizeof(*token), _Alignof(nx_clock_lease_t)) ||
            token->owner != domain || token->epoch != domain->epoch) {
            return false;
        }
        if (token == lease) {
            return true;
        }
        token = token->next;
    }
    return false;
}

nx_result_t nx_clock_domain_acquire(nx_clock_domain_t* domain,
                                    nx_clock_lease_t* lease) {
    if (!range_valid(domain, sizeof(*domain), _Alignof(nx_clock_domain_t)) ||
        !range_valid(lease, sizeof(*lease), _Alignof(nx_clock_lease_t))) {
        return NX_ERROR_INVALID;
    }
    if (!context_valid(false)) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (!live(domain)) {
        result = NX_ERROR_STATE;
    } else if (!nx_clock_domain_span_valid(domain, lease, sizeof(*lease))) {
        result = retained_identity(domain, lease) ? NX_ERROR_STATE
                                                  : NX_ERROR_INVALID;
    } else if (lease->owner != NULL || lease->next != NULL ||
               lease->epoch != 0U) {
        result = NX_ERROR_STATE;
    } else if (domain->closing) {
        result = NX_ERROR_BUSY;
    } else if (domain->count == domain->capacity) {
        result = NX_ERROR_EXHAUSTED;
    } else {
        lease->owner = domain;
        lease->epoch = domain->epoch;
        lease->next = domain->head;
        domain->head = lease;
        ++domain->count;
        result = NX_SUCCESS;
    }
    nx_arch_irq_restore(saved);
    return result;
}

nx_result_t nx_clock_domain_release(nx_clock_domain_t* domain,
                                    nx_clock_lease_t* lease) {
    if (!range_valid(domain, sizeof(*domain), _Alignof(nx_clock_domain_t)) ||
        !range_valid(lease, sizeof(*lease), _Alignof(nx_clock_lease_t)) ||
        overlap(domain, sizeof(*domain), lease, sizeof(*lease))) {
        return NX_ERROR_INVALID;
    }
    if (!context_valid(false)) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (live(domain)) {
        nx_clock_lease_t** link = &domain->head;
        for (size_t checked = 0U; checked < domain->count; ++checked) {
            nx_clock_lease_t* token = *link;
            if (!range_valid(token, sizeof(*token),
                             _Alignof(nx_clock_lease_t)) ||
                token->owner != domain || token->epoch != domain->epoch) {
                break;
            }
            if (token == lease) {
                *link = token->next;
                --domain->count;
                /* Detach every provider link before the token's final store.
                 * Caller drain evidence is independent of this metadata. */
                lease->next = NULL;
                lease->epoch = 0U;
                lease->owner = NULL;
                result = NX_SUCCESS;
                break;
            }
            link = &token->next;
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}

nx_result_t nx_clock_domain_close(nx_clock_domain_t* domain,
                                  nx_clock_barrier_t* control) {
    if (!range_valid(domain, sizeof(*domain), _Alignof(nx_clock_domain_t)) ||
        !range_valid(control, sizeof(*control), _Alignof(nx_clock_barrier_t))) {
        return NX_ERROR_INVALID;
    }
    if (!context_valid(true)) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (live(domain)) {
        if (!nx_clock_domain_span_valid(domain, control, sizeof(*control))) {
            result = NX_ERROR_INVALID;
        } else {
            domain->closing = true;
            control->domain = domain;
            control->epoch = domain->epoch;
            result = NX_SUCCESS;
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Current closed identity only; zero count must also mean empty list.
 */
static nx_result_t quiescent(nx_clock_barrier_t control) {
    const nx_clock_domain_t* domain = control.domain;
    if (!live(domain) || !domain->closing || domain->epoch != control.epoch) {
        return NX_ERROR_STATE;
    }
    if (domain->count != 0U) {
        return NX_ERROR_BUSY;
    }
    return domain->head == NULL ? NX_SUCCESS : NX_ERROR_STATE;
}

nx_result_t nx_clock_domain_quiescent(nx_clock_barrier_t control) {
    if (!range_valid(control.domain, sizeof(*control.domain),
                     _Alignof(nx_clock_domain_t))) {
        return NX_ERROR_INVALID;
    }
    if (!context_valid(true)) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = quiescent(control);
    nx_arch_irq_restore(saved);
    return result;
}

nx_result_t nx_clock_domain_reopen(nx_clock_barrier_t control) {
    if (!range_valid(control.domain, sizeof(*control.domain),
                     _Alignof(nx_clock_domain_t))) {
        return NX_ERROR_INVALID;
    }
    if (!context_valid(true)) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = quiescent(control);
    if (result == NX_SUCCESS) {
        if (control.domain->epoch == UINT64_MAX) {
            result = NX_ERROR_EXHAUSTED;
        } else {
            ++control.domain->epoch;
            control.domain->closing = false;
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}
